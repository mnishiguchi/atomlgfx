// ports/proto_caps.c
#include "lgfx_port/proto_caps.h"

#include <stdint.h>

#include "lgfx_port/caps.h"

#ifndef LGFX_PORT_SUPPORTS_LAST_ERROR
#define LGFX_PORT_SUPPORTS_LAST_ERROR 1
#endif

/*
 * Safe-yield cap must be wire-safe:
 * - either disabled (0), or exactly one bit
 * - if non-zero, it must be inside the protocol-known capability mask
 */
_Static_assert(
    ((uint32_t) LGFX_PORT_SAFE_YIELD_CAP) == 0u
        || ((((uint32_t) LGFX_PORT_SAFE_YIELD_CAP) & (((uint32_t) LGFX_PORT_SAFE_YIELD_CAP) - 1u)) == 0u),
    "LGFX_PORT_SAFE_YIELD_CAP must be 0 or a single-bit mask");

_Static_assert(
    ((((uint32_t) LGFX_PORT_SAFE_YIELD_CAP) & ~((uint32_t) LGFX_CAP_KNOWN_MASK)) == 0u),
    "LGFX_PORT_SAFE_YIELD_CAP must be within LGFX_CAP_KNOWN_MASK");

static uint32_t build_ops_def_feature_bits(void)
{
    uint32_t feature_bits = 0u;

#define X(_op_name, _handler_fn, _atom_str, _min_arity, _max_arity, _allowed_flags_mask, _target_policy, _state_policy, feature_cap_bit) \
    do {                                                                                                                                 \
        uint32_t caps = ((uint32_t) (feature_cap_bit)) & (uint32_t) LGFX_CAP_KNOWN_MASK;                                                 \
        if (caps == 0u) {                                                                                                                 \
            break;                                                                                                                        \
        }                                                                                                                                 \
        feature_bits |= caps;                                                                                                             \
    } while (0);
#include "lgfx_port/ops.def"
#undef X

    return feature_bits & (uint32_t) LGFX_CAP_KNOWN_MASK;
}

uint32_t lgfx_proto_feature_bits(void)
{
    // Single source of truth: derive protocol FeatureBits from ops.def metadata.
    uint32_t feature_bits = build_ops_def_feature_bits();

#if !LGFX_PORT_SUPPORTS_LAST_ERROR
    feature_bits &= ~((uint32_t) LGFX_CAP_LAST_ERROR);
#endif

    /*
     * Safe-yield is only meaningful when the protocol supports transaction-style ops.
     * For now, treat BATCH_VOID as the transaction capability marker.
     * When you add other transaction ops later, extend this mask.
     */
    const uint32_t tx_mask = (uint32_t) LGFX_CAP_BATCH_VOID;
    if ((feature_bits & tx_mask) != 0u) {
        feature_bits |= (uint32_t) LGFX_PORT_SAFE_YIELD_CAP; // 0 or exactly one safe-yield bit
    }

    // Hard safety: only protocol FeatureBits may go on the wire.
    return feature_bits & (uint32_t) LGFX_CAP_KNOWN_MASK;
}
