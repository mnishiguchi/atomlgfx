/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/handlers/primitives.c
#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"

typedef struct
{
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
} lgfx_triangle_i16_t;

typedef struct
{
    int16_t x;
    int16_t y;
    uint16_t r0;
    uint16_t r1;
    float angle0;
    float angle1;
} lgfx_arc_args_t;

typedef struct
{
    bool is_index;
    uint32_t value;
} lgfx_display_color_or_index_t;

static bool decode_triangle_i16(const lgfx_request_t *req, lgfx_triangle_i16_t *out)
{
    return lgfx_decode_i16_at(req, 5, &out->x0)
        && lgfx_decode_i16_at(req, 6, &out->y0)
        && lgfx_decode_i16_at(req, 7, &out->x1)
        && lgfx_decode_i16_at(req, 8, &out->y1)
        && lgfx_decode_i16_at(req, 9, &out->x2)
        && lgfx_decode_i16_at(req, 10, &out->y2);
}

static bool decode_arc_args(const lgfx_request_t *req, lgfx_arc_args_t *out)
{
    return out
        && lgfx_decode_i16_at(req, 5, &out->x)
        && lgfx_decode_i16_at(req, 6, &out->y)
        && lgfx_decode_u16_at(req, 7, &out->r0)
        && lgfx_decode_u16_at(req, 8, &out->r1)
        && lgfx_decode_f32_at(req, 9, &out->angle0)
        && lgfx_decode_f32_at(req, 10, &out->angle1);
}

static bool decode_primitive_display_color_or_index_at(
    const lgfx_request_t *req,
    int index,
    lgfx_display_color_or_index_t *out)
{
    return out
        && lgfx_decode_display_color_or_index_at(
            req,
            index,
            LGFX_F_COLOR_INDEX,
            &out->is_index,
            &out->value);
}

