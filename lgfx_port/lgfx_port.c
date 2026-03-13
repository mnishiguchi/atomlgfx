// lgfx_port/lgfx_port.c

// AtomVM port driver entry point for the LovyanGFX port.
//
// Responsibilities in this file:
// - Per-context port creation and teardown
// - Atom table initialization for this port (ops + common atoms)
// - Parsing open_port/2 options into a per-port persisted config snapshot
// - Enforcing the boundary between per-port config snapshots and native singleton ownership
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

#include "driver/spi_common.h"
#include "esp_log.h"

#include "lgfx_device.h"
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

#define LGFX_ATOM(global, len_bytes, atom_text) \
    globalcontext_make_atom((global), ATOM_STR(len_bytes, atom_text))

static const char *const TAG = "lgfx_port";

// -----------------------------------------------------------------------------
// Atom initialization
// -----------------------------------------------------------------------------

/*
 * Canonical op list: lgfx_port/include_internal/lgfx_port/ops.def
 * Protocol contract: docs/LGFX_PORT_PROTOCOL.md
 */
void lgfx_atoms_init(GlobalContext *global, lgfx_atoms_t *atoms)
{
    atoms->ok = globalcontext_make_atom(global, ATOM_STR("\x02", "ok"));
    atoms->error = globalcontext_make_atom(global, ATOM_STR("\x05", "error"));

    atoms->lgfx = globalcontext_make_atom(global, ATOM_STR("\x04", "lgfx"));

    atoms->pong = globalcontext_make_atom(global, ATOM_STR("\x04", "pong"));
    atoms->true_ = globalcontext_make_atom(global, ATOM_STR("\x04", "true"));
    atoms->false_ = globalcontext_make_atom(global, ATOM_STR("\x05", "false"));

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

    atoms->caps = globalcontext_make_atom(global, ATOM_STR("\x04", "caps"));
    atoms->last_error = globalcontext_make_atom(global, ATOM_STR("\x0A", "last_error"));
    atoms->none = globalcontext_make_atom(global, ATOM_STR("\x04", "none"));

    // Generated from lgfx_port/include_internal/lgfx_port/ops.def.
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

static bool lgfx_port_touch_attached(const lgfx_port_t *port)
{
#if (LGFX_PORT_ENABLE_TOUCH != 1)
    (void) port;
    return false;
#else
    int32_t touch_cs_gpio = (int32_t) LGFX_PORT_TOUCH_CS_GPIO;

    if (port != NULL && port->open_config_overrides.has_touch_cs_gpio) {
        touch_cs_gpio = port->open_config_overrides.touch_cs_gpio;
    }

    return touch_cs_gpio >= 0;
#endif
}

static uint32_t lgfx_port_enabled_cap_mask(const lgfx_port_t *port)
{
    uint32_t mask = ((uint32_t) LGFX_BUILD_CAP_MASK) & ~((uint32_t) LGFX_CAP_TOUCH);

    if (lgfx_port_touch_attached(port)) {
        mask |= (uint32_t) LGFX_CAP_TOUCH;
    }

    return mask;
}

static inline bool lgfx_cap_bit_enabled(const lgfx_port_t *port, uint32_t cap_bits)
{
    if (cap_bits == 0u) {
        return true;
    }
    if ((cap_bits & ~((uint32_t) LGFX_CAP_KNOWN_MASK)) != 0u) {
        return false;
    }
    return (cap_bits & ~lgfx_port_enabled_cap_mask(port)) == 0u;
}

static bool lgfx_op_gated_by_index(const lgfx_port_t *port, int op_index)
{
    if (op_index < 0 || op_index >= (int) LGFX_OP_COUNT) {
        return false;
    }

    const uint32_t cap_bit = s_op_meta[op_index].feature_cap_bit;
    return lgfx_cap_bit_enabled(port, cap_bit);
}

static bool lgfx_op_enabled_by_index(const lgfx_port_t *port, int op_index)
{
    if (!lgfx_op_gated_by_index(port, op_index)) {
        return false;
    }

    if (s_handlers[op_index] == NULL) {
        return false;
    }

    return true;
}

uint32_t lgfx_port_feature_bits(const lgfx_port_t *port)
{
    uint32_t bits = 0;

    for (int i = 0; i < (int) LGFX_OP_COUNT; i++) {
        const uint32_t cap_bit = s_op_meta[i].feature_cap_bit;

        if (cap_bit == 0u) {
            continue;
        }
        if (!lgfx_op_enabled_by_index(port, i)) {
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

    return lgfx_op_enabled_by_index(port, op_index);
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

    if (!lgfx_op_enabled_by_index(port, op_index)) {
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

    // Domain check only; per-op target_policy is validated later.
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
// open_port/2 option parsing
// -----------------------------------------------------------------------------

/*
 * Canonical open_port/2 config contract.
 *
 * Keep aligned with:
 * - examples/elixir/lib/lgfx_port.ex
 * - src/lgfx_device.h
 *
 * Rules:
 * - opts is a proper list of {key, value}
 * - duplicate keys are allowed; last value wins
 * - parser accepts canonical normalized wire values only
 * - parser does structural and value parsing only
 * - public wrappers may normalize aliases before calling open_port/2
 */

static bool lgfx_term_to_int32_checked(term value, int32_t *out_value)
{
    if (!out_value || !term_is_integer(value)) {
        return false;
    }

    avm_int_t parsed = term_to_int(value);

    if (parsed < (avm_int_t) INT32_MIN || parsed > (avm_int_t) INT32_MAX) {
        return false;
    }

    *out_value = (int32_t) parsed;
    return true;
}

static bool lgfx_parse_bool_value(GlobalContext *global, term value, uint8_t *out_value)
{
    term true_atom = LGFX_ATOM(global, "\x04", "true");
    term false_atom = LGFX_ATOM(global, "\x05", "false");

    if (value == true_atom) {
        *out_value = 1u;
        return true;
    }

    if (value == false_atom) {
        *out_value = 0u;
        return true;
    }

    return false;
}

static bool lgfx_parse_panel_driver_value(
    GlobalContext *global,
    term value,
    lgfx_panel_driver_id_t *out_value)
{
    if (value == LGFX_ATOM(global, "\x07", "ili9488")) {
        *out_value = LGFX_PANEL_DRIVER_ID_ILI9488;
        return true;
    }

    if (value == LGFX_ATOM(global, "\x07", "ili9341")) {
        *out_value = LGFX_PANEL_DRIVER_ID_ILI9341;
        return true;
    }

    if (value == LGFX_ATOM(global, "\x09", "ili9341_2")) {
        *out_value = LGFX_PANEL_DRIVER_ID_ILI9341_2;
        return true;
    }

    if (value == LGFX_ATOM(global, "\x06", "st7789")) {
        *out_value = LGFX_PANEL_DRIVER_ID_ST7789;
        return true;
    }

    return false;
}

static bool lgfx_parse_positive_u16_value(term value, uint16_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed <= 0 || parsed > (int32_t) UINT16_MAX) {
        return false;
    }

    *out_value = (uint16_t) parsed;
    return true;
}

/*
 * Current effective accepted range is 1..INT32_MAX.
 *
 * The storage slot is uint32_t, but the current term decoder path first routes
 * through int32_t (`term_to_int`), so values above INT32_MAX are not currently
 * representable here.
 */
static bool lgfx_parse_positive_u32_value(term value, uint32_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed <= 0) {
        return false;
    }

    *out_value = (uint32_t) parsed;
    return true;
}

static bool lgfx_parse_i32_value(term value, int32_t *out_value)
{
    return lgfx_term_to_int32_checked(value, out_value);
}

static bool lgfx_parse_rotation_value(term value, uint8_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed < 0 || parsed > 7) {
        return false;
    }

    *out_value = (uint8_t) parsed;
    return true;
}

static bool lgfx_parse_spi_mode_value(term value, uint8_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed < 0 || parsed > 3) {
        return false;
    }

    *out_value = (uint8_t) parsed;
    return true;
}

static bool lgfx_parse_gpio_value(term value, int32_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed < 0 || parsed > 255) {
        return false;
    }

    *out_value = parsed;
    return true;
}

