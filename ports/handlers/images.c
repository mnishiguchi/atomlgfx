// ports/handlers/images.c
#include "lgfx_port/handlers/images.h"

#include <stddef.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

// NOTE: device calls now go through the worker wrappers.
#include "lgfx_port/worker.h"

#include "lgfx_port/handler_common.h"
#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_conv.h"
#include "lgfx_port/term_decode.h"
#include "lgfx_port/term_encode.h"
#include "lgfx_port/validate.h"

// Envelope checks (version/arity/flags/target/init-state) are centralized in
// lgfx_port.c via ops.def metadata. Handlers here only decode payload fields.

static term do_push_image(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, pushImage, target, flags, X, Y, W, H, StridePixels, DataRgb565Binary}
    term x_t = term_get_tuple_element(req->request_tuple, 5);
    term y_t = term_get_tuple_element(req->request_tuple, 6);
    term w_t = term_get_tuple_element(req->request_tuple, 7);
    term h_t = term_get_tuple_element(req->request_tuple, 8);
    term stride_t = term_get_tuple_element(req->request_tuple, 9);
    term data_t = term_get_tuple_element(req->request_tuple, 10);

    int32_t x32 = 0;
    int32_t y32 = 0;
    uint32_t w32 = 0;
    uint32_t h32 = 0;
    uint32_t stride32 = 0;

    if (!lgfx_term_to_i32(x_t, &x32) || !lgfx_validate_i16(x32)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_i32(y_t, &y32) || !lgfx_validate_i16(y32)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_u32(w_t, &w32) || !lgfx_validate_u16(w32) || w32 == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_u32(h_t, &h32) || !lgfx_validate_u16(h32) || h32 == 0) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_term_to_u32(stride_t, &stride32) || !lgfx_validate_u16(stride32)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!term_is_binary(data_t)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    // StrideEff rule (stride==0 means tightly packed: stride=w)
    uint32_t stride_eff = (stride32 == 0) ? w32 : stride32;
    if (stride_eff < w32) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const uint8_t *bytes = (const uint8_t *) term_binary_data(data_t);
    size_t len = (size_t) term_binary_size(data_t);

    if (len > (size_t) LGFX_PORT_MAX_BINARY_BYTES) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    // RGB565 must be even byte length.
    if ((len & 1u) != 0u) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    // Required minimum size: StrideEff * H * 2 (allow extra trailing bytes).
    uint64_t needed64 = (uint64_t) stride_eff * (uint64_t) h32 * 2u;
    if (needed64 > (uint64_t) SIZE_MAX) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    size_t needed = (size_t) needed64;

    if (len < needed) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req,
        lgfx_worker_device_push_image_rgb565_strided(
            port,
            (uint8_t) req->target,
            (int16_t) x32,
            (int16_t) y32,
            (uint16_t) w32,
            (uint16_t) h32,
            (uint16_t) stride32, // keep original (0 => tightly packed) and let C++ apply stride==0 rule
            bytes,
            len));

    return lgfx_reply_ok(ctx, port, port->atoms.ok); // {ok, ok}
}

term lgfx_handle_pushImage(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_push_image(ctx, port, req);
}
