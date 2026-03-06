// lgfx_port/handlers/sprites.c
#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "esp_err.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/worker.h"

// Request envelope validation (version/arity/flags/target/init-state) is
// centralized in lgfx_port.c via ops.def metadata. Handlers only decode payload fields.
//
// Destination-aware sprite ops handled here:
//
// - pushSprite request shape:
//   {lgfx, ver, pushSprite, SrcSprite, Flags,
//      DstTarget, X, Y [, Transparent565]}
//
// - SrcSprite is the request header Target (sprite-only; 1..254)
// - DstTarget is 0 (LCD) or 1..254 (sprite)
//
// pushRotateZoom wire payload convention used in this handler:
//
// - Request shape:
//   {lgfx, ver, pushRotateZoom, SrcSprite, Flags,
//      DstTarget, X, Y, AngleCentiDegI32, ZoomXX1024I32, ZoomYX1024I32 [, Transparent565]}
//
// - SrcSprite is the request header Target (sprite-only; 1..254)
// - DstTarget is 0 (LCD) or 1..254 (sprite)
// - Angle uses centi-degrees (i32; 9000 == 90.00°)
// - Zoom uses x1024 fixed-point scale (i32; 1024 == 1.0x)
//
// Current worker wrapper still accepts float angle/zoom values, so this handler
// decodes the wire format (i32/i32) and converts to float at the worker call site.
#define LGFX_ANGLE_X100_SCALE 100.0f
#define LGFX_ZOOM_X1024_SCALE 1024.0f

typedef struct
{
    uint16_t w;
    uint16_t h;
    uint8_t depth;
} lgfx_create_sprite_args_t;

typedef struct
{
    int16_t px;
    int16_t py;
} lgfx_set_pivot_args_t;

typedef struct
{
    uint8_t dst_target; // 0 => LCD, 1..254 => sprite
    int16_t x;
    int16_t y;
    bool has_transparent;
    uint16_t transparent565;
} lgfx_push_sprite_args_t;

typedef struct
{
    uint8_t dst_target; // 0 => LCD, 1..254 => sprite
    int16_t x;
    int16_t y;
    int32_t angle_x100;
    int32_t zoom_x_x1024;
    int32_t zoom_y_x1024;
    bool has_transparent;
    uint16_t transparent565;
} lgfx_push_rotate_zoom_args_t;

static bool decode_create_sprite_args(const lgfx_request_t *req, lgfx_create_sprite_args_t *out)
{
    // createSprite supports dual arity:
    // - {lgfx, ver, createSprite, Target, Flags, W, H}
    // - {lgfx, ver, createSprite, Target, Flags, W, H, Depth}
    //
    // Shared metadata already enforces arity range [7..8], but we still decode safely.

    uint32_t w32 = 0;
    uint32_t h32 = 0;
    uint32_t depth32 = (uint32_t) LGFX_PORT_SPRITE_DEFAULT_DEPTH;

    if (!lgfx_decode_u32_at(req, 5, &w32) || !lgfx_validate_u16(w32) || w32 == 0) {
        return false;
    }
    if (!lgfx_decode_u32_at(req, 6, &h32) || !lgfx_validate_u16(h32) || h32 == 0) {
        return false;
    }

    if (req->arity == 8) {
        if (!lgfx_decode_u32_at(req, 7, &depth32) || !lgfx_validate_color_depth(depth32)) {
            return false;
        }
    } else if (req->arity != 7) {
        // Defensive only; shared validator should catch this earlier.
        return false;
    }

    out->w = (uint16_t) w32;
    out->h = (uint16_t) h32;
    out->depth = (uint8_t) depth32;
    return true;
}

static bool decode_set_pivot_args(const lgfx_request_t *req, lgfx_set_pivot_args_t *out)
{
    // setPivot:
    // - {lgfx, ver, setPivot, Target, Flags, Px, Py}
    //
    // Shared metadata enforces exact arity 7.

    return lgfx_decode_i16_at(req, 5, &out->px)
        && lgfx_decode_i16_at(req, 6, &out->py);
}

static bool decode_push_sprite_args(const lgfx_request_t *req, lgfx_push_sprite_args_t *out)
{
    // pushSprite supports dual arity (destination-aware):
    // - {lgfx, ver, pushSprite, SrcSprite, Flags, DstTarget, X, Y}
    // - {lgfx, ver, pushSprite, SrcSprite, Flags, DstTarget, X, Y, Transparent565}
    //
    // Shared metadata enforces arity range [8..9].

    uint32_t dst_target32 = 0;
    uint32_t transparent32 = 0;

    if (!lgfx_decode_u32_at(req, 5, &dst_target32) || dst_target32 > 254u) {
        return false;
    }
    out->dst_target = (uint8_t) dst_target32;

    if (!lgfx_decode_i16_at(req, 6, &out->x)) {
        return false;
    }
    if (!lgfx_decode_i16_at(req, 7, &out->y)) {
        return false;
    }

    out->has_transparent = false;
    out->transparent565 = 0;

    if (req->arity == 9) {
        if (!lgfx_decode_u32_at(req, 8, &transparent32) || !lgfx_validate_u16(transparent32)) {
            return false;
        }

        out->has_transparent = true;
        out->transparent565 = (uint16_t) transparent32;
    } else if (req->arity != 8) {
        // Defensive only; shared validator should catch this earlier.
        return false;
    }

    return true;
}