static bool lgfx_parse_gpio_or_disabled_value(term value, int32_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed < -1 || parsed > 255) {
        return false;
    }

    *out_value = parsed;
    return true;
}

static bool lgfx_parse_spi_host_value(GlobalContext *global, term value, int32_t *out_value)
{
    if (value == LGFX_ATOM(global, "\x09", "spi2_host")) {
        *out_value = (int32_t) SPI2_HOST;
        return true;
    }

    if (value == LGFX_ATOM(global, "\x09", "spi3_host")) {
        *out_value = (int32_t) SPI3_HOST;
        return true;
    }

    return false;
}

static bool lgfx_parse_dma_channel_value(GlobalContext *global, term value, int32_t *out_value)
{
    int32_t parsed = 0;

    if (value == LGFX_ATOM(global, "\x0F", "spi_dma_ch_auto")) {
        *out_value = (int32_t) SPI_DMA_CH_AUTO;
        return true;
    }

    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed != 1 && parsed != 2) {
        return false;
    }

    *out_value = parsed;
    return true;
}

static bool lgfx_parse_open_option_tuple(
    GlobalContext *global,
    term entry,
    lgfx_open_config_overrides_t *overrides,
    const char **error_detail)
{
    if (!term_is_tuple(entry) || term_get_tuple_arity(entry) != 2) {
        *error_detail = "each open_port option must be a {key, value} tuple";
        return false;
    }

    term key = term_get_tuple_element(entry, 0);
    term value = term_get_tuple_element(entry, 1);

    if (!term_is_atom(key)) {
        *error_detail = "open_port option key must be an atom";
        return false;
    }

    if (key == LGFX_ATOM(global, "\x0C", "panel_driver")) {
        if (!lgfx_parse_panel_driver_value(global, value, &overrides->panel_driver)) {
            *error_detail = "bad value for panel_driver (expected ili9488, ili9341, ili9341_2, or st7789)";
            return false;
        }
        overrides->has_panel_driver = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x05", "width")) {
        if (!lgfx_parse_positive_u16_value(value, &overrides->width)) {
            *error_detail = "bad value for width (expected positive integer in 1..65535)";
            return false;
        }
        overrides->has_width = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x06", "height")) {
        if (!lgfx_parse_positive_u16_value(value, &overrides->height)) {
            *error_detail = "bad value for height (expected positive integer in 1..65535)";
            return false;
        }
        overrides->has_height = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x08", "offset_x")) {
        if (!lgfx_parse_i32_value(value, &overrides->offset_x)) {
            *error_detail = "bad value for offset_x (expected signed 32-bit integer)";
            return false;
        }
        overrides->has_offset_x = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x08", "offset_y")) {
        if (!lgfx_parse_i32_value(value, &overrides->offset_y)) {
            *error_detail = "bad value for offset_y (expected signed 32-bit integer)";
            return false;
        }
        overrides->has_offset_y = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0F", "offset_rotation")) {
        if (!lgfx_parse_rotation_value(value, &overrides->offset_rotation)) {
            *error_detail = "bad value for offset_rotation (expected integer in 0..7)";
            return false;
        }
        overrides->has_offset_rotation = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x08", "readable")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->readable)) {
            *error_detail = "bad value for readable (expected boolean)";
            return false;
        }
        overrides->has_readable = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x06", "invert")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->invert)) {
            *error_detail = "bad value for invert (expected boolean)";
            return false;
        }
        overrides->has_invert = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x09", "rgb_order")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->rgb_order)) {
            *error_detail = "bad value for rgb_order (expected boolean)";
            return false;
        }
        overrides->has_rgb_order = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0A", "dlen_16bit")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->dlen_16bit)) {
            *error_detail = "bad value for dlen_16bit (expected boolean)";
            return false;
        }
        overrides->has_dlen_16bit = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0C", "lcd_spi_mode")) {
        if (!lgfx_parse_spi_mode_value(value, &overrides->lcd_spi_mode)) {
            *error_detail = "bad value for lcd_spi_mode (expected integer in 0..3)";
            return false;
        }
        overrides->has_lcd_spi_mode = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x11", "lcd_freq_write_hz")) {
        if (!lgfx_parse_positive_u32_value(value, &overrides->lcd_freq_write_hz)) {
            *error_detail = "bad value for lcd_freq_write_hz (expected positive integer in 1..2147483647)";
            return false;
        }
        overrides->has_lcd_freq_write_hz = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x10", "lcd_freq_read_hz")) {
        if (!lgfx_parse_positive_u32_value(value, &overrides->lcd_freq_read_hz)) {
            *error_detail = "bad value for lcd_freq_read_hz (expected positive integer in 1..2147483647)";
            return false;
        }
        overrides->has_lcd_freq_read_hz = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0F", "lcd_dma_channel")) {
        if (!lgfx_parse_dma_channel_value(global, value, &overrides->lcd_dma_channel)) {
            *error_detail = "bad value for lcd_dma_channel (expected spi_dma_ch_auto, 1, or 2)";
            return false;
        }
        overrides->has_lcd_dma_channel = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0D", "lcd_spi_3wire")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->lcd_spi_3wire)) {
            *error_detail = "bad value for lcd_spi_3wire (expected boolean)";
            return false;
        }
        overrides->has_lcd_spi_3wire = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0C", "lcd_use_lock")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->lcd_use_lock)) {
            *error_detail = "bad value for lcd_use_lock (expected boolean)";
            return false;
        }
        overrides->has_lcd_use_lock = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0E", "lcd_bus_shared")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->lcd_bus_shared)) {
            *error_detail = "bad value for lcd_bus_shared (expected boolean)";
            return false;
        }
        overrides->has_lcd_bus_shared = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0D", "spi_sclk_gpio")) {
        if (!lgfx_parse_gpio_value(value, &overrides->spi_sclk_gpio)) {
            *error_detail = "bad value for spi_sclk_gpio (expected GPIO integer in 0..255)";
            return false;
        }
        overrides->has_spi_sclk_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0D", "spi_mosi_gpio")) {
        if (!lgfx_parse_gpio_value(value, &overrides->spi_mosi_gpio)) {
            *error_detail = "bad value for spi_mosi_gpio (expected GPIO integer in 0..255)";
            return false;
        }
        overrides->has_spi_mosi_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0D", "spi_miso_gpio")) {
        if (!lgfx_parse_gpio_or_disabled_value(value, &overrides->spi_miso_gpio)) {
            *error_detail = "bad value for spi_miso_gpio (expected GPIO integer in -1..255; -1 disables)";
            return false;
        }
        overrides->has_spi_miso_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0C", "lcd_spi_host")) {
        if (!lgfx_parse_spi_host_value(global, value, &overrides->lcd_spi_host)) {
            *error_detail = "bad value for lcd_spi_host (expected spi2_host or spi3_host)";
            return false;
        }
        overrides->has_lcd_spi_host = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0B", "lcd_cs_gpio")) {
        if (!lgfx_parse_gpio_value(value, &overrides->lcd_cs_gpio)) {
            *error_detail = "bad value for lcd_cs_gpio (expected GPIO integer in 0..255)";
            return false;
        }
        overrides->has_lcd_cs_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0B", "lcd_dc_gpio")) {
        if (!lgfx_parse_gpio_value(value, &overrides->lcd_dc_gpio)) {
            *error_detail = "bad value for lcd_dc_gpio (expected GPIO integer in 0..255)";
            return false;
        }
        overrides->has_lcd_dc_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0C", "lcd_rst_gpio")) {
        if (!lgfx_parse_gpio_or_disabled_value(value, &overrides->lcd_rst_gpio)) {
            *error_detail = "bad value for lcd_rst_gpio (expected GPIO integer in -1..255; -1 disables)";
            return false;
        }
        overrides->has_lcd_rst_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0C", "lcd_pin_busy")) {
        if (!lgfx_parse_gpio_or_disabled_value(value, &overrides->lcd_pin_busy)) {
            *error_detail = "bad value for lcd_pin_busy (expected GPIO integer in -1..255; -1 disables)";
            return false;
        }
        overrides->has_lcd_pin_busy = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0D", "touch_cs_gpio")) {
        if (!lgfx_parse_gpio_or_disabled_value(value, &overrides->touch_cs_gpio)) {
            *error_detail = "bad value for touch_cs_gpio (expected GPIO integer in -1..255; -1 disables)";
            return false;
        }
        overrides->has_touch_cs_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0E", "touch_irq_gpio")) {
        if (!lgfx_parse_gpio_or_disabled_value(value, &overrides->touch_irq_gpio)) {
            *error_detail = "bad value for touch_irq_gpio (expected GPIO integer in -1..255; -1 disables)";
            return false;
        }
        overrides->has_touch_irq_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0E", "touch_spi_host")) {
        if (!lgfx_parse_spi_host_value(global, value, &overrides->touch_spi_host)) {
            *error_detail = "bad value for touch_spi_host (expected spi2_host or spi3_host)";
            return false;
        }
        overrides->has_touch_spi_host = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x11", "touch_spi_freq_hz")) {
        if (!lgfx_parse_positive_u32_value(value, &overrides->touch_spi_freq_hz)) {
            *error_detail = "bad value for touch_spi_freq_hz (expected positive integer in 1..2147483647)";
            return false;
        }
        overrides->has_touch_spi_freq_hz = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x15", "touch_offset_rotation")) {
        if (!lgfx_parse_rotation_value(value, &overrides->touch_offset_rotation)) {
            *error_detail = "bad value for touch_offset_rotation (expected integer in 0..7)";
            return false;
        }
        overrides->has_touch_offset_rotation = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x10", "touch_bus_shared")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->touch_bus_shared)) {
            *error_detail = "bad value for touch_bus_shared (expected boolean)";
            return false;
        }
        overrides->has_touch_bus_shared = 1u;
        return true;
    }

    *error_detail = "unknown open_port option key";
    return false;
}

