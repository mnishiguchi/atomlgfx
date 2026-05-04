/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/op_registry.c

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/protocol.h"

#ifndef LGFX_PORT_DEBUG
#define LGFX_PORT_DEBUG 0
#endif

_Static_assert(CHAR_BIT == 8, "This code assumes 8-bit bytes");
_Static_assert(LGFX_REQ_MAX_INLINE_ARGS >= 1, "LGFX_REQ_MAX_INLINE_ARGS must be >= 1");
_Static_assert(LGFX_OP_TARGET_BAD_TARGET == 0, "LGFX_OP_TARGET_BAD_TARGET must be 0");
_Static_assert(LGFX_OP_TARGET_UNSUPPORTED == 1, "LGFX_OP_TARGET_UNSUPPORTED must be 1");
_Static_assert(LGFX_OP_TARGET_ANY == 2, "LGFX_OP_TARGET_ANY must be 2");
_Static_assert(LGFX_OP_TARGET_SPRITE_ONLY == 3, "LGFX_OP_TARGET_SPRITE_ONLY must be 3");
_Static_assert(LGFX_OP_STATE_ANY == 0, "LGFX_OP_STATE_ANY must be 0");
_Static_assert(LGFX_OP_STATE_REQUIRES_INIT == 1, "LGFX_OP_STATE_REQUIRES_INIT must be 1");

#define X(op_name, _handler_fn, _atom_str, _min_arity_v, max_arity_v, ...) \
    _Static_assert(((max_arity_v) - 5) <= LGFX_REQ_MAX_INLINE_ARGS, #op_name " exceeds LGFX_REQ_MAX_INLINE_ARGS");
#include "lgfx_port/ops.def"
#undef X

#if LGFX_PORT_DEBUG
_Static_assert(sizeof(lgfx_op_meta_t) == 16, "lgfx_op_meta_t must stay 16 bytes");
_Static_assert(offsetof(lgfx_op_meta_t, allowed_flags_mask) == 0, "allowed_flags_mask offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, feature_cap_bit) == 4, "feature_cap_bit offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, min_arity) == 8, "min_arity offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, max_arity) == 9, "max_arity offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, target_policy) == 10, "target_policy offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, state_policy) == 11, "state_policy offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, batchable) == 12, "batchable offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, needs_owned_payload) == 13, "needs_owned_payload offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, sync_only) == 14, "sync_only offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, batch_boundary_sensitive) == 15, "batch_boundary_sensitive offset drift");
#endif

