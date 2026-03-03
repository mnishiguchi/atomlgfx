// lgfx_port/handlers/text.c
#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

// Device-side preset IDs live in the device ABI header.
// This keeps the preset enum stable across:
// - ports/handlers/text.c (decode/range checks)
// - ports/lgfx_worker.c (job payload)
// - src/lgfx_device_api.cpp (mapping to LovyanGFX fonts)
#include "lgfx_device.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/worker.h"

// Request envelope validation (version/arity/flags/target/init-state) is
// centralized in lgfx_port.c via ops.def metadata. Handlers only decode payload fields.

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

static bool decode_bool_term(const lgfx_port_t *port, term t, bool *out_value)
{
    if (!port || !out_value) {
        return false;
    }

    // Prefer atom true/false if available; otherwise accept 0/1.
    if (term_is_atom(t)) {
        if (t == port->atoms.true_) {
            *out_value = true;
            return true;
        }
        if (t == port->atoms.false_) {
            *out_value = false;
            return true;
        }
        return false;
    }

    uint32_t v = 0;
    if (!lgfx_term_to_u32(t, &v) || (v != 0 && v != 1)) {
        return false;
    }

    *out_value = (v == 1);
    return true;
}

static term do_set_text_size(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // arity 6: {lgfx, ver, setTextSize, target, flags, Size}
    // arity 7: {lgfx, ver, setTextSize, target, flags, SizeX, SizeY}
    const int arity = term_get_tuple_arity(req->request_tuple);

    if (arity == 6) {
        term size_term = term_get_tuple_element(req->request_tuple, 5);

        uint32_t size = 0;
        if (!lgfx_term_to_u32(size_term, &size) || size == 0 || size > 255) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }

        LGFX_RETURN_IF_ESP_ERR(
            ctx,
            port,
            req,
            lgfx_worker_device_set_text_size(port, (uint8_t) req->target, (uint8_t) size));

        return reply_ok(ctx, port, req, port->atoms.ok); // {ok, ok}
    }

    // arity == 7 (enforced by ops.def validation)
    term sx_term = term_get_tuple_element(req->request_tuple, 5);
    term sy_term = term_get_tuple_element(req->request_tuple, 6);

    uint32_t sx = 0;
    uint32_t sy = 0;

    if (!lgfx_term_to_u32(sx_term, &sx) || sx == 0 || sx > 255) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    // sy==0 is allowed (means "same as x" per device ABI behavior)
    if (!lgfx_term_to_u32(sy_term, &sy) || sy > 255) {
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

    return reply_ok(ctx, port, req, port->atoms.ok); // {ok, ok}
}

static term do_set_text_datum(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, setTextDatum, target, flags, Datum}
    term datum_t = term_get_tuple_element(req->request_tuple, 5);

    uint32_t datum = 0;
    if (!lgfx_term_to_u32(datum_t, &datum) || datum > 255) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_text_datum(port, (uint8_t) req->target, (uint8_t) datum));

    return reply_ok(ctx, port, req, port->atoms.ok); // {ok, ok}
}

static term do_set_text_wrap(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // arity 6: {lgfx, ver, setTextWrap, target, flags, WrapX}
    // arity 7: {lgfx, ver, setTextWrap, target, flags, WrapX, WrapY}
    const int arity = term_get_tuple_arity(req->request_tuple);

    term wrap_x_t = term_get_tuple_element(req->request_tuple, 5);

    bool wrap_x = false;
    if (!decode_bool_term(port, wrap_x_t, &wrap_x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    bool wrap_y = wrap_x;
    if (arity == 7) {
        term wrap_y_t = term_get_tuple_element(req->request_tuple, 6);
        if (!decode_bool_term(port, wrap_y_t, &wrap_y)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_text_wrap_xy(port, (uint8_t) req->target, wrap_x, wrap_y));

    return reply_ok(ctx, port, req, port->atoms.ok); // {ok, ok}
}

static term do_set_text_font(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, setTextFont, target, flags, FontId}
    term font_t = term_get_tuple_element(req->request_tuple, 5);

    uint32_t font = 0;
    if (!lgfx_term_to_u32(font_t, &font) || font > 255) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_text_font(port, (uint8_t) req->target, (uint8_t) font));

    return reply_ok(ctx, port, req, port->atoms.ok); // {ok, ok}
}

static term do_set_font_preset(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, setFontPreset, target, flags, PresetId}
    // Wire form (integer):
    //   0=ascii, 1=jp_small, 2=jp_medium, 3=jp_large
    term preset_t = term_get_tuple_element(req->request_tuple, 5);

    uint8_t preset = 0;
    if (!decode_font_preset(preset_t, &preset)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    // Worker/device layer mapping:
    // - ascii: built-in ASCII fallback (e.g. setTextFont(1))
    // - jp_*:  build-dependent (may be NOT_SUPPORTED if compiled out)
    //
    // reply normalizes ESP_ERR_NOT_SUPPORTED to {error, unsupported}.
    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_font_preset(port, (uint8_t) req->target, preset));

    return reply_ok(ctx, port, req, port->atoms.ok); // {ok, ok}
}

static term do_set_text_color(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // setTextColor(Fg) or setTextColor(Fg, Bg) depending on F_TEXT_HAS_BG.
    // Shared dispatch validates flags + variable arity for this opcode.

    bool has_bg = ((req->flags & LGFX_F_TEXT_HAS_BG) != 0);

    term fg_t = term_get_tuple_element(req->request_tuple, 5);
    uint16_t fg565 = 0;

    if (!lgfx_term_to_color565(fg_t, &fg565)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    uint16_t bg565 = 0;
    if (has_bg) {
        term bg_t = term_get_tuple_element(req->request_tuple, 6);
        if (!lgfx_term_to_color565(bg_t, &bg565)) {
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

    return reply_ok(ctx, port, req, port->atoms.ok); // {ok, ok}
}

static term do_draw_string(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // drawString(X, Y, TextUtf8Binary)
    term x_t = term_get_tuple_element(req->request_tuple, 5);
    term y_t = term_get_tuple_element(req->request_tuple, 6);
    term text_t = term_get_tuple_element(req->request_tuple, 7);

    int16_t x = 0;
    int16_t y = 0;

    if (!lgfx_term_to_i16(x_t, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i16(y_t, &y)) {
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
            (uint16_t) len32));

    return reply_ok(ctx, port, req, port->atoms.ok); // {ok, ok}
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

term lgfx_handle_setFontPreset(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_set_font_preset(ctx, port, req);
}

term lgfx_handle_setTextColor(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_set_text_color(ctx, port, req);
}

term lgfx_handle_drawString(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_draw_string(ctx, port, req);
}