static bool lgfx_parse_open_config_opts(
    GlobalContext *global,
    term opts,
    lgfx_open_config_overrides_t *out_overrides,
    const char **error_detail)
{
    *out_overrides = (lgfx_open_config_overrides_t) { 0 };
    *error_detail = NULL;

    if (term_is_nil(opts)) {
        return true;
    }

    term cursor = opts;
    while (!term_is_nil(cursor)) {
        if (!term_is_list(cursor)) {
            *error_detail = "open_port opts must be a proper list";
            return false;
        }

        term entry = term_get_list_head(cursor);
        cursor = term_get_list_tail(cursor);

        if (!lgfx_parse_open_option_tuple(global, entry, out_overrides, error_detail)) {
            return false;
        }
    }

    return true;
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

    /*
     * port->initialized is a port-local lifecycle flag.
     *
     * It means this port completed init() successfully during its current
     * ownership window. It does not describe global singleton availability or
     * current ownership outside that window.
     *
     * Global publication, ownership, and ready-state live in
     * src/lgfx_device_state.cpp.
     */
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
         * Decode failed before full request metadata existed.
         *
         * If decode_error is invalid, treat it as no_memory. Otherwise prefer an
         * explicit {error, Reason} reply when one was already built.
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

    Message *message = mailbox_first(mailbox);
    while (message != NULL) {
        term message_term = message->message;

        lgfx_port_handle_mailbox_message(ctx, port, message_term);

        MailboxMessage *removed = mailbox_take_message(mailbox);
        if (removed == NULL) {
            break;
        }

        mailbox_message_dispose(removed, heap);

        message = mailbox_first(mailbox);
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

    lgfx_open_config_overrides_t open_config_overrides = { 0 };
    const char *open_config_error = NULL;

    if (!lgfx_parse_open_config_opts(global, opts, &open_config_overrides, &open_config_error)) {
        ESP_LOGE(
            TAG,
            "invalid open_port opts for lgfx_port: %s",
            open_config_error ? open_config_error : "unknown error");
        free(port);
        context_destroy(ctx);
        return NULL;
    }

    /*
     * Persist a per-port config snapshot.
     *
     * This includes the all-default case, so every opened port keeps a
     * deterministic baseline for future init() calls.
     *
     * The snapshot is configuration only. It is separate from singleton
     * publication, ownership, and begin()/ready state. Opening another port does
     * not overwrite a global pending config; the singleton constraint is enforced
     * only when a port tries to claim the live device via init().
     */
    port->open_config_overrides = open_config_overrides;

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
