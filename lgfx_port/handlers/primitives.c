// lgfx_port/handlers/primitives.c
#include <stdbool.h>
#include <stdint.h>

#include "term.h"

#include "lgfx_port/ops.h"
#include "lgfx_port/worker.h"

// Request envelope validation (version/arity/flags/target/init-state) is
// centralized in lgfx_port.c via ops.def metadata. Handlers only decode payload fields.

static term do_fill_screen(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term color_t = term_get_tuple_element(req->request_tuple, 5);

    uint16_t color565 = 0;
    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_fill_screen(port, (uint8_t) req->target, color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_clear(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term color_t = term_get_tuple_element(req->request_tuple, 5);

    uint16_t color565 = 0;
    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_clear(port, (uint8_t) req->target, color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_draw_pixel(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term x_t = term_get_tuple_element(req->request_tuple, 5);
    term y_t = term_get_tuple_element(req->request_tuple, 6);
    term color_t = term_get_tuple_element(req->request_tuple, 7);

    int16_t x = 0;
    int16_t y = 0;
    uint16_t color565 = 0;

    if (!lgfx_term_to_i16(x_t, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i16(y_t, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_draw_pixel(
            port,
            (uint8_t) req->target,
            x,
            y,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_draw_fast_vline(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term x_t = term_get_tuple_element(req->request_tuple, 5);
    term y_t = term_get_tuple_element(req->request_tuple, 6);
    term h_t = term_get_tuple_element(req->request_tuple, 7);
    term color_t = term_get_tuple_element(req->request_tuple, 8);

    int16_t x = 0;
    int16_t y = 0;
    uint16_t h = 0;
    uint16_t color565 = 0;

    if (!lgfx_term_to_i16(x_t, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i16(y_t, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_u16(h_t, &h) || h == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_draw_fast_vline(
            port,
            (uint8_t) req->target,
            x,
            y,
            h,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_draw_fast_hline(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term x_t = term_get_tuple_element(req->request_tuple, 5);
    term y_t = term_get_tuple_element(req->request_tuple, 6);
    term w_t = term_get_tuple_element(req->request_tuple, 7);
    term color_t = term_get_tuple_element(req->request_tuple, 8);

    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t color565 = 0;

    if (!lgfx_term_to_i16(x_t, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i16(y_t, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_u16(w_t, &w) || w == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_draw_fast_hline(
            port,
            (uint8_t) req->target,
            x,
            y,
            w,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_draw_line(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term x0_t = term_get_tuple_element(req->request_tuple, 5);
    term y0_t = term_get_tuple_element(req->request_tuple, 6);
    term x1_t = term_get_tuple_element(req->request_tuple, 7);
    term y1_t = term_get_tuple_element(req->request_tuple, 8);
    term color_t = term_get_tuple_element(req->request_tuple, 9);

    int16_t x0 = 0;
    int16_t y0 = 0;
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t color565 = 0;

    if (!lgfx_term_to_i16(x0_t, &x0)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i16(y0_t, &y0)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i16(x1_t, &x1)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i16(y1_t, &y1)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
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

static term do_draw_rect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term x_t = term_get_tuple_element(req->request_tuple, 5);
    term y_t = term_get_tuple_element(req->request_tuple, 6);
    term w_t = term_get_tuple_element(req->request_tuple, 7);
    term h_t = term_get_tuple_element(req->request_tuple, 8);
    term color_t = term_get_tuple_element(req->request_tuple, 9);

    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t color565 = 0;

    if (!lgfx_term_to_i16(x_t, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i16(y_t, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_u16(w_t, &w) || w == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_u16(h_t, &h) || h == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
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

static term do_fill_rect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term x_t = term_get_tuple_element(req->request_tuple, 5);
    term y_t = term_get_tuple_element(req->request_tuple, 6);
    term w_t = term_get_tuple_element(req->request_tuple, 7);
    term h_t = term_get_tuple_element(req->request_tuple, 8);
    term color_t = term_get_tuple_element(req->request_tuple, 9);

    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t color565 = 0;

    if (!lgfx_term_to_i16(x_t, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i16(y_t, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_u16(w_t, &w) || w == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_u16(h_t, &h) || h == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
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

static term do_draw_circle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term x_t = term_get_tuple_element(req->request_tuple, 5);
    term y_t = term_get_tuple_element(req->request_tuple, 6);
    term r_t = term_get_tuple_element(req->request_tuple, 7);
    term color_t = term_get_tuple_element(req->request_tuple, 8);

    int16_t x = 0;
    int16_t y = 0;
    uint16_t r = 0;
    uint16_t color565 = 0;

    if (!lgfx_term_to_i16(x_t, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i16(y_t, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_u16(r_t, &r) || r == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_draw_circle(
            port,
            (uint8_t) req->target,
            x,
            y,
            r,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_fill_circle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term x_t = term_get_tuple_element(req->request_tuple, 5);
    term y_t = term_get_tuple_element(req->request_tuple, 6);
    term r_t = term_get_tuple_element(req->request_tuple, 7);
    term color_t = term_get_tuple_element(req->request_tuple, 8);

    int16_t x = 0;
    int16_t y = 0;
    uint16_t r = 0;
    uint16_t color565 = 0;

    if (!lgfx_term_to_i16(x_t, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i16(y_t, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_u16(r_t, &r) || r == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_fill_circle(
            port,
            (uint8_t) req->target,
            x,
            y,
            r,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

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
    term x0_t = term_get_tuple_element(req->request_tuple, 5);
    term y0_t = term_get_tuple_element(req->request_tuple, 6);
    term x1_t = term_get_tuple_element(req->request_tuple, 7);
    term y1_t = term_get_tuple_element(req->request_tuple, 8);
    term x2_t = term_get_tuple_element(req->request_tuple, 9);
    term y2_t = term_get_tuple_element(req->request_tuple, 10);

    return lgfx_term_to_i16(x0_t, &out->x0)
        && lgfx_term_to_i16(y0_t, &out->y0)
        && lgfx_term_to_i16(x1_t, &out->x1)
        && lgfx_term_to_i16(y1_t, &out->y1)
        && lgfx_term_to_i16(x2_t, &out->x2)
        && lgfx_term_to_i16(y2_t, &out->y2);
}

static term do_draw_triangle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term color_t = term_get_tuple_element(req->request_tuple, 11);

    lgfx_triangle_i16_t tri = { 0 };
    uint16_t color565 = 0;

    if (!decode_triangle_i16(req, &tri)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_draw_triangle(
            port,
            (uint8_t) req->target,
            tri.x0, tri.y0,
            tri.x1, tri.y1,
            tri.x2, tri.y2,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_fill_triangle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term color_t = term_get_tuple_element(req->request_tuple, 11);

    lgfx_triangle_i16_t tri = { 0 };
    uint16_t color565 = 0;

    if (!decode_triangle_i16(req, &tri)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!lgfx_term_to_color565(color_t, &color565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_fill_triangle(
            port,
            (uint8_t) req->target,
            tri.x0, tri.y0,
            tri.x1, tri.y1,
            tri.x2, tri.y2,
            color565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_fillScreen(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_fill_screen(ctx, port, req);
}

term lgfx_handle_clear(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_clear(ctx, port, req);
}

term lgfx_handle_drawPixel(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_draw_pixel(ctx, port, req);
}

term lgfx_handle_drawFastVLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_draw_fast_vline(ctx, port, req);
}

term lgfx_handle_drawFastHLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_draw_fast_hline(ctx, port, req);
}

term lgfx_handle_drawLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_draw_line(ctx, port, req);
}

term lgfx_handle_drawRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_draw_rect(ctx, port, req);
}

term lgfx_handle_fillRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_fill_rect(ctx, port, req);
}

term lgfx_handle_drawCircle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_draw_circle(ctx, port, req);
}

term lgfx_handle_fillCircle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_fill_circle(ctx, port, req);
}

term lgfx_handle_drawTriangle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_draw_triangle(ctx, port, req);
}

term lgfx_handle_fillTriangle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_fill_triangle(ctx, port, req);
}
