// lgfx_port/handlers/primitives.c
#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/worker.h"

typedef struct
{
    int16_t x0;
    int16_t y0;
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
} lgfx_triangle_i16_t;

static bool decode_triangle_i16(const lgfx_request_t *req, lgfx_triangle_i16_t *out)
{
    return lgfx_decode_i16_at(req, 5, &out->x0)
        && lgfx_decode_i16_at(req, 6, &out->y0)
        && lgfx_decode_i16_at(req, 7, &out->x1)
        && lgfx_decode_i16_at(req, 8, &out->y1)
        && lgfx_decode_i16_at(req, 9, &out->x2)
        && lgfx_decode_i16_at(req, 10, &out->y2);
}

term lgfx_handle_fillScreen(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint16_t color565 = 0;
    if (!lgfx_decode_color565_at(req, 5, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_fill_screen(port, (uint8_t) req->target, color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_clear(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint16_t color565 = 0;
    if (!lgfx_decode_color565_at(req, 5, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_clear(port, (uint8_t) req->target, color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawPixel(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t color565 = 0;

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_color565_at(req, 7, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_draw_pixel(
            port,
            (uint8_t) req->target,
            x,
            y,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawFastVLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t h = 0;
    uint16_t color565 = 0;

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &h) || h == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_color565_at(req, 8, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_draw_fast_vline(
            port,
            (uint8_t) req->target,
            x,
            y,
            h,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawFastHLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t color565 = 0;

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &w) || w == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_color565_at(req, 8, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_draw_fast_hline(
            port,
            (uint8_t) req->target,
            x,
            y,
            w,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x0 = 0;
    int16_t y0 = 0;
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t color565 = 0;

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
    if (!lgfx_decode_color565_at(req, 9, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_draw_line(
            port,
            (uint8_t) req->target,
            x0,
            y0,
            x1,
            y1,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t color565 = 0;

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
    if (!lgfx_decode_color565_at(req, 9, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_draw_rect(
            port,
            (uint8_t) req->target,
            x,
            y,
            w,
            h,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_fillRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t color565 = 0;

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
    if (!lgfx_decode_color565_at(req, 9, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_fill_rect(
            port,
            (uint8_t) req->target,
            x,
            y,
            w,
            h,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawCircle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t r = 0;
    uint16_t color565 = 0;

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &r) || r == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_color565_at(req, 8, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_draw_circle(
            port,
            (uint8_t) req->target,
            x,
            y,
            r,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_fillCircle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t r = 0;
    uint16_t color565 = 0;

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &r) || r == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_color565_at(req, 8, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_fill_circle(
            port,
            (uint8_t) req->target,
            x,
            y,
            r,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawTriangle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_triangle_i16_t tri = { 0 };
    uint16_t color565 = 0;

    if (!decode_triangle_i16(req, &tri)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!lgfx_decode_color565_at(req, 11, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_draw_triangle(
            port,
            (uint8_t) req->target,
            tri.x0, tri.y0,
            tri.x1, tri.y1,
            tri.x2, tri.y2,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_fillTriangle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_triangle_i16_t tri = { 0 };
    uint16_t color565 = 0;

    if (!decode_triangle_i16(req, &tri)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!lgfx_decode_color565_at(req, 11, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_fill_triangle(
            port,
            (uint8_t) req->target,
            tri.x0, tri.y0,
            tri.x1, tri.y1,
            tri.x2, tri.y2,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}
