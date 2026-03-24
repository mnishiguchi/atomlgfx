/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/handlers/images.c
#include <stddef.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/proto_term.h"

#include "lgfx_port/ops.h"

term lgfx_handle_drawJpg(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, drawJpg, target, flags, X, Y, JpegBinary}
    // {lgfx, ver, drawJpg, target, flags, X, Y, MaxW, MaxH, OffX, OffY, ScaleX, ScaleY, JpegBinary}
    //
    // ScaleX / ScaleY use LovyanGFX-like numeric scale semantics on the wire.
    // Integer and float terms are both accepted by handler decode.
    //
    // Handler responsibility here is limited to wire decode.
    // Binary-size capping stays in lgfx_decode_binary_at().
    // Device code remains authoritative for JPEG decode/render behavior
    // and scale validity.

    if (req->arity != 8 && req->arity != 14) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    int16_t x = 0;
    int16_t y = 0;
    uint16_t max_w = 0;
    uint16_t max_h = 0;
    int16_t off_x = 0;
    int16_t off_y = 0;
    float scale_x = 1.0f;
    float scale_y = 1.0f;

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (req->arity == 14) {
        if (!lgfx_decode_u16_at(req, 7, &max_w)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
        if (!lgfx_decode_u16_at(req, 8, &max_h)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
        if (!lgfx_decode_i16_at(req, 9, &off_x)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
        if (!lgfx_decode_i16_at(req, 10, &off_y)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
        if (!lgfx_decode_f32_at(req, 11, &scale_x)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
        if (!lgfx_decode_f32_at(req, 12, &scale_y)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
    }

    const uint8_t *bytes = NULL;
    size_t len = 0;
    if (!lgfx_decode_binary_at(req, req->arity - 1, &bytes, &len)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_jpg(
            (uint8_t) req->target,
            x,
            y,
            max_w,
            max_h,
            off_x,
            off_y,
            scale_x,
            scale_y,
            bytes,
            len));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_pushImage(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, pushImage, target, flags, X, Y, W, H, StridePixels, DataRgb565Binary}
    //
    // DataRgb565Binary carries ordinary RGB565 image data encoded as little-endian
    // 16-bit words. Target-specific byte swapping remains controlled by
    // setSwapBytes(Target, Bool).
    //
    // Handler responsibility here is limited to wire decode.
    // Binary-size capping stays in lgfx_decode_binary_at().
    // Device code remains authoritative for:
    // - stride == 0 normalization
    // - stride >= w
    // - even byte length
    // - overflow / required byte count
    // - insufficient payload size

    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t stride = 0;

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &w)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 8, &h)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 9, &stride)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const uint8_t *bytes = NULL;
    size_t len = 0;
    if (!lgfx_decode_binary_at(req, 10, &bytes, &len)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_push_image_rgb565_strided(
            (uint8_t) req->target,
            x,
            y,
            w,
            h,
            stride,
            bytes,
            len));

    return reply_ok(ctx, port, req, port->atoms.ok);
}
