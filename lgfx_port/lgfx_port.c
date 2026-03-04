// lgfx_port/lgfx_port.c
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
// - Device calls (handled by lgfx_worker.c / lgfx_device.*)
// - AtomVM term decoding details (handled by term_decode.*)
// - Reply encoding helpers (handled by proto_term.c)

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "context.h"
#include "defaultatoms.h" // ATOM_STR
#include "globalcontext.h"
#include "memory.h" // memory_ensure_free / MEMORY_GC_OK
#include "port.h" // port_parse_gen_message / port_send_reply
#include "portnifloader.h"

#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/worker.h"

// Private (non-public) validation API (implemented in lgfx_port/validate.c)
#include "lgfx_port/validate.h"

// Internal debug knobs (kept out of public headers)
// - Gates op name table and non-wire layout asserts.
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
 * Canonical op list: lgfx_port/include/lgfx_port/ops.def
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

    // Op atoms (generated from lgfx_port/include/lgfx_port/ops.def)
#define X(op, handler, atom_str, ...) atoms->op = globalcontext_make_atom(global, (atom_str));
#include "lgfx_port/ops.def"
#undef X
}

// -----------------------------------------------------------------------------
// Op metadata registry + dispatch lookup
// -----------------------------------------------------------------------------

// Protocol enum values are part of wire-level behavior. Guard against accidental drift.
_Static_assert(CHAR_BIT == 8, "This code assumes 8-bit bytes");
_Static_assert(LGFX_OP_TARGET_BAD_TARGET == 0, "LGFX_OP_TARGET_BAD_TARGET must be 0");
_Static_assert(LGFX_OP_TARGET_UNSUPPORTED == 1, "LGFX_OP_TARGET_UNSUPPORTED must be 1");
_Static_assert(LGFX_OP_TARGET_ANY == 2, "LGFX_OP_TARGET_ANY must be 2");
_Static_assert(LGFX_OP_TARGET_SPRITE_ONLY == 3, "LGFX_OP_TARGET_SPRITE_ONLY must be 3");
_Static_assert(LGFX_OP_STATE_ANY == 0, "LGFX_OP_STATE_ANY must be 0");
_Static_assert(LGFX_OP_STATE_REQUIRES_INIT == 1, "LGFX_OP_STATE_REQUIRES_INIT must be 1");

// Non-wire internal layout asserts (useful while iterating, but not part of the wire contract).
#if LGFX_PORT_DEBUG
_Static_assert(sizeof(lgfx_op_meta_t) == 12, "lgfx_op_meta_t must stay 12 bytes");
_Static_assert(offsetof(lgfx_op_meta_t, allowed_flags_mask) == 0, "allowed_flags_mask offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, feature_cap_bit) == 4, "feature_cap_bit offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, min_arity) == 8, "min_arity offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, max_arity) == 9, "max_arity offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, target_policy) == 10, "target_policy offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, state_policy) == 11, "state_policy offset drift");
#endif

// Validate ops.def values at compile time.
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

// Dispatch table (indexed by LGFX_OP_* enum)
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

// -----------------------------------------------------------------------------
// Capability toggle sanity: toggles must be backed by ops.def metadata.
//
// FeatureBits is derived from ops.def feature_cap_bit + enabled dispatch surface.
// Therefore, setting LGFX_PORT_SUPPORTS_* to 1 is only meaningful if at least one
// op in ops.def declares the corresponding LGFX_CAP_* bit.
// -----------------------------------------------------------------------------

enum
{
    LGFX_OPS_DECLARED_CAP_BITS = 0
#define X(_op_name, _handler_fn, _atom_str, _min_arity, _max_arity, _allowed_flags_mask, _target_policy, _state_policy, feature_cap_bit_v) \
    | ((int) (feature_cap_bit_v))
#include "lgfx_port/ops.def"
#undef X
};

_Static_assert(
    ((((uint32_t) LGFX_OPS_DECLARED_CAP_BITS) & ~((uint32_t) LGFX_CAP_KNOWN_MASK)) == 0u),
    "ops.def feature_cap_bit contains unknown bits");

#if LGFX_PORT_SUPPORTS_JPG_FILE
_Static_assert(
    ((((uint32_t) LGFX_OPS_DECLARED_CAP_BITS) & (uint32_t) LGFX_CAP_JPG_FILE) != 0u),
    "LGFX_PORT_SUPPORTS_JPG_FILE=1 but no op in ops.def declares LGFX_CAP_JPG_FILE");
#endif

#if LGFX_PORT_SUPPORTS_PNG_FILE
_Static_assert(
    ((((uint32_t) LGFX_OPS_DECLARED_CAP_BITS) & (uint32_t) LGFX_CAP_PNG_FILE) != 0u),
    "LGFX_PORT_SUPPORTS_PNG_FILE=1 but no op in ops.def declares LGFX_CAP_PNG_FILE");
#endif

