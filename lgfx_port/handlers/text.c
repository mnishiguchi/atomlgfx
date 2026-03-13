// lgfx_port/handlers/text.c
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "context.h"
#include "term.h"

// Device-side preset IDs live in lgfx_device.h so handlers and the device ABI
// stay aligned.
#include "lgfx_device.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/worker.h"

static bool decode_font_preset(term preset_t, uint8_t *out_preset)
{
    if (!out_preset) {
        return false;
    }

    // MVP wire form: integer only
    // 0=ascii, 1=jp_small, 2=jp_medium, 3=jp_large
    uint32_t v = 0;
    if (!lgfx_term_to_u32(preset_t, &v) || v > (uint32_t) LGFX_FONT_PRESET_JP_LARGE) {
        return false;
    }

    *out_preset = (uint8_t) v;
    return true;
}

term lgfx_handle_setTextSize(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    const int arity = req->arity;

    if (arity == 6) {
        uint32_t size = 0;
        if (!lgfx_decode_u32_at(req, 5, &size) || size == 0 || size > 255u) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }

        LGFX_RETURN_IF_ESP_ERR(
            ctx,
            port,
            req,
            lgfx_worker_device_set_text_size(port, (uint8_t) req->target, (uint8_t) size));

        return reply_ok(ctx, port, req, port->atoms.ok);
    }

    uint32_t sx = 0;
    uint32_t sy = 0;

    if (!lgfx_decode_u32_at(req, 5, &sx) || sx == 0 || sx > 255u) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!lgfx_decode_u32_at(req, 6, &sy) || sy == 0 || sy > 255u) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_text_size_xy(
            port,
            (uint8_t) req->target,
            (uint8_t) sx,
            (uint8_t) sy));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setTextDatum(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint32_t datum = 0;
    if (!lgfx_decode_u32_at(req, 5, &datum) || datum > 255u) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_text_datum(port, (uint8_t) req->target, (uint8_t) datum));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setTextWrap(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    const int arity = req->arity;

    term wrap_x_t = lgfx_req_elem(req, 5);

    bool wrap_x = false;
    if (!lgfx_decode_bool_term(port, wrap_x_t, &wrap_x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    bool wrap_y = wrap_x;
    if (arity == 7) {
        term wrap_y_t = lgfx_req_elem(req, 6);
        if (!lgfx_decode_bool_term(port, wrap_y_t, &wrap_y)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_text_wrap_xy(port, (uint8_t) req->target, wrap_x, wrap_y));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setTextFont(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint32_t font = 0;
    if (!lgfx_decode_u32_at(req, 5, &font) || font > 255u) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_text_font(port, (uint8_t) req->target, (uint8_t) font));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setFontPreset(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term preset_t = lgfx_req_elem(req, 5);

    uint8_t preset = 0;
    if (!decode_font_preset(preset_t, &preset)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_font_preset(port, (uint8_t) req->target, preset));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setTextColor(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    bool has_bg = ((req->flags & LGFX_F_TEXT_HAS_BG) != 0);

    uint16_t fg565 = 0;
    if (!lgfx_decode_color565_at(req, 5, &fg565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    uint16_t bg565 = 0;
    if (has_bg) {
        if (!lgfx_decode_color565_at(req, 6, &bg565)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_text_color(
            port,
            (uint8_t) req->target,
            fg565,
            has_bg,
            bg565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_drawString(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const uint8_t *bytes = NULL;
    size_t len = 0;
    if (!lgfx_decode_binary_at(req, 7, &bytes, &len)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (len == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (memchr(bytes, 0, len) != NULL) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_draw_string(
            port,
            (uint8_t) req->target,
            x,
            y,
            bytes,
            len));

    return reply_ok(ctx, port, req, port->atoms.ok);
}
