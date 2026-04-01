/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_runtime/include_internal/lgfx_runtime/lgfx_command_dispatch.h

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
    bool has_failure;
    uint32_t failed_index;
    esp_err_t failed_esp_err;
} lgfx_dispatch_batch_result_t;

void lgfx_dispatch_batch_result_clear(lgfx_dispatch_batch_result_t *result);

bool lgfx_command_dispatch_is_supported(lgfx_op_t op);

esp_err_t lgfx_command_dispatch_one(const lgfx_batch_command_t *command);

esp_err_t lgfx_command_dispatch_run_batch(
    const lgfx_batch_command_t *commands,
    size_t command_count,
    lgfx_dispatch_batch_result_t *out_result);

#ifdef __cplusplus
}
#endif