#if LGFX_PORT_SUPPORTS_BATCH_VOID
_Static_assert(
    ((((uint32_t) LGFX_OPS_DECLARED_CAP_BITS) & (uint32_t) LGFX_CAP_BATCH_VOID) != 0u),
    "LGFX_PORT_SUPPORTS_BATCH_VOID=1 but no op in ops.def declares LGFX_CAP_BATCH_VOID");
#endif

#if (LGFX_PORT_SAFE_YIELD_CAP != 0u)
_Static_assert(
    ((((uint32_t) LGFX_OPS_DECLARED_CAP_BITS) & (uint32_t) LGFX_CAP_BATCH_VOID) != 0u),
    "LGFX_PORT_SAFE_YIELD_CAP requires a transaction surface; declare LGFX_CAP_BATCH_VOID on an op in ops.def");
#endif

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

// Gates only (build/runtime capability gates via cap bits), independent of dispatch wiring.
static bool lgfx_op_gated_by_index(int op_index)
{
    if (op_index < 0 || op_index >= (int) LGFX_OP_COUNT) {
        return false;
    }

    const uint32_t cap_bit = s_op_meta[op_index].feature_cap_bit;
    return lgfx_cap_bit_enabled(cap_bit);
}

// Hardening: "enabled" surface also requires a live dispatch entry.
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

    /*
     * NOTE:
     * - Only op-linked capability bits are advertised (ops.def feature_cap_bit).
     * - Build toggles alone do not advertise reserved bits; see sanity asserts above.
     */
    uint32_t bits = 0;
    bool has_transaction_ops = false;

    // Op-linked capability bits: derived from ops.def metadata and enabled dispatch.
    for (int i = 0; i < (int) LGFX_OP_COUNT; i++) {
        const uint32_t cap_bit = s_op_meta[i].feature_cap_bit;

        if (cap_bit == 0u) {
            continue;
        }
        if (!lgfx_op_enabled_by_index(i)) {
            continue;
        }

        bits |= cap_bit;

        // Treat CAP_BATCH_VOID as a transaction-style surface for safe-yield advertisement.
        if ((cap_bit & (uint32_t) LGFX_CAP_BATCH_VOID) != 0u) {
            has_transaction_ops = true;
        }
    }

    // Safe-yield cap is only valid when transaction-style ops exist.
    if (has_transaction_ops) {
        bits |= (uint32_t) LGFX_PORT_SAFE_YIELD_CAP;
    }

    return bits & (uint32_t) LGFX_CAP_KNOWN_MASK;
}

uint8_t lgfx_port_max_sprites(const lgfx_port_t *port)
{
    const uint32_t bits = lgfx_port_feature_bits(port);
    if ((bits & (uint32_t) LGFX_CAP_SPRITE) == 0u) {
        // Contract: MaxSprites must be 0 when CAP_SPRITE is not advertised.
        return 0;
    }

    return (uint8_t) LGFX_PORT_MAX_SPRITES;
}

bool lgfx_port_op_is_enabled(const lgfx_port_t *port, term op_atom)
{
    if (port == NULL) {
        return false;
    }

    const int op_index = lgfx_op_index_from_atom(port, op_atom);
    if (op_index < 0) {
        return false;
    }

    // Public meaning: enabled by build/runtime gates (not dispatch wiring).
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

    // Disabled ops (by gates or missing dispatch entry) are treated as not present.
    if (!lgfx_op_enabled_by_index(op_index)) {
        return NULL;
    }

    return s_handlers[op_index];
}

// -----------------------------------------------------------------------------
// Port lifecycle + mailbox -> decode -> validate -> dispatch
// -----------------------------------------------------------------------------

/*
 * Ensure the reply term is valid.
 *
 * If a handler/validator returns an invalid term (typically due to OOM while
 * constructing a reply), try to replace it with {error, no_memory}.
 * If that allocation also fails, return invalid_term() and the caller will
 * skip sending a reply.
 */
static term ensure_valid_reply(Context *ctx, lgfx_port_t *port, term reply)
{
    if (!term_is_invalid_term(reply)) {
        return reply;
    }

    // Best effort: allocate {error, no_memory}.
    if (memory_ensure_free(ctx, /* tuple2 */ 3) != MEMORY_GC_OK) {
        return term_invalid_term();
    }

    return lgfx_reply_error(ctx, port, port->atoms.no_memory);
}

/*
 * Per-port-instance teardown.
 *
 * Important:
 * - REGISTER_PORT_DRIVER init/destroy callbacks are driver-global lifecycle hooks.
 * - lgfx_port_t is per AtomVM Context and is owned via ctx->platform_data.
 * - Therefore, per-port cleanup must happen from the native handler when the
 *   context is marked Killed.
 *
 * Teardown order:
 * 1) Detach ctx->platform_data first (double-teardown guard)
 * 2) Best-effort device close while worker is still running
 * 3) Reset local lifecycle/cache state
 * 4) Stop worker task/queue
 * 5) Free lgfx_port_t
 */
