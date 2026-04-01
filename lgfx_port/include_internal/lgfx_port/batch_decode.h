/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/include_internal/lgfx_port/batch_decode.h
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_runtime/batch_command.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    lgfx_batch_command_t *commands;
    size_t command_count;
} lgfx_batch_build_t;

void lgfx_batch_build_clear(lgfx_batch_build_t *build);

/*
 * Outer submitBatch request:
 *
 *   {lgfx, ProtoVer, submitBatch, 0, 0, Commands}
 */
bool lgfx_batch_decode_submit_request(
    const lgfx_request_t *req,
    term *out_commands_term);

/*
 * First real batch builder slice:
 *
 * - validate that Commands is a non-empty proper list
 * - validate each inner command header against ops.def metadata
 * - build native inline commands for a small MVP subset only
 * - reject sync-only / payload / boundary-sensitive / not-yet-built ops
 */
bool lgfx_batch_build_inline_submit(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    term commands_t,
    lgfx_batch_build_t *out_build,
    term *out_error_reply);

#ifdef __cplusplus
}
#endif
