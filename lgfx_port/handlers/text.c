// lgfx_port/handlers/text.c
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/worker.h"

typedef struct
{
    bool is_index;
    uint32_t value;
} lgfx_wire_color_t;

static bool decode_font_preset(term preset_t, uint8_t *out_preset)
{
    if (!out_preset) {
        return false;
    }

    // Wire form is integer-only.
    // Preset IDs are protocol-owned constants from include/lgfx_port/lgfx_port.h:
    // 0=ascii, 1=jp_small, 2=jp_medium, 3=jp_large
    uint32_t value = 0;
    if (!lgfx_term_to_u32(preset_t, &value) || value > (uint32_t) LGFX_FONT_PRESET_JP_LARGE) {
        return false;
    }

    *out_preset = (uint8_t) value;
    return true;
}

static bool decode_u8_passthrough_at(const lgfx_request_t *req, int index, uint8_t *out_value)
{
    if (!req || !out_value) {
        return false;
    }

    uint32_t value = 0;
    if (!lgfx_decode_u32_at(req, index, &value) || value > 255u) {
        return false;
    }

    *out_value = (uint8_t) value;
    return true;
}

static bool decode_text_color_at(
    const lgfx_request_t *req,
    int index,
    uint32_t index_flag,
    lgfx_wire_color_t *out)
{
    return out
        && lgfx_decode_color_or_index_at(
            req,
            index,
            index_flag,
            &out->is_index,
            &out->value);
}

term lgfx_handle_setTextSize(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    const int arity = req->arity;

    if (arity == 6) {
        uint16_t scale_x256 = 0;
        if (!lgfx_decode_text_scale_x256_at(req, 5, &scale_x256)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }

        LGFX_RETURN_IF_ESP_ERR(
            ctx,
            port,
            req,
            lgfx_worker_device_set_text_size(
                port,
                (uint8_t) req->target,
                scale_x256));

        return reply_ok(ctx, port, req, port->atoms.ok);
    }

    uint16_t scale_x_x256 = 0;
    uint16_t scale_y_x256 = 0;

    if (!lgfx_decode_text_scale_x256_at(req, 5, &scale_x_x256)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!lgfx_decode_text_scale_x256_at(req, 6, &scale_y_x256)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_text_size_xy(
            port,
            (uint8_t) req->target,
            scale_x_x256,
            scale_y_x256));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setTextDatum(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint8_t datum = 0;
    if (!decode_u8_passthrough_at(req, 5, &datum)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_text_datum(port, (uint8_t) req->target, datum));

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

    // LovyanGFX semantics:
    // - setTextWrap(wrap_x) => wrap_y defaults to false
    // - setTextWrap(wrap_x, wrap_y)
    bool wrap_y = false;
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
    uint8_t font = 0;
    if (!decode_u8_passthrough_at(req, 5, &font)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_text_font(port, (uint8_t) req->target, font));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setTextFontPreset(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
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
        lgfx_worker_device_set_text_font_preset(port, (uint8_t) req->target, preset));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setTextColor(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    const bool has_bg = lgfx_req_has_flag(req, LGFX_F_TEXT_HAS_BG);
    const bool bg_is_index_flag = lgfx_req_has_flag(req, LGFX_F_TEXT_BG_INDEX);

    if ((has_bg && req->arity != 7) || (!has_bg && req->arity != 6)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (bg_is_index_flag && !has_bg) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    lgfx_wire_color_t fg = { 0 };
    if (!decode_text_color_at(req, 5, LGFX_F_TEXT_FG_INDEX, &fg)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    lgfx_wire_color_t bg = { 0 };
    if (has_bg) {
        if (!decode_text_color_at(req, 6, LGFX_F_TEXT_BG_INDEX, &bg)) {
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
            fg.is_index,
            fg.value,
            has_bg,
            bg.is_index,
            bg.value));

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