term lgfx_handle_fillScreen(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_display_color_or_index_t color = { 0 };

    if (!decode_primitive_display_color_or_index_at(req, 5, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_fill_screen(
            (uint8_t) req->target,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_clear(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_display_color_or_index_t color = { 0 };

    if (!decode_primitive_display_color_or_index_at(req, 5, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_clear(
            (uint8_t) req->target,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawPixel(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    lgfx_display_color_or_index_t color = { 0 };

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 7, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_pixel(
            (uint8_t) req->target,
            x,
            y,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawFastVLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t h = 0;
    lgfx_display_color_or_index_t color = { 0 };

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &h) || h == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 8, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_fast_vline(
            (uint8_t) req->target,
            x,
            y,
            h,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawFastHLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    lgfx_display_color_or_index_t color = { 0 };

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &w) || w == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 8, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_fast_hline(
            (uint8_t) req->target,
            x,
            y,
            w,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x0 = 0;
    int16_t y0 = 0;
    int16_t x1 = 0;
    int16_t y1 = 0;
    lgfx_display_color_or_index_t color = { 0 };

    if (!lgfx_decode_i16_at(req, 5, &x0)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y0)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 7, &x1)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 8, &y1)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 9, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_line(
            (uint8_t) req->target,
            x0,
            y0,
            x1,
            y1,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    lgfx_display_color_or_index_t color = { 0 };

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
    if (!decode_primitive_display_color_or_index_at(req, 9, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_rect(
            (uint8_t) req->target,
            x,
            y,
            w,
            h,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_fillRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    lgfx_display_color_or_index_t color = { 0 };

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
    if (!decode_primitive_display_color_or_index_at(req, 9, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_fill_rect(
            (uint8_t) req->target,
            x,
            y,
            w,
            h,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawRoundRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t r = 0;
    lgfx_display_color_or_index_t color = { 0 };

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
    if (!lgfx_decode_u16_at(req, 9, &r) || r == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 10, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_round_rect(
            (uint8_t) req->target,
            x,
            y,
            w,
            h,
            r,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_fillRoundRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t r = 0;
    lgfx_display_color_or_index_t color = { 0 };

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
    if (!lgfx_decode_u16_at(req, 9, &r) || r == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 10, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_fill_round_rect(
            (uint8_t) req->target,
            x,
            y,
            w,
            h,
            r,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawCircle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t r = 0;
    lgfx_display_color_or_index_t color = { 0 };

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &r) || r == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 8, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_circle(
            (uint8_t) req->target,
            x,
            y,
            r,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_fillCircle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t r = 0;
    lgfx_display_color_or_index_t color = { 0 };

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &r) || r == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 8, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_fill_circle(
            (uint8_t) req->target,
            x,
            y,
            r,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawEllipse(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t rx = 0;
    uint16_t ry = 0;
    lgfx_display_color_or_index_t color = { 0 };

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &rx) || rx == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 8, &ry) || ry == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 9, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_ellipse(
            (uint8_t) req->target,
            x,
            y,
            rx,
            ry,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_fillEllipse(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t rx = 0;
    uint16_t ry = 0;
    lgfx_display_color_or_index_t color = { 0 };

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &rx) || rx == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 8, &ry) || ry == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 9, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_fill_ellipse(
            (uint8_t) req->target,
            x,
            y,
            rx,
            ry,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawArc(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_arc_args_t arc = { 0 };
    lgfx_display_color_or_index_t color = { 0 };

    if (!decode_arc_args(req, &arc)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (arc.r0 == 0 || arc.r1 == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 11, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_arc(
            (uint8_t) req->target,
            arc.x,
            arc.y,
            arc.r0,
            arc.r1,
            arc.angle0,
            arc.angle1,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_fillArc(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_arc_args_t arc = { 0 };
    lgfx_display_color_or_index_t color = { 0 };

    if (!decode_arc_args(req, &arc)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (arc.r0 == 0 || arc.r1 == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!decode_primitive_display_color_or_index_at(req, 11, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_fill_arc(
            (uint8_t) req->target,
            arc.x,
            arc.y,
            arc.r0,
            arc.r1,
            arc.angle0,
            arc.angle1,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawBezier(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x0 = 0;
    int16_t y0 = 0;
    int16_t x1 = 0;
    int16_t y1 = 0;
    int16_t x2 = 0;
    int16_t y2 = 0;
    int16_t x3 = 0;
    int16_t y3 = 0;
    lgfx_display_color_or_index_t color = { 0 };

    if (!lgfx_decode_i16_at(req, 5, &x0)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y0)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 7, &x1)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 8, &y1)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 9, &x2)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 10, &y2)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (req->arity == 12) {
        if (!decode_primitive_display_color_or_index_at(req, 11, &color)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }

        LGFX_RETURN_IF_ESP_ERR(
            ctx,
            port,
            req,
            lgfx_device_draw_bezier3(
                (uint8_t) req->target,
                x0,
                y0,
                x1,
                y1,
                x2,
                y2,
                color.is_index,
                color.value));

        return reply_ok(ctx, port, req, port->atoms.ok);
    }

    if (req->arity == 14) {
        if (!lgfx_decode_i16_at(req, 11, &x3)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
        if (!lgfx_decode_i16_at(req, 12, &y3)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
        if (!decode_primitive_display_color_or_index_at(req, 13, &color)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }

        LGFX_RETURN_IF_ESP_ERR(
            ctx,
            port,
            req,
            lgfx_device_draw_bezier4(
                (uint8_t) req->target,
                x0,
                y0,
                x1,
                y1,
                x2,
                y2,
                x3,
                y3,
                color.is_index,
                color.value));

        return reply_ok(ctx, port, req, port->atoms.ok);
    }

    return reply_error(ctx, port, req, port->atoms.bad_args, 0);
}

term lgfx_handle_drawTriangle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_triangle_i16_t tri = { 0 };
    lgfx_display_color_or_index_t color = { 0 };

    if (!decode_triangle_i16(req, &tri)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!decode_primitive_display_color_or_index_at(req, 11, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_triangle(
            (uint8_t) req->target,
            tri.x0,
            tri.y0,
            tri.x1,
            tri.y1,
            tri.x2,
            tri.y2,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_fillTriangle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_triangle_i16_t tri = { 0 };
    lgfx_display_color_or_index_t color = { 0 };

    if (!decode_triangle_i16(req, &tri)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!decode_primitive_display_color_or_index_at(req, 11, &color)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_fill_triangle(
            (uint8_t) req->target,
            tri.x0,
            tri.y0,
            tri.x1,
            tri.y1,
            tri.x2,
            tri.y2,
            color.is_index,
            color.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}