static void lgfx_port_teardown(Context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    lgfx_port_t *port = (lgfx_port_t *) ctx->platform_data;
    if (port == NULL) {
        return;
    }

    // Guard against accidental double teardown.
    ctx->platform_data = NULL;

    /*
     * Best-effort device close before stopping the worker.
     *
     * lgfx_worker_device_close() runs through the worker queue, so the worker must
     * still be alive here. Ignore the return value during teardown: cleanup should
     * continue even if device close fails.
     */
    if (port->initialized) {
        (void) lgfx_worker_device_close(port);
    }

    // Final local state reset (defensive; port is freed immediately after).
    port->initialized = false;
    port->width = 0;
    port->height = 0;
    lgfx_last_error_clear(port);

    // Stop/free worker resources, then free the port state.
    lgfx_worker_stop(port);

    // Break back-reference before free (debug hygiene).
    port->ctx = NULL;

    free(port);
}

/*
 * Process one mailbox message term on the port thread.
 *
 * Ownership:
 * - The mailbox drainer owns MailboxMessage* and disposes it.
 * - This function only consumes the message term and must not retain it.
 * - This function must not dispose mailbox messages.
 */
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
         * Decode failed before dispatch.
         *
         * If decode_error is invalid, we likely failed to allocate an error reply,
         * so treat it as no_memory. Otherwise default to bad_proto, but prefer the
         * explicit reason if decode_error is already a structured {error, Reason}.
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

    // 0) Protocol-wide envelope rules (proto_term.c does not enforce these).
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

    // 1) Metadata lookup from the generated op registry (unknown op => bad_op).
    const lgfx_op_meta_t *meta = lgfx_op_meta_lookup(port, req.op);
    if (meta == NULL) {
        lgfx_last_error_set(port, req.op, port->atoms.bad_op, req.flags, req.target, 0);
        reply = lgfx_reply_error(ctx, port, port->atoms.bad_op);
        goto send_reply;
    }

    // 1b) Build/runtime gated ops exist in metadata but are not part of the active surface.
    if (!lgfx_port_op_is_enabled(port, req.op)) {
        lgfx_last_error_set(port, req.op, port->atoms.unsupported, req.flags, req.target, 0);
        reply = lgfx_reply_error(ctx, port, port->atoms.unsupported);
        goto send_reply;
    }

    // 2) Shared validation driven by ops.def metadata.
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

    /*
     * 3) Dispatch lookup.
     *
     * This should always exist if ops.def metadata and the dispatch table stay in
     * sync. A miss here indicates an internal wiring mismatch.
     */
    lgfx_handler_fn handler = lgfx_dispatch_lookup(port, req.op);
    if (handler == NULL) {
        // If we got here, it is not a "disabled op" (we already handled that above).
        // Treat it as internal wiring drift.
        lgfx_last_error_set(port, req.op, port->atoms.internal, req.flags, req.target, 0);
        reply = lgfx_reply_error(ctx, port, port->atoms.internal);
        goto send_reply;
    }

    // 4) Execute handler (handlers may still perform op-specific validation/body).
    reply = handler(ctx, port, &req);

send_reply:
    /*
     * Normalize invalid replies to {error, no_memory} when possible.
     *
     * Handlers/validators may have already recorded a richer last_error (for example
     * an esp_err value). Keep that context where possible, but force reason=no_memory
     * if reply construction failed.
     */
    if (term_is_invalid_term(reply)) {
        if (port->last_error.last_op != req.op) {
            lgfx_last_error_set(port, req.op, port->atoms.no_memory, req.flags, req.target, 0);
        } else if (port->last_error.reason != port->atoms.no_memory) {
            lgfx_last_error_set(port, req.op, port->atoms.no_memory, req.flags, req.target, port->last_error.esp_err);
        }
        reply = ensure_valid_reply(ctx, port, reply);
    }

    if (!term_is_invalid_term(reply)) {
        port_send_reply(ctx, gen.pid, gen.ref, reply);
    }
}

static NativeHandlerResult lgfx_port_native_handler(Context *ctx)
{
    lgfx_port_t *port = (lgfx_port_t *) ctx->platform_data;
    if (port == NULL) {
        return NativeContinue;
    }

    /*
     * Per-context teardown path.
     *
     * AtomVM marks the Context as Killed before final destruction. Clean up the
     * worker task/queue and per-port state here, because REGISTER_PORT_DRIVER
     * destroy is driver-global and not called once per port instance.
     */
    if (ctx->flags & Killed) {
        lgfx_port_teardown(ctx);
        return NativeContinue;
    }

    // Normal path: port thread drains mailbox and processes messages.
    lgfx_worker_drain_mailbox(port);

    return NativeContinue;
}

static void lgfx_port_init(GlobalContext *global)
{
    (void) global;
}

static void lgfx_port_destroy(GlobalContext *global)
{
    /*
     * Driver-global teardown only.
     *
     * There is no per-port lgfx_port_t to free here. Per-context teardown is
     * handled in lgfx_port_native_handler() when the Context is marked Killed.
     */
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
