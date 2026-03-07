// /lgfx_port/lgfx_port.c
//
// AtomVM port driver entry point for the LovyanGFX port.
//
// Responsibilities in this file:
// - Per-context port creation and teardown
// - Atom table initialization for this port (ops + common atoms)
// - Op metadata registry + dispatch lookup (generated from ops.def)
// - Mailbox message handling on the port thread
// - Request decode -> metadata validation -> dispatch -> reply flow
//
// Non-responsibilities:
// - Device calls (handled by lgfx_worker_*.c / src/lgfx_device*)
// - AtomVM term decoding details (handled by proto_term.c)
// - Reply encoding helpers (handled by proto_term.c)

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "context.h"
#include "defaultatoms.h" // ATOM_STR
#include "globalcontext.h"
#include "mailbox.h"
#include "memory.h" // memory_ensure_free / MEMORY_GC_OK
#include "port.h" // port_parse_gen_message / port_send_reply
#include "portnifloader.h"

#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/worker.h"

#ifndef LGFX_PORT_DEBUG
#define LGFX_PORT_DEBUG 0
#endif
#if (LGFX_PORT_DEBUG != 0) && (LGFX_PORT_DEBUG != 1)
#error "LGFX_PORT_DEBUG must be 0 or 1"
#endif

// -----------------------------------------------------------------------------
// Atom initialization
// -----------------------------------------------------------------------------

/*
 * Canonical op list: lgfx_port/include_internal/lgfx_port/ops.def
 * Protocol contract: docs/LGFX_PORT_PROTOCOL.md
 */
void lgfx_atoms_init(GlobalContext *global, lgfx_atoms_t *atoms)
{
    // Common reply atoms
    atoms->ok = globalcontext_make_atom(global, ATOM_STR("\x02", "ok"));
    atoms->error = globalcontext_make_atom(global, ATOM_STR("\x05", "error"));

    // Namespace / misc
    atoms->lgfx = globalcontext_make_atom(global, ATOM_STR("\x04", "lgfx"));

    // Common values
    atoms->pong = globalcontext_make_atom(global, ATOM_STR("\x04", "pong"));
    atoms->true_ = globalcontext_make_atom(global, ATOM_STR("\x04", "true"));
    atoms->false_ = globalcontext_make_atom(global, ATOM_STR("\x05", "false"));

    // Error atoms
    atoms->bad_proto = globalcontext_make_atom(global, ATOM_STR("\x09", "bad_proto"));
    atoms->bad_op = globalcontext_make_atom(global, ATOM_STR("\x06", "bad_op"));
    atoms->bad_flags = globalcontext_make_atom(global, ATOM_STR("\x09", "bad_flags"));
    atoms->bad_args = globalcontext_make_atom(global, ATOM_STR("\x08", "bad_args"));
    atoms->bad_target = globalcontext_make_atom(global, ATOM_STR("\x0A", "bad_target"));
    atoms->not_writing = globalcontext_make_atom(global, ATOM_STR("\x0B", "not_writing"));
    atoms->no_memory = globalcontext_make_atom(global, ATOM_STR("\x09", "no_memory"));
    atoms->internal = globalcontext_make_atom(global, ATOM_STR("\x08", "internal"));
    atoms->unsupported = globalcontext_make_atom(global, ATOM_STR("\x0B", "unsupported"));
    atoms->not_initialized = globalcontext_make_atom(global, ATOM_STR("\x0F", "not_initialized"));

    // Capability / info atoms
    atoms->caps = globalcontext_make_atom(global, ATOM_STR("\x04", "caps"));
    atoms->last_error = globalcontext_make_atom(global, ATOM_STR("\x0A", "last_error"));
    atoms->none = globalcontext_make_atom(global, ATOM_STR("\x04", "none"));

    // Op atoms (generated from lgfx_port/include_internal/lgfx_port/ops.def)
#define X(op, handler, atom_str, ...) atoms->op = globalcontext_make_atom(global, (atom_str));
#include "lgfx_port/ops.def"
#undef X
}

// -----------------------------------------------------------------------------
// Op metadata registry + dispatch lookup
// -----------------------------------------------------------------------------

