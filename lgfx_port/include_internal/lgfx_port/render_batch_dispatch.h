/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Validate a multi-target render command stream without touching the device.
// Returns:
// - ESP_OK when every command is syntactically valid and supported
// - ESP_ERR_INVALID_ARG with out_malformed_command=true for malformed command bytes
// - ESP_ERR_NOT_SUPPORTED for unsupported render command opcodes
esp_err_t lgfx_render_batch_dispatch_validate(
    const uint8_t *bytes,
    size_t len,
    uint8_t initial_target,
    uint32_t *out_failed_index,
    uint8_t *out_failed_opcode,
    bool *out_malformed_command);

// Decode and execute a multi-target render command stream.
//
// When LGFX_PORT_RENDER_BATCH_PREVALIDATE=1, the stream is checked before the
// write session starts, so malformed bytes or unsupported opcodes fail without
// partial display mutation. When it is 0, the stream is validated while it is
// executed to avoid a second hot-path parse. Device, runtime, and malformed
// command failures can then stop execution after earlier commands have run.
esp_err_t lgfx_render_batch_dispatch_run(
    const uint8_t *bytes,
    size_t len,
    uint8_t initial_target,
    uint32_t *out_failed_index,
    uint8_t *out_failed_opcode,
    bool *out_malformed_command);

#ifdef __cplusplus
}
#endif
