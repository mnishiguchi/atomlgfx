// ports/handlers/text.c
#include "lgfx_port/handlers/text.h"

#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

// NOTE: device calls now go through the worker wrappers.
#include "lgfx_port/worker.h"

#include "lgfx_port/color.h"
#include "lgfx_port/handler_common.h"
#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_conv.h"
#include "lgfx_port/term_decode.h"
#include "lgfx_port/term_encode.h"
#include "lgfx_port/validate.h"

// Envelope checks (version/arity/flags/target/init-state) are centralized in
// lgfx_port.c via ops.def metadata. Handlers here only decode payload fields.

static term do_set_text_size(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, setTextSize, target, flags, Size}
    term size_t = term_get_tuple_element(req->request_tuple, 5);

    uint32_t size = 0;
    if (!lgfx_term_to_u32(size_t, &size) || size == 0 || size > 255) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_set_text_size(port, (uint8_t) req->target, (uint8_t) size));

    return lgfx_reply_ok(ctx, port, port->atoms.ok); // {ok, ok}
}

static term do_set_text_datum(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, setTextDatum, target, flags, Datum}
    term datum_t = term_get_tuple_element(req->request_tuple, 5);

    uint32_t datum = 0;
    if (!lgfx_term_to_u32(datum_t, &datum) || datum > 255) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_set_text_datum(port, (uint8_t) req->target, (uint8_t) datum));

    return lgfx_reply_ok(ctx, port, port->atoms.ok); // {ok, ok}
}

static term do_set_text_wrap(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, setTextWrap, target, flags, WrapBool}
    term wrap_t = term_get_tuple_element(req->request_tuple, 5);

    bool wrap = false;

    // Prefer atom true/false if available; otherwise accept 0/1.
    if (term_is_atom(wrap_t)) {
        if (wrap_t == port->atoms.true_) {
            wrap = true;
        } else if (wrap_t == port->atoms.false_) {
            wrap = false;
        } else {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
    } else {
        uint32_t v = 0;
        if (!lgfx_term_to_u32(wrap_t, &v) || (v != 0 && v != 1)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
        wrap = (v == 1);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_set_text_wrap(port, (uint8_t) req->target, wrap));

    return lgfx_reply_ok(ctx, port, port->atoms.ok); // {ok, ok}
}

static term do_set_text_font(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, setTextFont, target, flags, FontId}
    term font_t = term_get_tuple_element(req->request_tuple, 5);

    uint32_t font = 0;
    if (!lgfx_term_to_u32(font_t, &font) || font > 255) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_set_text_font(port, (uint8_t) req->target, (uint8_t) font));

    return lgfx_reply_ok(ctx, port, port->atoms.ok); // {ok, ok}
}

static term do_set_text_color(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // setTextColor(Fg) or setTextColor(Fg, Bg) depending on F_TEXT_HAS_BG.
    // Shared dispatch validates flags + variable arity for this opcode.

    bool has_bg = ((req->flags & LGFX_F_TEXT_HAS_BG) != 0);

    term fg_t = term_get_tuple_element(req->request_tuple, 5);
    uint32_t fg888 = 0;

    if (!lgfx_term_to_u32(fg_t, &fg888) || !lgfx_validate_color888(fg888)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    uint32_t bg888 = 0;
    if (has_bg) {
        term bg_t = term_get_tuple_element(req->request_tuple, 6);
        if (!lgfx_term_to_u32(bg_t, &bg888) || !lgfx_validate_color888(bg888)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
    }

    uint16_t fg565 = lgfx_color888_to_rgb565(fg888);
    uint16_t bg565 = lgfx_color888_to_rgb565(bg888);

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_set_text_color(
            port,
            (uint8_t) req->target,
            fg565,
            has_bg,
            bg565));

    return lgfx_reply_ok(ctx, port, port->atoms.ok); // {ok, ok}
}

static term do_draw_string(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // drawString(X, Y, TextUtf8Binary)
    term x_t = term_get_tuple_element(req->request_tuple, 5);
    term y_t = term_get_tuple_element(req->request_tuple, 6);
    term text_t = term_get_tuple_element(req->request_tuple, 7);

    int32_t x = 0;
    int32_t y = 0;

    if (!lgfx_term_to_i32(x_t, &x) || !lgfx_validate_i16(x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i32(y_t, &y) || !lgfx_validate_i16(y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!term_is_binary(text_t)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const uint8_t *bytes = (const uint8_t *) term_binary_data(text_t);
    uint32_t len32 = (uint32_t) term_binary_size(text_t);

    // Device adapter clamps at 255; enforce here for predictable protocol behavior.
    if (len32 == 0 || len32 > 255u || len32 > (uint32_t) LGFX_PORT_MAX_BINARY_BYTES) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    // drawString is backed by a C-string API (NUL-terminated).
    // Reject embedded NUL to avoid silent truncation.
    for (uint32_t i = 0; i < len32; i++) {
        if (bytes[i] == 0) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_draw_string(
            port,
            (uint8_t) req->target,
            (int16_t) x,
            (int16_t) y,
            bytes,
            (uint16_t) len32));

    return lgfx_reply_ok(ctx, port, port->atoms.ok); // {ok, ok}
}

term lgfx_handle_setTextSize(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_set_text_size(ctx, port, req);
}

term lgfx_handle_setTextDatum(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_set_text_datum(ctx, port, req);
}

term lgfx_handle_setTextWrap(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_set_text_wrap(ctx, port, req);
}

term lgfx_handle_setTextFont(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_set_text_font(ctx, port, req);
}

term lgfx_handle_setTextColor(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_set_text_color(ctx, port, req);
}

term lgfx_handle_drawString(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_draw_string(ctx, port, req);
}