static bool decode_push_rotate_zoom_args(const lgfx_request_t *req, lgfx_push_rotate_zoom_args_t *out)
{
    // pushRotateZoom supports dual arity:
    // - {lgfx, ver, pushRotateZoom, SrcSprite, Flags, DstTarget, X, Y, AngleX100I32, ZoomXX1024I32, ZoomYX1024I32}
    // - {lgfx, ver, pushRotateZoom, SrcSprite, Flags, DstTarget, X, Y, AngleX100I32, ZoomXX1024I32, ZoomYX1024I32, Transparent565}
    //
    // Shared metadata enforces arity range [11..12].

    uint32_t dst_target32 = 0;
    uint32_t transparent32 = 0;

    if (!lgfx_decode_u32_at(req, 5, &dst_target32) || dst_target32 > 254u) {
        return false;
    }
    out->dst_target = (uint8_t) dst_target32;

    if (!lgfx_decode_i16_at(req, 6, &out->x)) {
        return false;
    }
    if (!lgfx_decode_i16_at(req, 7, &out->y)) {
        return false;
    }
    if (!lgfx_decode_i32_at(req, 8, &out->angle_x100)) {
        return false;
    }
    if (!lgfx_decode_i32_at(req, 9, &out->zoom_x_x1024)) {
        return false;
    }
    if (!lgfx_decode_i32_at(req, 10, &out->zoom_y_x1024)) {
        return false;
    }

    // x1024 zoom values must be positive (0 and negatives are invalid).
    if (out->zoom_x_x1024 <= 0) {
        return false;
    }
    if (out->zoom_y_x1024 <= 0) {
        return false;
    }

    out->has_transparent = false;
    out->transparent565 = 0;

    if (req->arity == 12) {
        if (!lgfx_decode_u32_at(req, 11, &transparent32) || !lgfx_validate_u16(transparent32)) {
            return false;
        }

        out->has_transparent = true;
        out->transparent565 = (uint16_t) transparent32;
    } else if (req->arity != 11) {
        // Defensive only; shared validator should catch this earlier.
        return false;
    }

    return true;
}

static term do_create_sprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_create_sprite_args_t args = { 0 };

    if (!decode_create_sprite_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_create_sprite(
            port,
            (uint8_t) req->target,
            args.w,
            args.h,
            args.depth));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_delete_sprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_delete_sprite(port, (uint8_t) req->target));
    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_set_pivot(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_set_pivot_args_t args = { 0 };

    if (!decode_set_pivot_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_set_pivot(
            port,
            (uint8_t) req->target,
            args.px,
            args.py));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_push_sprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_push_sprite_args_t args = { 0 };

    if (!decode_push_sprite_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    // Protocol semantics:
    // - SrcSprite (request header Target) must exist
    // - DstTarget must exist when DstTarget != 0
    //
    // Use get_target_dims as the cheap existence probe.
    uint16_t src_w = 0;
    uint16_t src_h = 0;
    esp_err_t err = lgfx_worker_device_get_target_dims(port, (uint8_t) req->target, &src_w, &src_h);
    if (err == ESP_ERR_NOT_FOUND) {
        return reply_error(ctx, port, req, port->atoms.bad_target, 0);
    }
    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, err);

    if (args.dst_target != 0) {
        uint16_t dst_w = 0;
        uint16_t dst_h = 0;
        err = lgfx_worker_device_get_target_dims(port, args.dst_target, &dst_w, &dst_h);
        if (err == ESP_ERR_NOT_FOUND) {
            return reply_error(ctx, port, req, port->atoms.bad_target, 0);
        }
        LGFX_RETURN_IF_ESP_ERR(ctx, port, req, err);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_push_sprite(
            port,
            (uint8_t) req->target, // SrcSprite
            args.dst_target, // DstTarget (0 => LCD, 1..254 => sprite)
            args.x,
            args.y,
            args.has_transparent,
            args.transparent565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

static term do_push_rotate_zoom(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_push_rotate_zoom_args_t args = { 0 };

    if (!decode_push_rotate_zoom_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    // Worker wrapper currently accepts float degrees / float zoom.
    // Convert from protocol wire format:
    // - angle: centi-degrees i32 -> float degrees
    // - zoom : x1024 scale i32   -> float scale
    const float angle_deg = ((float) args.angle_x100) / LGFX_ANGLE_X100_SCALE;
    const float zoom_x = ((float) args.zoom_x_x1024) / LGFX_ZOOM_X1024_SCALE;
    const float zoom_y = ((float) args.zoom_y_x1024) / LGFX_ZOOM_X1024_SCALE;

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_worker_device_push_rotate_zoom(
            port,
            (uint8_t) req->target, // SrcSprite
            args.dst_target, // DstTarget (0 => LCD, 1..254 => sprite)
            args.x,
            args.y,
            angle_deg,
            zoom_x,
            zoom_y,
            args.has_transparent,
            args.transparent565));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_createSprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_create_sprite(ctx, port, req);
}

term lgfx_handle_deleteSprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_delete_sprite(ctx, port, req);
}

term lgfx_handle_setPivot(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_set_pivot(ctx, port, req);
}

term lgfx_handle_pushSprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_push_sprite(ctx, port, req);
}

term lgfx_handle_pushRotateZoom(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_push_rotate_zoom(ctx, port, req);
}
