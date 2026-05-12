/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/include_internal/lgfx_port/handlers/control.inc
//
// Control-plane handlers:
// - ping / getCaps / getLastError / submitBinaryBatch
// - init / close
// - startWrite / endWrite
#include <stdint.h>

#include "context.h"
#include "term.h"
#include "port.h"

#include "esp_err.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/render_batch_dispatch.h"
#include "lgfx_port/protocol.h"

term lgfx_handle_ping(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return reply_ok(ctx, port, req, port->atoms.pong);
}


static term lgfx_reply_render_batch_failed(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *req,
    uint32_t failed_index,
    uint8_t failed_opcode,
    term mapped_reason,
    int32_t esp_err)
{
    term detail_elems[3];
    detail_elems[0] = term_from_int32((int32_t) failed_index);
    detail_elems[1] = term_from_int32((int32_t) failed_opcode);
    detail_elems[2] = mapped_reason;

    term detail = lgfx_make_tuple(ctx, 3, detail_elems);
    if (term_is_invalid_term(detail)) {
        return lgfx_reply_error_detail_req(
            ctx,
            port,
            req,
            port->atoms.no_memory,
            term_from_int32((int32_t) ESP_ERR_NO_MEM),
            (int32_t) ESP_ERR_NO_MEM);
    }

    return lgfx_reply_error_detail_req(
        ctx,
        port,
        req,
        port->atoms.batch_failed,
        detail,
        esp_err);
}

term lgfx_handle_submitBinaryBatch(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    /*
     * Render batch is the multi-target frame-script path for drawing bursts.
     * It accepts exactly one borrowed binary payload and executes it under one
     * native lock/write session. The request target seeds the initial batch
     * target; the command stream can still change targets explicitly.
     */
    const uint8_t *bytes = NULL;
    size_t len = 0u;

    if (!lgfx_decode_binary_at(req, 5, &bytes, &len) || len == 0u) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    uint32_t failed_index = 0u;
    uint8_t failed_opcode = 0u;
    bool malformed_command = false;

    esp_err_t execute_err = lgfx_render_batch_dispatch_run(
        bytes,
        len,
        (uint8_t) req->target,
        &failed_index,
        &failed_opcode,
        &malformed_command);

    if (execute_err == ESP_ERR_NOT_SUPPORTED) {
        return lgfx_reply_render_batch_failed(
            ctx,
            port,
            req,
            failed_index,
            failed_opcode,
            port->atoms.bad_op,
            (int32_t) execute_err);
    }

    if (malformed_command) {
        return lgfx_reply_render_batch_failed(
            ctx,
            port,
            req,
            failed_index,
            failed_opcode,
            port->atoms.bad_args,
            (int32_t) execute_err);
    }

    if (execute_err != ESP_OK) {
        term mapped_reason = port->atoms.internal;
        term maybe_reply = lgfx_reply_from_esp_err(ctx, port, execute_err);
        if (!term_is_invalid_term(maybe_reply)) {
            term extracted = port->atoms.internal;
            if (lgfx_is_error_reply(ctx, port, maybe_reply, &extracted)) {
                mapped_reason = extracted;
            }
        }

        return lgfx_reply_render_batch_failed(
            ctx,
            port,
            req,
            failed_index,
            failed_opcode,
            mapped_reason,
            (int32_t) execute_err);
    }

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_getCaps(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint32_t feature_bits = lgfx_port_feature_bits(port);
    return reply_ok(ctx, port, req, term_from_int32((int32_t) feature_bits));
}

term lgfx_handle_getLastError(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_last_error_t e = port->last_error;

    term elems[6] = {
        port->atoms.last_error,
        e.last_op,
        e.reason,
        term_from_int32((int32_t) e.flags),
        term_from_int32((int32_t) e.target),
        term_from_int32((int32_t) e.esp_err)
    };

    term payload = lgfx_make_tuple(ctx, 6, elems);
    if (term_is_invalid_term(payload)) {
        return reply_error(ctx, port, req, port->atoms.no_memory, (int32_t) ESP_ERR_NO_MEM);
    }

    term reply = reply_ok(ctx, port, req, payload);
    if (term_is_invalid_term(reply)) {
        return reply;
    }

    lgfx_last_error_clear(port);
    return reply;
}

term lgfx_handle_init(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (port->initialized) {
        return reply_ok(ctx, port, req, port->atoms.ok);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_init_with_open_config(&port->open_config_overrides, (const void *) port));

    port->initialized = true;
    lgfx_last_error_clear(port);
    lgfx_refresh_cached_dims(port);

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_close(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (!port->initialized) {
        lgfx_last_error_clear(port);
        return reply_ok(ctx, port, req, port->atoms.ok);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_close_for_owner((const void *) port));

    port->initialized = false;
    port->width = 0;
    port->height = 0;

    lgfx_last_error_clear(port);
    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_startWrite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_device_start_write());
    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_endWrite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_device_end_write());
    return reply_ok(ctx, port, req, port->atoms.ok);
}
