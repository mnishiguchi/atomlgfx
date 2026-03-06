// /lgfx_port/include_internal/lgfx_port/ops.h
#pragma once

#include <stdint.h>

#include "context.h"
#include "term.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations (keep this header light)
typedef struct lgfx_port_t lgfx_port_t;
typedef struct lgfx_request_t lgfx_request_t;

// ----------------------------------------------------------------------------
// Op policies
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// Op metadata
// ----------------------------------------------------------------------------
typedef struct
{
    uint32_t allowed_flags_mask; // per-op allowed flags
    uint32_t feature_cap_bit; // 0 => always enabled, else required bit in getCaps FeatureBits
    uint8_t min_arity; // tuple arity including header
    uint8_t max_arity; // supports range for flag-dependent arity
    uint8_t target_policy; // encoded lgfx_op_target_policy_t
    uint8_t state_policy; // encoded lgfx_op_state_policy_t
} lgfx_op_meta_t;

// ----------------------------------------------------------------------------
// Op enum
// ----------------------------------------------------------------------------
typedef enum
{
#define X(op_name, _handler_fn, _atom_str, ...) LGFX_OP_##op_name,
#include "lgfx_port/ops.def"
#undef X

    LGFX_OP_COUNT
} lgfx_op_t;

// ----------------------------------------------------------------------------
// Handler prototypes
// ----------------------------------------------------------------------------
#define X(_op, _handler, _atom_str, ...) \
    term _handler(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
#include "lgfx_port/ops.def"
#undef X

// ----------------------------------------------------------------------------
// Dispatch API
// ----------------------------------------------------------------------------
typedef term (*lgfx_handler_fn)(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);

lgfx_handler_fn lgfx_dispatch_lookup(lgfx_port_t *port, term op);

// ----------------------------------------------------------------------------
// Metadata lookup / debug helpers
// ----------------------------------------------------------------------------
const lgfx_op_meta_t *lgfx_op_meta_lookup(const lgfx_port_t *port, term op_atom);
const char *lgfx_op_name_from_atom(const lgfx_port_t *port, term op_atom);

#ifdef __cplusplus
}
#endif
