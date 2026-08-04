/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/include_internal/lgfx_port/handlers/device.inc
//
// Device-facing protocol handlers:
// - width / height
// - setRotation / setBrightness / setColorDepth / setSwapBytes
// - display
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "esp_err.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"

term lgfx_handle_width(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (req->target == 0u) {
        return reply_ok(ctx, port, req, lgfx_term_from_i32((int32_t) port->width));
    }

    uint16_t w = 0;
    uint16_t h = 0;
    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_get_target_dims((uint8_t) req->target, &w, &h));
    return reply_ok(ctx, port, req, lgfx_term_from_i32((int32_t) w));
}

term lgfx_handle_height(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (req->target == 0u) {
        return reply_ok(ctx, port, req, lgfx_term_from_i32((int32_t) port->height));
    }

    uint16_t w = 0;
    uint16_t h = 0;
    LGFX_RETURN_IF_ESP_ERR(
        ctx,
        port,
        req,
        lgfx_device_get_target_dims((uint8_t) req->target, &w, &h));
    return reply_ok(ctx, port, req, lgfx_term_from_i32((int32_t) h));
}

#define LGFX_DEVICE_BAD_ARGS() reply_error(ctx, port, req, port->atoms.bad_args, 0)

#define LGFX_DEFINE_DEVICE_TARGETLESS_U32_TO_U8_HANDLER(                                    \
    handler_name,                                                                           \
    device_fn,                                                                              \
    max_value,                                                                              \
    after_success_stmt)                                                                     \
    term handler_name(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)          \
    {                                                                                       \
        uint32_t value = 0;                                                                 \
        if (!lgfx_decode_u32_at(req, 5, &value) || value > (max_value)) {                   \
            return LGFX_DEVICE_BAD_ARGS();                                                  \
        }                                                                                   \
                                                                                            \
        LGFX_RETURN_IF_ESP_ERR(ctx, port, req, device_fn((uint8_t) value));                 \
        after_success_stmt                                                                  \
        return reply_ok(ctx, port, req, port->atoms.ok);                                   \
    }

#define LGFX_DEFINE_DEVICE_TARGET_U32_HANDLER(handler_name, validator_expr, device_fn)      \
    term handler_name(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)          \
    {                                                                                       \
        uint32_t value = 0;                                                                 \
        if (!lgfx_decode_u32_at(req, 5, &value) || !(validator_expr)) {                     \
            return LGFX_DEVICE_BAD_ARGS();                                                  \
        }                                                                                   \
                                                                                            \
        LGFX_RETURN_IF_ESP_ERR(                                                             \
            ctx,                                                                            \
            port,                                                                           \
            req,                                                                            \
            device_fn((uint8_t) req->target, (uint8_t) value));                            \
                                                                                            \
        return reply_ok(ctx, port, req, port->atoms.ok);                                   \
    }

#define LGFX_DEFINE_DEVICE_TARGET_BOOL_HANDLER(handler_name, device_fn)                     \
    term handler_name(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)          \
    {                                                                                       \
        bool enabled = false;                                                               \
        if (!lgfx_decode_bool_term(port, lgfx_req_elem(req, 5), &enabled)) {                \
            return LGFX_DEVICE_BAD_ARGS();                                                  \
        }                                                                                   \
                                                                                            \
        LGFX_RETURN_IF_ESP_ERR(                                                             \
            ctx,                                                                            \
            port,                                                                           \
            req,                                                                            \
            device_fn((uint8_t) req->target, enabled));                                     \
                                                                                            \
        return reply_ok(ctx, port, req, port->atoms.ok);                                   \
    }

#define LGFX_DEFINE_DEVICE_TARGETLESS_HANDLER(handler_name, device_fn)                      \
    term handler_name(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)          \
    {                                                                                       \
        LGFX_RETURN_IF_ESP_ERR(ctx, port, req, device_fn());                                \
        return reply_ok(ctx, port, req, port->atoms.ok);                                   \
    }

LGFX_DEFINE_DEVICE_TARGETLESS_U32_TO_U8_HANDLER(
    lgfx_handle_setRotation,
    lgfx_device_set_rotation,
    7u,
    lgfx_refresh_cached_dims(port);)
LGFX_DEFINE_DEVICE_TARGETLESS_U32_TO_U8_HANDLER(
    lgfx_handle_setBrightness,
    lgfx_device_set_brightness,
    255u,
    ((void) 0);)
LGFX_DEFINE_DEVICE_TARGET_U32_HANDLER(
    lgfx_handle_setColorDepth,
    lgfx_validate_color_depth(value),
    lgfx_device_set_color_depth)
LGFX_DEFINE_DEVICE_TARGET_BOOL_HANDLER(lgfx_handle_setSwapBytes, lgfx_device_set_swap_bytes)
LGFX_DEFINE_DEVICE_TARGETLESS_HANDLER(lgfx_handle_display, lgfx_device_display)

#undef LGFX_DEFINE_DEVICE_TARGETLESS_HANDLER
#undef LGFX_DEFINE_DEVICE_TARGET_BOOL_HANDLER
#undef LGFX_DEFINE_DEVICE_TARGET_U32_HANDLER
#undef LGFX_DEFINE_DEVICE_TARGETLESS_U32_TO_U8_HANDLER
#undef LGFX_DEVICE_BAD_ARGS
