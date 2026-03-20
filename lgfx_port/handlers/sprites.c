/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/handlers/sprites.c
#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"

// Destination-aware sprite ops handled here:
//
// - pushSprite request shape:
//   {lgfx, ver, pushSprite, SrcSprite, Flags,
//      DstTarget, X, Y [, Transparent]}
//
// - SrcSprite is the request header Target (sprite-only; 1..254)
// - DstTarget is 0 (LCD) or 1..254 (sprite)
// - Transparent is interpreted as:
//   - RGB565 when LGFX_F_TRANSPARENT_INDEX is not set
//   - palette index when LGFX_F_TRANSPARENT_INDEX is set
//
// pushRotateZoom wire payload convention used in this handler:
//
// - Request shape:
//   {lgfx, ver, pushRotateZoom, SrcSprite, Flags,
//      DstTarget, X, Y, Angle, ZoomX, ZoomY [, Transparent]}
//
// - SrcSprite is the request header Target (sprite-only; 1..254)
// - DstTarget is 0 (LCD) or 1..254 (sprite)
// - Angle uses LovyanGFX-like degree semantics
// - ZoomX / ZoomY use LovyanGFX-like positive scale semantics
// - integer and float terms are both accepted by handler decode
// - Transparent is interpreted as:
//   - RGB565 when LGFX_F_TRANSPARENT_INDEX is not set
//   - palette index when LGFX_F_TRANSPARENT_INDEX is set

typedef struct
{
    uint16_t w;
    uint16_t h;
    uint8_t depth;
} lgfx_create_sprite_args_t;

typedef struct
{
    uint8_t palette_index;
    uint32_t rgb888;
} lgfx_set_palette_color_args_t;

typedef struct
{
    int16_t px;
    int16_t py;
} lgfx_set_pivot_args_t;

typedef struct
{
    uint8_t dst_target;
    int16_t x;
    int16_t y;
    bool has_transparent;
    bool transparent_is_index;
    uint32_t transparent_value;
} lgfx_push_sprite_args_t;

typedef struct
{
    uint8_t dst_target;
    int16_t x;
    int16_t y;
    float angle;
    float zoom_x;
    float zoom_y;
    bool has_transparent;
    bool transparent_is_index;
    uint32_t transparent_value;
} lgfx_push_rotate_zoom_args_t;

static bool decode_create_sprite_args(const lgfx_request_t *req, lgfx_create_sprite_args_t *out)
{
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
        return false;
    }

    out->w = (uint16_t) w32;
    out->h = (uint16_t) h32;
    out->depth = (uint8_t) depth32;
    return true;
}

static bool decode_set_palette_color_args(const lgfx_request_t *req, lgfx_set_palette_color_args_t *out)
{
    return out
        && lgfx_decode_palette_index_at(req, 5, &out->palette_index)
        && lgfx_decode_rgb888_at(req, 6, &out->rgb888);
}

static bool decode_set_pivot_args(const lgfx_request_t *req, lgfx_set_pivot_args_t *out)
{
    return lgfx_decode_i16_at(req, 5, &out->px)
        && lgfx_decode_i16_at(req, 6, &out->py);
}

static bool decode_push_sprite_args(const lgfx_request_t *req, lgfx_push_sprite_args_t *out)
{
    const bool transparent_is_index_flag = lgfx_req_has_flag(req, LGFX_F_TRANSPARENT_INDEX);
    uint32_t dst_target32 = 0;

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
    out->transparent_is_index = false;
    out->transparent_value = 0;

    if (req->arity == 9) {
        if (!lgfx_decode_color_or_index_at(
                req,
                8,
                LGFX_F_TRANSPARENT_INDEX,
                &out->transparent_is_index,
                &out->transparent_value)) {
            return false;
        }

        out->has_transparent = true;
        return true;
    }

    if (req->arity == 8) {
        return !transparent_is_index_flag;
    }

    return false;
}

static bool decode_push_rotate_zoom_args(const lgfx_request_t *req, lgfx_push_rotate_zoom_args_t *out)
{
    const bool transparent_is_index_flag = lgfx_req_has_flag(req, LGFX_F_TRANSPARENT_INDEX);
    uint32_t dst_target32 = 0;

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
    if (!lgfx_decode_f32_at(req, 8, &out->angle)) {
        return false;
    }
    if (!lgfx_decode_f32_at(req, 9, &out->zoom_x)) {
        return false;
    }
    if (!lgfx_decode_f32_at(req, 10, &out->zoom_y)) {
        return false;
    }

    out->has_transparent = false;
    out->transparent_is_index = false;
    out->transparent_value = 0;

    if (req->arity == 12) {
        if (!lgfx_decode_color_or_index_at(
                req,
                11,
                LGFX_F_TRANSPARENT_INDEX,
                &out->transparent_is_index,
                &out->transparent_value)) {
            return false;
        }

        out->has_transparent = true;
        return true;
    }

    if (req->arity == 11) {
        return !transparent_is_index_flag;
    }

    return false;
}

term lgfx_handle_createSprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_create_sprite_args_t args = { 0 };

    if (!decode_create_sprite_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_sprite_create_at(
            (uint8_t) req->target,
            args.w,
            args.h,
            args.depth));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_deleteSprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_device_sprite_delete((uint8_t) req->target));
    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_createPalette(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_sprite_create_palette((uint8_t) req->target));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setPaletteColor(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_set_palette_color_args_t args = { 0 };

    if (!decode_set_palette_color_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_sprite_set_palette_color(
            (uint8_t) req->target,
            args.palette_index,
            args.rgb888));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_setPivot(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_set_pivot_args_t args = { 0 };

    if (!decode_set_pivot_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_set_pivot(
            (uint8_t) req->target,
            args.px,
            args.py));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_pushSprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_push_sprite_args_t args = { 0 };

    if (!decode_push_sprite_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_sprite_push_sprite(
            (uint8_t) req->target,
            args.dst_target,
            args.x,
            args.y,
            args.has_transparent,
            args.transparent_is_index,
            args.transparent_value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_pushRotateZoom(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_push_rotate_zoom_args_t args = { 0 };

    if (!decode_push_rotate_zoom_args(req, &args)) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_sprite_push_rotate_zoom(
            (uint8_t) req->target,
            args.dst_target,
            args.x,
            args.y,
            args.angle,
            args.zoom_x,
            args.zoom_y,
            args.has_transparent,
            args.transparent_is_index,
            args.transparent_value));

    return reply_ok(ctx, port, req, port->atoms.ok);
}
