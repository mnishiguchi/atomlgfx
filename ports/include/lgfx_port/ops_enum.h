#pragma once

/*
 * Generated op enum from ops.def.
 *
 * Notes:
 * - Order is defined by ports/include/lgfx_port/ops.def
 * - LGFX_OP_COUNT is a sentinel for table sizing / sanity checks
 * - Enum names intentionally mirror ops.def op_name tokens (including camelCase)
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
#define X(op_name, _handler_fn, _atom_str, ...) LGFX_OP_##op_name,
#include "lgfx_port/ops.def"
#undef X

    LGFX_OP_COUNT
} lgfx_op_t;

#ifdef __cplusplus
}
#endif
