// ports/include/lgfx_port/op_meta.h
#pragma once

#include <stdint.h>

#include "lgfx_port/lgfx_port.h"
#include "term.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    LGFX_OP_TARGET_BAD_TARGET = 0,
    LGFX_OP_TARGET_UNSUPPORTED = 1,
    LGFX_OP_TARGET_ANY = 2,
    LGFX_OP_TARGET_SPRITE_ONLY = 3
} lgfx_op_target_policy_t;

typedef enum
{
    LGFX_OP_STATE_ANY = 0,
    LGFX_OP_STATE_REQUIRES_INIT = 1
} lgfx_op_state_policy_t;

typedef struct
{
    uint32_t allowed_flags_mask;   // per-op allowed flags
    uint32_t feature_cap_bit;      // 0 => always enabled, else required bit in getCaps FeatureBits
    uint8_t min_arity;             // tuple arity including header
    uint8_t max_arity;             // supports range for flag-dependent arity
    uint8_t target_policy;         // encoded lgfx_op_target_policy_t
    uint8_t state_policy;          // encoded lgfx_op_state_policy_t
} lgfx_op_meta_t;

const lgfx_op_meta_t *lgfx_op_meta_lookup(const lgfx_port_t *port, term op_atom);

// Debug helper generated from ops.def
const char *lgfx_op_name_from_atom(const lgfx_port_t *port, term op_atom);

#ifdef __cplusplus
}
#endif