_Static_assert(CHAR_BIT == 8, "This code assumes 8-bit bytes");
_Static_assert(LGFX_OP_TARGET_BAD_TARGET == 0, "LGFX_OP_TARGET_BAD_TARGET must be 0");
_Static_assert(LGFX_OP_TARGET_UNSUPPORTED == 1, "LGFX_OP_TARGET_UNSUPPORTED must be 1");
_Static_assert(LGFX_OP_TARGET_ANY == 2, "LGFX_OP_TARGET_ANY must be 2");
_Static_assert(LGFX_OP_TARGET_SPRITE_ONLY == 3, "LGFX_OP_TARGET_SPRITE_ONLY must be 3");
_Static_assert(LGFX_OP_STATE_ANY == 0, "LGFX_OP_STATE_ANY must be 0");
_Static_assert(LGFX_OP_STATE_REQUIRES_INIT == 1, "LGFX_OP_STATE_REQUIRES_INIT must be 1");

#if LGFX_PORT_DEBUG
_Static_assert(sizeof(lgfx_op_meta_t) == 12, "lgfx_op_meta_t must stay 12 bytes");
_Static_assert(offsetof(lgfx_op_meta_t, allowed_flags_mask) == 0, "allowed_flags_mask offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, feature_cap_bit) == 4, "feature_cap_bit offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, min_arity) == 8, "min_arity offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, max_arity) == 9, "max_arity offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, target_policy) == 10, "target_policy offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, state_policy) == 11, "state_policy offset drift");
#endif

