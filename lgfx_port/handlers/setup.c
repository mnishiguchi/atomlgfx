// lgfx_port/handlers/setup.c
//
// Control-plane handlers:
// - ping / getCaps / getLastError / width / height
// - init / close / display and basic device configuration
#include <stdint.h>
#include <stdio.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/worker.h"

// Request envelope validation (version/arity/flags/target/init-state) is
// centralized in lgfx_port.c via ops.def metadata. Handlers only decode payload fields.

// -----------------------------------------------------------------------------
// Control ops
// -----------------------------------------------------------------------------

static term do_get_caps(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // Metadata-driven caps + runtime enable gating live in lgfx_port.c.
    // This keeps FeatureBits aligned with ops.def feature_cap_bit and build gates.
    uint32_t feature_bits = lgfx_port_feature_bits(port);
    uint32_t max_sprites = (uint32_t) lgfx_port_max_sprites(port);

    term elems[5] = {
        port->atoms.caps,
        term_from_int32((int32_t) LGFX_PORT_PROTO_VER),
        term_from_int32((int32_t) LGFX_PORT_MAX_BINARY_BYTES),
        term_from_int32((int32_t) max_sprites),
        term_from_int32((int32_t) feature_bits)
    };

    term payload = lgfx_make_tuple(ctx, 5, elems);
    if (term_is_invalid_term(payload)) {
        return reply_error(ctx, port, req, port->atoms.no_memory, (int32_t) ESP_ERR_NO_MEM);
    }

    return reply_ok(ctx, port, req, payload);
}

static term do_get_last_error(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
#if LGFX_PORT_SUPPORTS_LAST_ERROR
    // Snapshot first. Clear only after the final {ok, Payload} reply is encoded.
    lgfx_last_error_t e = port->last_error;

    term elems[6] = {
        port->atoms.last_error,
        e.last_op,
        e.reason,
        term_from_int32((int32_t) e.flags),
        term_from_int32((int32_t) e.target),
        term_from_int32((int32_t) e.esp_err)
    };

    term payload = lgfx_make_tuple(ctx, 6, elems);
    if (term_is_invalid_term(payload)) {
        // Payload encoding failed; record no_memory as the latest error.
        return reply_error(ctx, port, req, port->atoms.no_memory, (int32_t) ESP_ERR_NO_MEM);
    }

    term reply = reply_ok(ctx, port, req, payload);
    if (term_is_invalid_term(reply)) {
        // reply_ok already recorded last_error = no_memory.
        return reply;
    }

    lgfx_last_error_clear(port);
    return reply;
#else
    return reply_error(ctx, port, req, port->atoms.unsupported, 0);
#endif
}

term lgfx_handle_ping(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return reply_ok(ctx, port, req, port->atoms.pong);
}

term lgfx_handle_getCaps(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_get_caps(ctx, port, req);
}

term lgfx_handle_getLastError(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_get_last_error(ctx, port, req);
}

// Request envelope validation is centralized in lgfx_port.c via ops.def metadata.
term lgfx_handle_width(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
#if !LGFX_PORT_SUPPORTS_SPRITE
    // Sprite surface is compiled out; treat non-zero targets as unsupported.
    if (req->target != 0u) {
        return reply_error(ctx, port, req, port->atoms.unsupported, (int32_t) ESP_ERR_NOT_SUPPORTED);
    }
#endif

    // LCD target uses cached dimensions (refreshed at init / setRotation).
    if (req->target == 0u) {
        return reply_ok(ctx, port, req, term_from_int32((int32_t) port->width));
    }

    // Sprite target: query live dimensions (unallocated => ESP_ERR_NOT_FOUND => {error, bad_target}).
    uint16_t w = 0;
    uint16_t h = 0;
    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_get_target_dims(port, (uint8_t) req->target, &w, &h));
    return reply_ok(ctx, port, req, term_from_int32((int32_t) w));
}

term lgfx_handle_height(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
#if !LGFX_PORT_SUPPORTS_SPRITE
    if (req->target != 0u) {
        return reply_error(ctx, port, req, port->atoms.unsupported, (int32_t) ESP_ERR_NOT_SUPPORTED);
    }
#endif

    if (req->target == 0u) {
        return reply_ok(ctx, port, req, term_from_int32((int32_t) port->height));
    }

    uint16_t w = 0;
    uint16_t h = 0;
    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_get_target_dims(port, (uint8_t) req->target, &w, &h));
    return reply_ok(ctx, port, req, term_from_int32((int32_t) h));
}

// -----------------------------------------------------------------------------
// Setup ops
// -----------------------------------------------------------------------------

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

#if !LGFX_PORT_SUPPORTS_SPRITE
    // Sprite surface is compiled out; treat non-zero targets as unsupported.
    if (req->target != 0u) {
        return reply_error(ctx, port, req, port->atoms.unsupported, (int32_t) ESP_ERR_NOT_SUPPORTED);
    }
#endif

    uint32_t d = 0;
    if (!lgfx_term_to_u32(d_t, &d)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    if (!lgfx_validate_color_depth(d)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    // Device ABI: (target, depth)
    // Protocol validates target via ops.def metadata (target-aware).
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
