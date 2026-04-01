/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_runtime/include_internal/lgfx_runtime/lgfx_runtime.h

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#include "lgfx_runtime/batch_command.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool initialized;

    lgfx_batch_id_t next_batch_id;
    lgfx_batch_id_t last_batch_id;
    lgfx_batch_state_t last_batch_state;
    lgfx_batch_failure_t last_failure;

    bool has_pending_batch;
    lgfx_batch_command_t *pending_commands;
    size_t pending_command_count;
} lgfx_runtime_t;

void lgfx_runtime_reset(lgfx_runtime_t *runtime);

esp_err_t lgfx_runtime_init(lgfx_runtime_t *runtime);

void lgfx_runtime_deinit(lgfx_runtime_t *runtime);

bool lgfx_runtime_is_initialized(const lgfx_runtime_t *runtime);

esp_err_t lgfx_runtime_enqueue(
    lgfx_runtime_t *runtime,
    const lgfx_batch_command_t *commands,
    size_t command_count,
    lgfx_batch_id_t *out_batch_id);

bool lgfx_runtime_has_pending(const lgfx_runtime_t *runtime);

esp_err_t lgfx_runtime_process_pending(lgfx_runtime_t *runtime);

lgfx_batch_id_t lgfx_runtime_get_last_batch_id(const lgfx_runtime_t *runtime);

lgfx_batch_state_t lgfx_runtime_get_last_state(const lgfx_runtime_t *runtime);

void lgfx_runtime_get_last_failure(
    const lgfx_runtime_t *runtime,
    lgfx_batch_failure_t *out_failure);

#ifdef __cplusplus
}
#endif