#define X(op_name, _handler_fn, _atom_str, min_arity_v, max_arity_v, allowed_flags_mask_v, target_policy_v, state_policy_v, feature_cap_bit_v)                                                                              \
    _Static_assert((min_arity_v) >= 0 && (min_arity_v) <= UINT8_MAX, #op_name " min_arity out of uint8_t range");                                                                                                           \
    _Static_assert((max_arity_v) >= 0 && (max_arity_v) <= UINT8_MAX, #op_name " max_arity out of uint8_t range");                                                                                                           \
    _Static_assert((min_arity_v) <= (max_arity_v), #op_name " min_arity must be <= max_arity");                                                                                                                             \
    _Static_assert(((uint32_t) (allowed_flags_mask_v)) == (allowed_flags_mask_v), #op_name " flags mask out of uint32_t range");                                                                                            \
    _Static_assert((target_policy_v) >= 0 && (target_policy_v) <= UINT8_MAX, #op_name " target_policy out of uint8_t range");                                                                                               \
    _Static_assert(((target_policy_v) == LGFX_OP_TARGET_BAD_TARGET) || ((target_policy_v) == LGFX_OP_TARGET_UNSUPPORTED) || ((target_policy_v) == LGFX_OP_TARGET_ANY) || ((target_policy_v) == LGFX_OP_TARGET_SPRITE_ONLY), \
        #op_name " invalid target_policy value");                                                                                                                                                                           \
    _Static_assert((state_policy_v) >= 0 && (state_policy_v) <= UINT8_MAX, #op_name " state_policy out of uint8_t range");                                                                                                  \
    _Static_assert(((state_policy_v) == LGFX_OP_STATE_ANY) || ((state_policy_v) == LGFX_OP_STATE_REQUIRES_INIT),                                                                                                            \
        #op_name " invalid state_policy value");                                                                                                                                                                            \
    _Static_assert(((uint32_t) (feature_cap_bit_v)) == (feature_cap_bit_v), #op_name " feature_cap_bit out of uint32_t range");                                                                                             \
    _Static_assert((((uint32_t) (feature_cap_bit_v)) & ~((uint32_t) LGFX_CAP_KNOWN_MASK)) == 0u,                                                                                                                            \
        #op_name " feature_cap_bit has unknown bits");
#include "lgfx_port/ops.def"
#undef X

#define X(op_name, _handler_fn, _atom_str, min_arity_v, max_arity_v, allowed_flags_mask_v, target_policy_v, state_policy_v, feature_cap_bit_v) \
    [LGFX_OP_##op_name] = {                                                                                                                    \
        .allowed_flags_mask = (uint32_t) (allowed_flags_mask_v),                                                                               \
        .feature_cap_bit = (uint32_t) (feature_cap_bit_v),                                                                                     \
        .min_arity = (uint8_t) (min_arity_v),                                                                                                  \
        .max_arity = (uint8_t) (max_arity_v),                                                                                                  \
        .target_policy = (uint8_t) (target_policy_v),                                                                                          \
        .state_policy = (uint8_t) (state_policy_v),                                                                                            \
    },

static const lgfx_op_meta_t s_op_meta[LGFX_OP_COUNT] = {
#include "lgfx_port/ops.def"
};
#undef X

#if LGFX_PORT_DEBUG
#define X(op_name, _handler_fn, _atom_str, ...) [LGFX_OP_##op_name] = #op_name,

static const char *const s_op_names[LGFX_OP_COUNT] = {
#include "lgfx_port/ops.def"
};
#undef X
#endif

#define X(op_name, handler_fn, _atom_str, ...) [LGFX_OP_##op_name] = (handler_fn),

static const lgfx_handler_fn s_handlers[LGFX_OP_COUNT] = {
#include "lgfx_port/ops.def"
};
#undef X

_Static_assert((sizeof(s_op_meta) / sizeof(s_op_meta[0])) == LGFX_OP_COUNT, "s_op_meta size mismatch");
#if LGFX_PORT_DEBUG
_Static_assert((sizeof(s_op_names) / sizeof(s_op_names[0])) == LGFX_OP_COUNT, "s_op_names size mismatch");
#endif
_Static_assert((sizeof(s_handlers) / sizeof(s_handlers[0])) == LGFX_OP_COUNT, "s_handlers size mismatch");

static int lgfx_op_index_from_atom(const lgfx_port_t *port, term op_atom)
{
#define X(op_name, _handler_fn, _atom_str, ...) \
    if (op_atom == port->atoms.op_name) {       \
        return LGFX_OP_##op_name;               \
    }

#include "lgfx_port/ops.def"
#undef X

    return -1;
}

// -----------------------------------------------------------------------------
// getCaps: metadata-driven FeatureBits + op enable gating
// -----------------------------------------------------------------------------

static inline bool lgfx_cap_bit_enabled(uint32_t cap_bits)
{
    if (cap_bits == 0u) {
        return true;
    }
    if ((cap_bits & ~((uint32_t) LGFX_CAP_KNOWN_MASK)) != 0u) {
        return false;
    }
    return (cap_bits & ~((uint32_t) LGFX_BUILD_CAP_MASK)) == 0u;
}

static bool lgfx_op_gated_by_index(int op_index)
{
    if (op_index < 0 || op_index >= (int) LGFX_OP_COUNT) {
        return false;
    }

    const uint32_t cap_bit = s_op_meta[op_index].feature_cap_bit;
    return lgfx_cap_bit_enabled(cap_bit);
}

static bool lgfx_op_enabled_by_index(int op_index)
{
    if (!lgfx_op_gated_by_index(op_index)) {
        return false;
    }

    if (s_handlers[op_index] == NULL) {
        return false;
    }

    return true;
}

uint32_t lgfx_port_feature_bits(const lgfx_port_t *port)
{
    (void) port;

    uint32_t bits = 0;

    for (int i = 0; i < (int) LGFX_OP_COUNT; i++) {
        const uint32_t cap_bit = s_op_meta[i].feature_cap_bit;

        if (cap_bit == 0u) {
            continue;
        }
        if (!lgfx_op_enabled_by_index(i)) {
            continue;
        }

        bits |= cap_bit;
    }

    return bits & (uint32_t) LGFX_CAP_KNOWN_MASK;
}

uint8_t lgfx_port_max_sprites(const lgfx_port_t *port)
{
    const uint32_t bits = lgfx_port_feature_bits(port);
    if ((bits & (uint32_t) LGFX_CAP_SPRITE) == 0u) {
        return 0;
    }

    return (uint8_t) LGFX_PORT_MAX_SPRITES;
}

static bool lgfx_port_op_is_enabled(const lgfx_port_t *port, term op_atom)
{
    if (port == NULL) {
        return false;
    }

    const int op_index = lgfx_op_index_from_atom(port, op_atom);
    if (op_index < 0) {
        return false;
    }

    return lgfx_op_gated_by_index(op_index);
}

const lgfx_op_meta_t *lgfx_op_meta_lookup(const lgfx_port_t *port, term op_atom)
{
    int op_index = lgfx_op_index_from_atom(port, op_atom);
    if (op_index < 0) {
        return NULL;
    }

    return &s_op_meta[op_index];
}

const char *lgfx_op_name_from_atom(const lgfx_port_t *port, term op_atom)
{
    int op_index = lgfx_op_index_from_atom(port, op_atom);
    if (op_index < 0) {
        return "unknown_op";
    }

#if LGFX_PORT_DEBUG
    return s_op_names[op_index];
#else
    return "op";
#endif
}

lgfx_handler_fn lgfx_dispatch_lookup(lgfx_port_t *port, term op_atom)
{
    int op_index = lgfx_op_index_from_atom((const lgfx_port_t *) port, op_atom);
    if (op_index < 0) {
        return NULL;
    }

    if (!lgfx_op_enabled_by_index(op_index)) {
        return NULL;
    }

    return s_handlers[op_index];
}

// -----------------------------------------------------------------------------
// Validation helpers
// -----------------------------------------------------------------------------

static term lgfx_require_proto_ver(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    if (req->proto_ver != (uint32_t) LGFX_PORT_PROTO_VER) {
        return reply_error(ctx, port, req, port->atoms.bad_proto, 0);
    }

    return term_invalid_term();
}

static term lgfx_require_target_domain(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    // Protocol target domain is always 0..254 (0 = LCD, 1..254 = sprite).
    // This check is intentionally independent of per-op target_policy.
    if (req->target > 254u) {
        return reply_error(ctx, port, req, port->atoms.bad_target, 0);
    }

    return term_invalid_term();
}

static term lgfx_require_arity_range(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, int min_arity, int max_arity)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    if (min_arity > max_arity) {
        return reply_error(ctx, port, req, port->atoms.internal, 0);
    }

    if (req->arity < min_arity || req->arity > max_arity) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    return term_invalid_term();
}

static term lgfx_require_flags_allowed_mask(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, uint32_t allowed_mask)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    if ((req->flags & ~allowed_mask) != 0u) {
        return reply_error(ctx, port, req, port->atoms.bad_flags, 0);
    }

    return term_invalid_term();
}

static term lgfx_require_target_zero_reason(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason_atom)
{
    if (req->target != 0u) {
        return reply_error(ctx, port, req, reason_atom, 0);
    }

    return term_invalid_term();
}

static term lgfx_require_target_sprite_only(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (req->target >= 1u && req->target <= 254u) {
        return term_invalid_term();
    }

    return reply_error(ctx, port, req, port->atoms.bad_target, 0);
}

static term lgfx_require_target_policy(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, uint8_t policy)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    switch ((lgfx_op_target_policy_t) policy) {
        case LGFX_OP_TARGET_ANY:
            return term_invalid_term();

        case LGFX_OP_TARGET_BAD_TARGET:
            return lgfx_require_target_zero_reason(ctx, port, req, port->atoms.bad_target);

        case LGFX_OP_TARGET_UNSUPPORTED:
            return lgfx_require_target_zero_reason(ctx, port, req, port->atoms.unsupported);

        case LGFX_OP_TARGET_SPRITE_ONLY:
            return lgfx_require_target_sprite_only(ctx, port, req);

        default:
            return reply_error(ctx, port, req, port->atoms.internal, 0);
    }
}

static term lgfx_require_state_policy(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, uint8_t policy)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    switch ((lgfx_op_state_policy_t) policy) {
        case LGFX_OP_STATE_ANY:
            return term_invalid_term();

        case LGFX_OP_STATE_REQUIRES_INIT:
            if (port->initialized) {
                return term_invalid_term();
            }

            return reply_error(ctx, port, req, port->atoms.not_initialized, 0);

        default:
            return reply_error(ctx, port, req, port->atoms.internal, 0);
    }
}

// -----------------------------------------------------------------------------
// Port lifecycle + mailbox -> decode -> validate -> dispatch
// -----------------------------------------------------------------------------

static term ensure_valid_reply(Context *ctx, lgfx_port_t *port, term reply)
{
    if (!term_is_invalid_term(reply)) {
        return reply;
    }

    if (memory_ensure_free(ctx, 3) != MEMORY_GC_OK) {
        return term_invalid_term();
    }

    return lgfx_reply_error(ctx, port, port->atoms.no_memory);
}

static void lgfx_port_teardown(Context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    lgfx_port_t *port = (lgfx_port_t *) ctx->platform_data;
    if (port == NULL) {
        return;
    }

    ctx->platform_data = NULL;

    if (port->initialized) {
        (void) lgfx_worker_device_close(port);
    }

    port->initialized = false;
    port->width = 0;
    port->height = 0;
    lgfx_last_error_clear(port);

    lgfx_worker_stop(port);

    port->ctx = NULL;

    free(port);
}

void lgfx_port_handle_mailbox_message(Context *ctx, lgfx_port_t *port, term msg)
{
    GenMessage gen;
    enum GenMessageParseResult parse_res = port_parse_gen_message(msg, &gen);
    if (parse_res != GenCallMessage) {
        return;
    }

    lgfx_request_t req;
    term decode_error = term_invalid_term();

    if (!lgfx_term_decode_request(ctx, port, gen.req, &req, &decode_error)) {

        /*
         * Decode failed before request metadata exists.
         *
         * If decode_error is invalid, we likely failed while building the error
         * reply, so treat it as no_memory. Otherwise default to bad_proto, but
         * prefer the explicit reason when decode_error is already {error, Reason}.
         */
        term reason = term_is_invalid_term(decode_error) ? port->atoms.no_memory : port->atoms.bad_proto;

        term decoded_reason = term_invalid_term();
        if (!term_is_invalid_term(decode_error)
            && lgfx_is_error_reply(ctx, port, decode_error, &decoded_reason)
            && term_is_atom(decoded_reason)) {
            reason = decoded_reason;
        }

        lgfx_last_error_set(port, port->atoms.none, reason, 0, 0, 0);

        term safe = ensure_valid_reply(ctx, port, decode_error);
        if (!term_is_invalid_term(safe)) {
            port_send_reply(ctx, gen.pid, gen.ref, safe);
        }

        return;
    }

    term reply = term_invalid_term();
    term pre = term_invalid_term();

    pre = lgfx_require_proto_ver(ctx, port, &req);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    pre = lgfx_require_target_domain(ctx, port, &req);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    const lgfx_op_meta_t *meta = lgfx_op_meta_lookup(port, req.op);
    if (meta == NULL) {
        reply = reply_error(ctx, port, &req, port->atoms.bad_op, 0);
        goto send_reply;
    }

    if (!lgfx_port_op_is_enabled(port, req.op)) {
        reply = reply_error(ctx, port, &req, port->atoms.unsupported, 0);
        goto send_reply;
    }

    pre = lgfx_require_arity_range(ctx, port, &req, meta->min_arity, meta->max_arity);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    pre = lgfx_require_flags_allowed_mask(ctx, port, &req, meta->allowed_flags_mask);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    pre = lgfx_require_target_policy(ctx, port, &req, meta->target_policy);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    pre = lgfx_require_state_policy(ctx, port, &req, meta->state_policy);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    lgfx_handler_fn handler = lgfx_dispatch_lookup(port, req.op);
    if (handler == NULL) {
        reply = reply_error(ctx, port, &req, port->atoms.internal, 0);
        goto send_reply;
    }

    reply = handler(ctx, port, &req);

send_reply:
    if (term_is_invalid_term(reply)) {
        int32_t esp_err = 0;
        if (port->last_error.last_op == req.op) {
            esp_err = port->last_error.esp_err;
        }

        if (port->last_error.last_op != req.op || port->last_error.reason != port->atoms.no_memory) {
            reply = reply_error(ctx, port, &req, port->atoms.no_memory, esp_err);
        }

        reply = ensure_valid_reply(ctx, port, reply);
    }

    if (!term_is_invalid_term(reply)) {
        port_send_reply(ctx, gen.pid, gen.ref, reply);
    }
}

static void lgfx_port_drain_mailbox(lgfx_port_t *port)
{
    if (port == NULL || port->ctx == NULL) {
        return;
    }

    Context *ctx = port->ctx;
    Mailbox *mailbox = &ctx->mailbox;
    Heap *heap = &ctx->heap;

    while (true) {
        MailboxMessage *message = mailbox_take_message(mailbox);
        if (message == NULL) {
            break;
        }

        if (message->type == NormalMessage) {
            Message *normal_message = (Message *) message;
            term message_term = normal_message->message;

            lgfx_port_handle_mailbox_message(ctx, port, message_term);
        }

        mailbox_message_dispose(message, heap);
    }
}

static NativeHandlerResult lgfx_port_native_handler(Context *ctx)
{
    lgfx_port_t *port = (lgfx_port_t *) ctx->platform_data;
    if (port == NULL) {
        return NativeContinue;
    }

    if (ctx->flags & Killed) {
        lgfx_port_teardown(ctx);
        return NativeContinue;
    }

    lgfx_port_drain_mailbox(port);

    return NativeContinue;
}

static void lgfx_port_init(GlobalContext *global)
{
    (void) global;
}

static void lgfx_port_destroy(GlobalContext *global)
{
    (void) global;
}

static Context *lgfx_port_create_port(GlobalContext *global, term opts)
{
    (void) opts;

    Context *ctx = context_new(global);
    if (ctx == NULL) {
        return NULL;
    }

    lgfx_port_t *port = (lgfx_port_t *) calloc(1, sizeof(lgfx_port_t));
    if (port == NULL) {
        context_destroy(ctx);
        return NULL;
    }

    port->global = global;
    port->ctx = ctx;

    lgfx_atoms_init(global, &port->atoms);
    lgfx_last_error_clear(port);

    if (!lgfx_worker_start(port)) {
        free(port);
        context_destroy(ctx);
        return NULL;
    }

    ctx->platform_data = port;
    ctx->native_handler = lgfx_port_native_handler;

    return ctx;
}

REGISTER_PORT_DRIVER(lgfx_port, lgfx_port_init, lgfx_port_destroy, lgfx_port_create_port);
