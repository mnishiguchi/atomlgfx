/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_runtime/include_internal/lgfx_runtime/batch_command.h

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"

#include "lgfx_port/ops.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t lgfx_batch_id_t;

#define LGFX_BATCH_ID_NONE ((lgfx_batch_id_t) 0u)
#define LGFX_BATCH_INLINE_ARG_WORDS 12u

typedef enum
{
    LGFX_BATCH_STATE_IDLE = 0,
    LGFX_BATCH_STATE_QUEUED = 1,
    LGFX_BATCH_STATE_RUNNING = 2,
    LGFX_BATCH_STATE_COMPLETED = 3,
    LGFX_BATCH_STATE_FAILED = 4
} lgfx_batch_state_t;

typedef struct
{
    bool has_failure;
    lgfx_batch_id_t batch_id;
    uint32_t failed_index;
    lgfx_op_t failed_op;
    uint8_t failed_target;
    uint32_t failed_flags;
    esp_err_t failed_esp_err;
} lgfx_batch_failure_t;

typedef struct
{
    uint8_t *bytes;
    size_t size;
} lgfx_owned_payload_t;

/*
 * Milestone A skeleton:
 *
 * - Keep the command header explicit now.
 * - Keep inline args as opaque fixed storage for compile-only scaffolding.
 * - Milestone B can replace opaque storage with per-op decoded structs or
 *   union members as the first batchable MVP ops land.
 */
typedef struct
{
    uint32_t words[LGFX_BATCH_INLINE_ARG_WORDS];
} lgfx_batch_inline_args_t;

typedef struct
{
    lgfx_op_t op;
    uint8_t target;
    uint32_t flags;
    lgfx_batch_inline_args_t inline_args;
    lgfx_owned_payload_t payload;
} lgfx_batch_command_t;

static inline void lgfx_batch_failure_clear(lgfx_batch_failure_t *failure)
{
    if (failure == NULL) {
        return;
    }

    failure->has_failure = false;
    failure->batch_id = LGFX_BATCH_ID_NONE;
    failure->failed_index = 0u;
    failure->failed_op = LGFX_OP_COUNT;
    failure->failed_target = 0u;
    failure->failed_flags = 0u;
    failure->failed_esp_err = ESP_OK;
}

static inline void lgfx_owned_payload_clear(lgfx_owned_payload_t *payload)
{
    if (payload == NULL) {
        return;
    }

    payload->bytes = NULL;
    payload->size = 0u;
}

static inline void lgfx_batch_inline_args_clear(lgfx_batch_inline_args_t *inline_args)
{
    if (inline_args == NULL) {
        return;
    }

    memset(inline_args, 0, sizeof(*inline_args));
}

static inline void lgfx_batch_command_clear(lgfx_batch_command_t *command)
{
    if (command == NULL) {
        return;
    }

    command->op = LGFX_OP_COUNT;
    command->target = 0u;
    command->flags = 0u;
    lgfx_batch_inline_args_clear(&command->inline_args);
    lgfx_owned_payload_clear(&command->payload);
}

#ifdef __cplusplus
}
#endif
