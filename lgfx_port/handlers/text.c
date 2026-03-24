/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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

typedef struct
{
    bool is_index;
    uint32_t value;
} lgfx_display_color_or_index_t;

static bool decode_font_preset(term preset_t, uint8_t *out_preset)
{
    if (!out_preset) {
        return false;
    }

    // Wire form is integer-only.
    // Preset IDs are protocol-owned constants from include/lgfx_port/lgfx_port.h:
    // 0=ascii, 1=jp
    uint32_t value = 0;
    if (!lgfx_term_to_u32(preset_t, &value) || value > (uint32_t) LGFX_FONT_PRESET_JP) {
        return false;
    }

    *out_preset = (uint8_t) value;
    return true;
}

static bool decode_text_color_at(
    const lgfx_request_t *req,
    int index,
    uint32_t index_flag,
    lgfx_display_color_or_index_t *out)
{
    return out
        && lgfx_decode_display_color_or_index_at(
            req,
            index,
            index_flag,
            &out->is_index,
            &out->value);
}

static bool decode_text_binary_at(
    const lgfx_request_t *req,
    int index,
    const uint8_t **out_bytes,
    size_t *out_len,
    bool allow_empty)
{
    if (!lgfx_decode_binary_at(req, index, out_bytes, out_len)) {
        return false;
    }

    if (!allow_empty && *out_len == 0) {
        return false;
    }

    if (*out_len > 0 && memchr(*out_bytes, 0, *out_len) != NULL) {
        return false;
    }

    return true;
}

static term reply_cursor_xy(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *req,
    int32_t x,
    int32_t y)
{
    term result = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(result, 0, term_from_int(x));
    term_put_tuple_element(result, 1, term_from_int(y));
    return reply_ok(ctx, port, req, result);
}

term lgfx_handle_setTextSize(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    const int arity = req->arity;

    if (arity == 6) {
        float scale = 0.0f;
        if (!lgfx_decode_f32_at(req, 5, &scale)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }

        LGFX_RETURN_IF_ESP_ERR(
            ctx,
            port,
            req,
            lgfx_device_set_text_size(
                (uint8_t) req->target,
                scale));

        return reply_ok(ctx, port, req, port->atoms.ok);
    }

    float scale_x = 0.0f;
    float scale_y = 0.0f;

    if (!lgfx_decode_f32_at(req, 5, &scale_x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!lgfx_decode_f32_at(req, 6, &scale_y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_set_text_size_xy(
            (uint8_t) req->target,
            scale_x,
            scale_y));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setTextDatum(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint8_t datum = 0;
    if (!lgfx_decode_u8_at(req, 5, &datum)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_set_text_datum((uint8_t) req->target, datum));

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
        lgfx_device_set_text_wrap((uint8_t) req->target, wrap_x, wrap_y));

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
        lgfx_device_set_text_font_preset((uint8_t) req->target, preset));

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

    lgfx_display_color_or_index_t fg = { 0 };
    if (!decode_text_color_at(req, 5, LGFX_F_TEXT_FG_INDEX, &fg)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    lgfx_display_color_or_index_t bg = { 0 };
    if (has_bg) {
        if (!decode_text_color_at(req, 6, LGFX_F_TEXT_BG_INDEX, &bg)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_set_text_color(
            (uint8_t) req->target,
            fg.is_index,
            fg.value,
            has_bg,
            bg.is_index,
            bg.value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setCursor(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int16_t x = 0;
    int16_t y = 0;

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_set_cursor(
            (uint8_t) req->target,
            x,
            y));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_getCursor(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    int32_t x = 0;
    int32_t y = 0;

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_get_cursor(
            (uint8_t) req->target,
            &x,
            &y));

    return reply_cursor_xy(ctx, port, req, x, y);
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
    if (!decode_text_binary_at(req, 7, &bytes, &len, false)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_draw_string(
            (uint8_t) req->target,
            x,
            y,
            bytes,
            len));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_print(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    const uint8_t *bytes = NULL;
    size_t len = 0;

    if (!decode_text_binary_at(req, 5, &bytes, &len, true)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_print(
            (uint8_t) req->target,
            bytes,
            len));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_println(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    const uint8_t *bytes = NULL;
    size_t len = 0;

    if (!decode_text_binary_at(req, 5, &bytes, &len, true)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_println(
            (uint8_t) req->target,
            bytes,
            len));

    return reply_ok(ctx, port, req, port->atoms.ok);
}
