// ports/handlers/setup.c
#include "lgfx_port/handlers/setup.h"

#include <stdint.h>
#include <stdio.h>

#include "context.h"
#include "term.h"

// Device calls go through worker wrappers.
#include "lgfx_port/worker.h"

#include "lgfx_port/reply_common.h"
#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_conv.h"
#include "lgfx_port/term_encode.h"
#include "lgfx_port/validate.h"

// Request envelope validation (version/arity/flags/target/init-state) is
// centralized in lgfx_port.c via ops.def metadata. Handlers only decode payload fields.

static term do_init(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // Idempotent init: if already initialized, do nothing and return ok.
    // Re-init is supported via close() + init() cycle.
    if (port->initialized) {
        return reply_ok(ctx, port, req, port->atoms.ok);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_init(port));

    port->initialized = true;
    lgfx_last_error_clear(port);

    // Cache dimensions for width/height ops.
    // If this fails, cached values remain 0/0.
    uint16_t w = 0;
    uint16_t h = 0;
    if (lgfx_worker_device_get_dims(port, &w, &h) == ESP_OK) {
        port->width = (uint32_t) w;
        port->height = (uint32_t) h;
    }

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_close(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // close() is always safe to call (even if not initialized).
    if (!port->initialized) {
        lgfx_last_error_clear(port);
        return reply_ok(ctx, port, req, port->atoms.ok);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_close(port));

    port->initialized = false;

    // Drop cached dimensions (optional, but avoids returning stale values after close).
    port->width = 0;
    port->height = 0;

    lgfx_last_error_clear(port);
    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_set_rotation(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    term rot_t = term_get_tuple_element(req->request_tuple, 5);

    uint32_t rot = 0;
    if (!lgfx_term_to_u32(rot_t, &rot) || rot > 7) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_set_rotation(port, (uint8_t) rot));

    // Refresh cached dimensions after rotation.
    uint16_t w = 0;
    uint16_t h = 0;
    if (lgfx_worker_device_get_dims(port, &w, &h) == ESP_OK) {
        port->width = (uint32_t) w;
        port->height = (uint32_t) h;
    }

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_set_brightness(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, setBrightness, target, flags, Brightness}
    term b_t = term_get_tuple_element(req->request_tuple, 5);

    uint32_t b = 0;
    if (!lgfx_term_to_u32(b_t, &b) || b > 255) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_set_brightness(port, (uint8_t) b));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_set_color_depth(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // {lgfx, ver, setColorDepth, target, flags, Depth}
    term d_t = term_get_tuple_element(req->request_tuple, 5);

    uint32_t d = 0;
    if (!lgfx_term_to_u32(d_t, &d)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!lgfx_validate_color_depth(d)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    // Device ABI: (target, depth)
    // Protocol v1 validates target via ops.def metadata (T0/unsupported).
    LGFX_RETURN_IF_ESP_ERR(
        ctx, port, req, lgfx_worker_device_set_color_depth(port, (uint8_t) req->target, (uint8_t) d));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_display(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_display(port));
    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_init(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_init(ctx, port, req);
}

term lgfx_handle_close(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_close(ctx, port, req);
}

term lgfx_handle_setRotation(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_set_rotation(ctx, port, req);
}

term lgfx_handle_setBrightness(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_set_brightness(ctx, port, req);
}

term lgfx_handle_setColorDepth(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_set_color_depth(ctx, port, req);
}

term lgfx_handle_display(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_display(ctx, port, req);
}
