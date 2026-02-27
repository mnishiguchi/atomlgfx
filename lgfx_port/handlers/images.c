// lgfx_port/handlers/images.c
#include <stddef.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/ops.h"
#include "lgfx_port/worker.h"

// Request envelope validation (version/arity/flags/target/init-state) is
// centralized in lgfx_port.c via ops.def metadata. Handlers only decode payload fields.

static term do_push_image(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, pushImage, target, flags, X, Y, W, H, StridePixels, DataRgb565Binary}
    term x_t = term_get_tuple_element(req->request_tuple, 5);
    term y_t = term_get_tuple_element(req->request_tuple, 6);
    term w_t = term_get_tuple_element(req->request_tuple, 7);
    term h_t = term_get_tuple_element(req->request_tuple, 8);
    term stride_t = term_get_tuple_element(req->request_tuple, 9);
    term data_t = term_get_tuple_element(req->request_tuple, 10);

    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t stride = 0;

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
    if (!lgfx_term_to_u16(stride_t, &stride)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!term_is_binary(data_t)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    // Effective stride rule: stride == 0 means tightly packed (stride = w).
    uint32_t stride_eff = (stride == 0) ? (uint32_t) w : (uint32_t) stride;
    if (stride_eff < (uint32_t) w) {
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
    uint64_t needed64 = (uint64_t) stride_eff * (uint64_t) h * 2u;
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
            x,
            y,
            w,
            h,
            stride, // keep original value (0 => tightly packed); C++ applies the stride==0 rule
            bytes,
            len));

    return reply_ok(ctx, port, req, port->atoms.ok); // {ok, ok}
}

term lgfx_handle_pushImage(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_push_image(ctx, port, req);
}
