// ports/op_meta.c
#include "lgfx_port/op_meta.h"

#include <limits.h>
#include <stddef.h>

#include "lgfx_port/caps.h"

// Protocol enum values are part of wire-level behavior. Guard against accidental drift.
_Static_assert(CHAR_BIT == 8, "This code assumes 8-bit bytes");
_Static_assert(LGFX_OP_TARGET_BAD_TARGET == 0, "LGFX_OP_TARGET_BAD_TARGET must be 0");
_Static_assert(LGFX_OP_TARGET_UNSUPPORTED == 1, "LGFX_OP_TARGET_UNSUPPORTED must be 1");
_Static_assert(LGFX_OP_TARGET_ANY == 2, "LGFX_OP_TARGET_ANY must be 2");
_Static_assert(LGFX_OP_STATE_ANY == 0, "LGFX_OP_STATE_ANY must be 0");
_Static_assert(LGFX_OP_STATE_REQUIRES_INIT == 1, "LGFX_OP_STATE_REQUIRES_INIT must be 1");

// Keep layout deterministic and compact.
_Static_assert(sizeof(lgfx_op_meta_t) == 8, "lgfx_op_meta_t must stay 8 bytes");
_Static_assert(offsetof(lgfx_op_meta_t, allowed_flags_mask) == 0, "allowed_flags_mask offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, min_arity) == 4, "min_arity offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, max_arity) == 5, "max_arity offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, target_policy) == 6, "target_policy offset drift");
_Static_assert(offsetof(lgfx_op_meta_t, state_policy) == 7, "state_policy offset drift");

// Validate ops.def values at compile time.
#define X(op_name, _handler_fn, _atom_str, min_arity_v, max_arity_v, allowed_flags_mask_v, target_policy_v, state_policy_v, feature_cap_bit_v) \
    _Static_assert((min_arity_v) >= 0 && (min_arity_v) <= UINT8_MAX, #op_name " min_arity out of uint8_t range");                               \
    _Static_assert((max_arity_v) >= 0 && (max_arity_v) <= UINT8_MAX, #op_name " max_arity out of uint8_t range");                               \
    _Static_assert((min_arity_v) <= (max_arity_v), #op_name " min_arity must be <= max_arity");                                                 \
    _Static_assert(((uint32_t) (allowed_flags_mask_v)) == (allowed_flags_mask_v), #op_name " flags mask out of uint32_t range");                \
    _Static_assert((target_policy_v) >= 0 && (target_policy_v) <= UINT8_MAX, #op_name " target_policy out of uint8_t range");                   \
    _Static_assert(((target_policy_v) == LGFX_OP_TARGET_BAD_TARGET) ||                                                                           \
                       ((target_policy_v) == LGFX_OP_TARGET_UNSUPPORTED) ||                                                                      \
                       ((target_policy_v) == LGFX_OP_TARGET_ANY),                                                                                \
                   #op_name " invalid target_policy value");                                                                                      \
    _Static_assert((state_policy_v) >= 0 && (state_policy_v) <= UINT8_MAX, #op_name " state_policy out of uint8_t range");                      \
    _Static_assert(((state_policy_v) == LGFX_OP_STATE_ANY) ||                                                                                     \
                       ((state_policy_v) == LGFX_OP_STATE_REQUIRES_INIT),                                                                        \
                   #op_name " invalid state_policy value");                                                                                       \
    _Static_assert(((uint32_t) (feature_cap_bit_v)) == (feature_cap_bit_v), #op_name " feature_cap_bit out of uint32_t range");                 \
    _Static_assert((((uint32_t) (feature_cap_bit_v)) & ~((uint32_t) LGFX_CAP_KNOWN_MASK)) == 0u,                                                \
                   #op_name " feature_cap_bit has unknown bits");
#include "lgfx_port/ops.def"
#undef X

const lgfx_op_meta_t *lgfx_op_meta_lookup(const lgfx_port_t *port, term op_atom)
{
#define X(op_name, _handler_fn, _atom_str, min_arity_v, max_arity_v, allowed_flags_mask_v, target_policy_v, state_policy_v, feature_cap_bit_v) \
    if (op_atom == port->atoms.op_name) {                                                                                                         \
        static const lgfx_op_meta_t meta = {                                                                                                      \
            .allowed_flags_mask = (uint32_t) (allowed_flags_mask_v),                                                                              \
            .min_arity = (uint8_t) (min_arity_v),                                                                                                 \
            .max_arity = (uint8_t) (max_arity_v),                                                                                                 \
            .target_policy = (uint8_t) (target_policy_v),                                                                                         \
            .state_policy = (uint8_t) (state_policy_v),                                                                                           \
        };                                                                                                                                        \
        return &meta;                                                                                                                             \
    }

#include "lgfx_port/ops.def"
#undef X

    return NULL;
}

const char *lgfx_op_name_from_atom(const lgfx_port_t *port, term op_atom)
{
#define X(op_name, _handler_fn, _atom_str, ...) \
    if (op_atom == port->atoms.op_name) {       \
        return #op_name;                        \
    }

#include "lgfx_port/ops.def"
#undef X

    return "unknown_op";
}
