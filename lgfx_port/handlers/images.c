// lgfx_port/handlers/images.c
#include <stddef.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/proto_term.h"

#include "lgfx_port/ops.h"
#include "lgfx_port/worker.h"

term lgfx_handle_pushImage(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, pushImage, target, flags, X, Y, W, H, StridePixels, DataRgb565Binary}
    //
    // Handler responsibility here is limited to wire decode.
    // Binary-size capping stays in lgfx_decode_binary_at().
    // Device code remains authoritative for:
    // - stride == 0 normalization
    // - stride >= w
    // - even byte length
    // - overflow / required byte count
    // - insufficient payload size

    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t stride = 0;

    if (!lgfx_decode_i16_at(req, 5, &x)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_i16_at(req, 6, &y)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 7, &w)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 8, &h)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }
    if (!lgfx_decode_u16_at(req, 9, &stride)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    const uint8_t *bytes = NULL;
    size_t len = 0;
    if (!lgfx_decode_binary_at(req, 10, &bytes, &len)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_push_image_rgb565_strided(
            port,
            (uint8_t) req->target,
            x,
            y,
            w,
            h,
            stride,
            bytes,
            len));

    return reply_ok(ctx, port, req, port->atoms.ok);
}
