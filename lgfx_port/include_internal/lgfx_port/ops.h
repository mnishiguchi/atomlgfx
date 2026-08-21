/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Stable operation identifiers and validation policies shared by the direct
// NIF dispatcher and the binary batch executor.
#pragma once

#include <stdint.h>

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

typedef enum
{
#define X(op_name, ...) LGFX_OP_##op_name,
#include "lgfx_port/ops.def"
#undef X

    LGFX_OP_COUNT
} lgfx_op_t;
