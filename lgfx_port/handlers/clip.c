/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/handlers/clip.c
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"

term lgfx_handle_setClipRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &w) || w == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 8, &h) || h == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_set_clip_rect((uint8_t) req->target, x, y, w, h));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_clearClipRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_clear_clip_rect((uint8_t) req->target));

    return reply_ok(ctx, port, req, port->atoms.ok);
}