#define X(                                                                                                                                                                                                       \
    op_name,                                                                                                                                                                                                     \
    _handler_fn,                                                                                                                                                                                                 \
    _atom_str,                                                                                                                                                                                                   \
    min_arity_v,                                                                                                                                                                                                 \
    max_arity_v,                                                                                                                                                                                                 \
    allowed_flags_mask_v,                                                                                                                                                                                        \
    target_policy_v,                                                                                                                                                                                             \
    state_policy_v,                                                                                                                                                                                              \
    feature_cap_bit_v,                                                                                                                                                                                           \
    batchable_v,                                                                                                                                                                                                 \
    needs_owned_payload_v,                                                                                                                                                                                       \
    sync_only_v,                                                                                                                                                                                                 \
    batch_boundary_sensitive_v)                                                                                                                                                                                  \
    _Static_assert((min_arity_v) >= 0 && (min_arity_v) <= UINT8_MAX, #op_name " min_arity out of uint8_t range");                                                                                                \
    _Static_assert((max_arity_v) >= 0 && (max_arity_v) <= UINT8_MAX, #op_name " max_arity out of uint8_t range");                                                                                                \
    _Static_assert((min_arity_v) <= (max_arity_v), #op_name " min_arity must be <= max_arity");                                                                                                                  \
    _Static_assert(((uint32_t) (allowed_flags_mask_v)) == (allowed_flags_mask_v), #op_name " flags mask out of uint32_t range");                                                                                 \
    _Static_assert((target_policy_v) >= 0 && (target_policy_v) <= UINT8_MAX, #op_name " target_policy out of uint8_t range");                                                                                    \
    _Static_assert(                                                                                                                                                                                              \
        ((target_policy_v) == LGFX_OP_TARGET_BAD_TARGET) || ((target_policy_v) == LGFX_OP_TARGET_UNSUPPORTED) || ((target_policy_v) == LGFX_OP_TARGET_ANY) || ((target_policy_v) == LGFX_OP_TARGET_SPRITE_ONLY), \
        #op_name " invalid target_policy value");                                                                                                                                                                \
    _Static_assert((state_policy_v) >= 0 && (state_policy_v) <= UINT8_MAX, #op_name " state_policy out of uint8_t range");                                                                                       \
    _Static_assert(                                                                                                                                                                                              \
        ((state_policy_v) == LGFX_OP_STATE_ANY) || ((state_policy_v) == LGFX_OP_STATE_REQUIRES_INIT),                                                                                                            \
        #op_name " invalid state_policy value");                                                                                                                                                                 \
    _Static_assert(((uint32_t) (feature_cap_bit_v)) == (feature_cap_bit_v), #op_name " feature_cap_bit out of uint32_t range");                                                                                  \
    _Static_assert((((uint32_t) (feature_cap_bit_v)) & ~((uint32_t) LGFX_CAP_KNOWN_MASK)) == 0u, #op_name " feature_cap_bit has unknown bits");                                                                  \
    _Static_assert(((batchable_v) == 0) || ((batchable_v) == 1), #op_name " batchable must be 0 or 1");                                                                                                          \
    _Static_assert(((needs_owned_payload_v) == 0) || ((needs_owned_payload_v) == 1), #op_name " needs_owned_payload must be 0 or 1");                                                                            \
    _Static_assert(((sync_only_v) == 0) || ((sync_only_v) == 1), #op_name " sync_only must be 0 or 1");                                                                                                          \
    _Static_assert(((batch_boundary_sensitive_v) == 0) || ((batch_boundary_sensitive_v) == 1), #op_name " batch_boundary_sensitive must be 0 or 1");                                                             \
    _Static_assert(!((needs_owned_payload_v) && !(batchable_v)), #op_name " needs_owned_payload requires batchable");                                                                                            \
    _Static_assert(!((sync_only_v) && (batchable_v)), #op_name " sync_only and batchable cannot both be 1");                                                                                                     \
    _Static_assert(!((batch_boundary_sensitive_v) && !(batchable_v)), #op_name " batch_boundary_sensitive requires batchable");                                                                                  \
    _Static_assert(((batchable_v) + (sync_only_v)) == 1, #op_name " exactly one of batchable or sync_only must be 1");
#include "lgfx_port/ops.def"
#undef X

#define X(                                                                  \
    op_name,                                                                \
    _handler_fn,                                                            \
    _atom_str,                                                              \
    min_arity_v,                                                            \
    max_arity_v,                                                            \
    allowed_flags_mask_v,                                                   \
    target_policy_v,                                                        \
    state_policy_v,                                                         \
    feature_cap_bit_v,                                                      \
    batchable_v,                                                            \
    needs_owned_payload_v,                                                  \
    sync_only_v,                                                            \
    batch_boundary_sensitive_v)                                             \
    [LGFX_OP_##op_name] = {                                                 \
        .allowed_flags_mask = (uint32_t) (allowed_flags_mask_v),            \
        .feature_cap_bit = (uint32_t) (feature_cap_bit_v),                  \
        .min_arity = (uint8_t) (min_arity_v),                               \
        .max_arity = (uint8_t) (max_arity_v),                               \
        .target_policy = (uint8_t) (target_policy_v),                       \
        .state_policy = (uint8_t) (state_policy_v),                         \
        .batchable = (uint8_t) (batchable_v),                               \
        .needs_owned_payload = (uint8_t) (needs_owned_payload_v),           \
        .sync_only = (uint8_t) (sync_only_v),                               \
        .batch_boundary_sensitive = (uint8_t) (batch_boundary_sensitive_v), \
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

_Static_assert((sizeof(s_op_meta) / sizeof(s_op_meta[0])) == LGFX_OP_COUNT, "s_op_meta size mismatch");
#if LGFX_PORT_DEBUG
_Static_assert((sizeof(s_op_names) / sizeof(s_op_names[0])) == LGFX_OP_COUNT, "s_op_names size mismatch");
#endif

static bool lgfx_port_touch_attached(const lgfx_port_t *port)
{
#if (LGFX_PORT_ENABLE_TOUCH != 1)
    (void) port;
    return false;
#else
    lgfx_touch_driver_id_t touch_driver = LGFX_TOUCH_DRIVER_ID_XPT2046;
    int32_t touch_cs_gpio = (int32_t) LGFX_PORT_TOUCH_CS_GPIO;

    if (port != NULL) {
        if (port->open_config_overrides.has_touch_driver) {
            touch_driver = port->open_config_overrides.touch_driver;
        } else if (port->open_config_overrides.has_touch_i2c_port
            || port->open_config_overrides.has_touch_sda_gpio
            || port->open_config_overrides.has_touch_scl_gpio
            || port->open_config_overrides.has_touch_i2c_addr
            || port->open_config_overrides.has_touch_rst_gpio) {
            touch_driver = LGFX_TOUCH_DRIVER_ID_FT6336U;
        }

        if (port->open_config_overrides.has_touch_cs_gpio) {
            touch_cs_gpio = port->open_config_overrides.touch_cs_gpio;
        }
    }

    switch (touch_driver) {
        case LGFX_TOUCH_DRIVER_ID_XPT2046:
            return touch_cs_gpio >= 0;
        case LGFX_TOUCH_DRIVER_ID_FT6336U: {
            int32_t touch_sda_gpio = -1;
            int32_t touch_scl_gpio = -1;

            if (port != NULL && port->open_config_overrides.has_touch_sda_gpio) {
                touch_sda_gpio = port->open_config_overrides.touch_sda_gpio;
            }

            if (port != NULL && port->open_config_overrides.has_touch_scl_gpio) {
                touch_scl_gpio = port->open_config_overrides.touch_scl_gpio;
            }

            return touch_sda_gpio >= 0 && touch_scl_gpio >= 0;
        }
        default:
            return false;
    }
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
    return lgfx_op_gated_by_index(port, op_index);
}

bool lgfx_op_try_from_opcode(uint32_t opcode, lgfx_op_t *out_op)
{
    if (out_op == NULL || opcode >= (uint32_t) LGFX_OP_COUNT) {
        return false;
    }

    *out_op = (lgfx_op_t) opcode;
    return true;
}

const char *lgfx_op_name_from_op(lgfx_op_t op)
{
    if (op < 0 || op >= LGFX_OP_COUNT) {
        return "unknown_op";
    }

#if LGFX_PORT_DEBUG
    return s_op_names[op];
#else
    return "op";
#endif
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

bool lgfx_port_op_is_enabled_by_op(const lgfx_port_t *port, lgfx_op_t op)
{
    if (port == NULL || op < 0 || op >= LGFX_OP_COUNT) {
        return false;
    }

    return lgfx_op_enabled_by_index(port, (int) op);
}

const lgfx_op_meta_t *lgfx_op_meta_lookup_by_op(lgfx_op_t op)
{
    if (op < 0 || op >= LGFX_OP_COUNT) {
        return NULL;
    }

    return &s_op_meta[(int) op];
}

lgfx_handler_fn lgfx_dispatch_lookup_by_op(lgfx_port_t *port, lgfx_op_t op)
{
    if (op < 0 || op >= LGFX_OP_COUNT) {
        return NULL;
    }

    if (!lgfx_op_enabled_by_index(port, (int) op)) {
        return NULL;
    }

    switch (op) {
#define X(op_name, handler_fn, _atom_str, ...) \
        case LGFX_OP_##op_name:                \
            return handler_fn;
#include "lgfx_port/ops.def"
#undef X

        default:
            return NULL;
    }
}
