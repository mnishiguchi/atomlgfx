// lgfx_port/handlers/device.c
//
// Device-facing protocol handlers:
// - width / height
// - setRotation / setBrightness / setColorDepth
// - display
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "esp_err.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/worker.h"

static void refresh_cached_dims(lgfx_port_t *port)
{
    uint16_t w = 0;
    uint16_t h = 0;

    if (lgfx_worker_device_get_dims(port, &w, &h) == ESP_OK) {
        port->width = (uint32_t) w;
        port->height = (uint32_t) h;
    }
}

term lgfx_handle_width(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (req->target == 0u) {
        return reply_ok(ctx, port, req, term_from_int32((int32_t) port->width));
    }

    uint16_t w = 0;
    uint16_t h = 0;
    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_get_target_dims(port, (uint8_t) req->target, &w, &h));
    return reply_ok(ctx, port, req, term_from_int32((int32_t) w));
}

term lgfx_handle_height(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (req->target == 0u) {
        return reply_ok(ctx, port, req, term_from_int32((int32_t) port->height));
    }

    uint16_t w = 0;
    uint16_t h = 0;
    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_get_target_dims(port, (uint8_t) req->target, &w, &h));
    return reply_ok(ctx, port, req, term_from_int32((int32_t) h));
}

term lgfx_handle_setRotation(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint32_t rot = 0;
    if (!lgfx_decode_u32_at(req, 5, &rot) || rot > 7u) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_set_rotation(port, (uint8_t) rot));

    refresh_cached_dims(port);

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setBrightness(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint32_t b = 0;
    if (!lgfx_decode_u32_at(req, 5, &b) || b > 255u) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_set_brightness(port, (uint8_t) b));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setColorDepth(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint32_t d = 0;
    if (!lgfx_decode_u32_at(req, 5, &d)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!lgfx_validate_color_depth(d)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_color_depth(port, (uint8_t) req->target, (uint8_t) d));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_display(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_display(port));
    return reply_ok(ctx, port, req, port->atoms.ok);
}
