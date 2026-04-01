/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_runtime/lgfx_runtime.c

#include "lgfx_runtime/lgfx_runtime.h"
#include "lgfx_runtime/lgfx_command_dispatch.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"

static void lgfx_runtime_free_command_array(
    lgfx_batch_command_t *commands,
    size_t command_count)
{
    if (commands == NULL) {
        return;
    }

    for (size_t i = 0; i < command_count; i++) {
        if (commands[i].payload.bytes != NULL) {
            free(commands[i].payload.bytes);
            commands[i].payload.bytes = NULL;
            commands[i].payload.size = 0u;
        }
    }

    free(commands);
}

static esp_err_t lgfx_runtime_clone_command_array(
    const lgfx_batch_command_t *source,
    size_t command_count,
    lgfx_batch_command_t **out_commands)
{
    if (out_commands == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_commands = NULL;

    if (source == NULL || command_count == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_batch_command_t *cloned = (lgfx_batch_command_t *) calloc(command_count, sizeof(lgfx_batch_command_t));
    if (cloned == NULL) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < command_count; i++) {
        cloned[i] = source[i];

        if (source[i].payload.bytes != NULL && source[i].payload.size > 0u) {
            cloned[i].payload.bytes = (uint8_t *) malloc(source[i].payload.size);
            if (cloned[i].payload.bytes == NULL) {
                lgfx_runtime_free_command_array(cloned, command_count);
                return ESP_ERR_NO_MEM;
            }

            memcpy(cloned[i].payload.bytes, source[i].payload.bytes, source[i].payload.size);
            cloned[i].payload.size = source[i].payload.size;
        }
    }

    *out_commands = cloned;
    return ESP_OK;
}

static void lgfx_runtime_record_failure(
    lgfx_runtime_t *runtime,
    lgfx_batch_id_t batch_id,
    uint32_t failed_index,
    const lgfx_batch_command_t *command,
    esp_err_t err)
{
    if (runtime == NULL) {
        return;
    }

    runtime->last_batch_id = batch_id;
    runtime->last_batch_state = LGFX_BATCH_STATE_FAILED;
    runtime->last_failure.has_failure = true;
    runtime->last_failure.batch_id = batch_id;
    runtime->last_failure.failed_index = failed_index;
    runtime->last_failure.failed_op = (command != NULL) ? command->op : LGFX_OP_COUNT;
    runtime->last_failure.failed_target = (command != NULL) ? command->target : 0u;
    runtime->last_failure.failed_flags = (command != NULL) ? command->flags : 0u;
    runtime->last_failure.failed_esp_err = err;
}

void lgfx_runtime_reset(lgfx_runtime_t *runtime)
{
    if (runtime == NULL) {
        return;
    }

    if (runtime->pending_commands != NULL) {
        lgfx_runtime_free_command_array(
            runtime->pending_commands,
            runtime->pending_command_count);
    }

    runtime->initialized = false;
    runtime->next_batch_id = 1u;
    runtime->last_batch_id = LGFX_BATCH_ID_NONE;
    runtime->last_batch_state = LGFX_BATCH_STATE_IDLE;
    lgfx_batch_failure_clear(&runtime->last_failure);

    runtime->has_pending_batch = false;
    runtime->pending_commands = NULL;
    runtime->pending_command_count = 0u;
}

esp_err_t lgfx_runtime_init(lgfx_runtime_t *runtime)
{
    if (runtime == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_runtime_reset(runtime);
    runtime->initialized = true;

    return ESP_OK;
}

void lgfx_runtime_deinit(lgfx_runtime_t *runtime)
{
    if (runtime == NULL) {
        return;
    }

    lgfx_runtime_reset(runtime);
}

bool lgfx_runtime_is_initialized(const lgfx_runtime_t *runtime)
{
    return runtime != NULL && runtime->initialized;
}

esp_err_t lgfx_runtime_enqueue(
    lgfx_runtime_t *runtime,
    const lgfx_batch_command_t *commands,
    size_t command_count,
    lgfx_batch_id_t *out_batch_id)
{
    if (out_batch_id != NULL) {
        *out_batch_id = LGFX_BATCH_ID_NONE;
    }

    if (runtime == NULL || commands == NULL || command_count == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!runtime->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (runtime->has_pending_batch) {
        return ESP_ERR_INVALID_STATE;
    }

    lgfx_batch_command_t *cloned_commands = NULL;
    esp_err_t clone_err = lgfx_runtime_clone_command_array(commands, command_count, &cloned_commands);
    if (clone_err != ESP_OK) {
        return clone_err;
    }

    const lgfx_batch_id_t batch_id = runtime->next_batch_id++;
    if (runtime->next_batch_id == LGFX_BATCH_ID_NONE) {
        runtime->next_batch_id = 1u;
    }

    runtime->pending_commands = cloned_commands;
    runtime->pending_command_count = command_count;
    runtime->has_pending_batch = true;

    runtime->last_batch_id = batch_id;
    runtime->last_batch_state = LGFX_BATCH_STATE_QUEUED;
    lgfx_batch_failure_clear(&runtime->last_failure);

    if (out_batch_id != NULL) {
        *out_batch_id = batch_id;
    }

    return ESP_OK;
}

bool lgfx_runtime_has_pending(const lgfx_runtime_t *runtime)
{
    return runtime != NULL && runtime->initialized && runtime->has_pending_batch;
}

esp_err_t lgfx_runtime_process_pending(lgfx_runtime_t *runtime)
{
    if (runtime == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!runtime->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!runtime->has_pending_batch) {
        return ESP_OK;
    }

    const lgfx_batch_id_t batch_id = runtime->last_batch_id;
    lgfx_batch_command_t *commands = runtime->pending_commands;
    const size_t command_count = runtime->pending_command_count;

    runtime->pending_commands = NULL;
    runtime->pending_command_count = 0u;
    runtime->has_pending_batch = false;

    runtime->last_batch_state = LGFX_BATCH_STATE_RUNNING;
    lgfx_batch_failure_clear(&runtime->last_failure);

    lgfx_dispatch_batch_result_t dispatch_result;
    lgfx_dispatch_batch_result_clear(&dispatch_result);

    esp_err_t err = lgfx_command_dispatch_run_batch(
        commands,
        command_count,
        &dispatch_result);

    if (dispatch_result.has_failure) {
        const lgfx_batch_command_t *failed_command = (dispatch_result.failed_index < command_count)
            ? &commands[dispatch_result.failed_index]
            : NULL;

        lgfx_runtime_record_failure(
            runtime,
            batch_id,
            dispatch_result.failed_index,
            failed_command,
            dispatch_result.failed_esp_err);
    } else if (err != ESP_OK) {
        lgfx_runtime_record_failure(runtime, batch_id, 0u, NULL, err);
    } else {
        runtime->last_batch_id = batch_id;
        runtime->last_batch_state = LGFX_BATCH_STATE_COMPLETED;
    }

    lgfx_runtime_free_command_array(commands, command_count);
    return ESP_OK;
}

lgfx_batch_id_t lgfx_runtime_get_last_batch_id(const lgfx_runtime_t *runtime)
{
    if (runtime == NULL) {
        return LGFX_BATCH_ID_NONE;
    }

    return runtime->last_batch_id;
}

lgfx_batch_state_t lgfx_runtime_get_last_state(const lgfx_runtime_t *runtime)
{
    if (runtime == NULL) {
        return LGFX_BATCH_STATE_IDLE;
    }

    return runtime->last_batch_state;
}

void lgfx_runtime_get_last_failure(
    const lgfx_runtime_t *runtime,
    lgfx_batch_failure_t *out_failure)
{
    if (out_failure == NULL) {
        return;
    }

    if (runtime == NULL) {
        lgfx_batch_failure_clear(out_failure);
        return;
    }

    *out_failure = runtime->last_failure;
}
