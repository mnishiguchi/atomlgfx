/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/include_internal/lgfx_port/handlers/clip.inc
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"

#define LGFX_CLIP_BAD_ARGS() reply_error(ctx, port, req, port->atoms.bad_args, 0)

#define LGFX_DEFINE_CLIP_TARGET_ONLY_HANDLER(handler_name, device_fn)                     \
    term handler_name(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)        \
    {                                                                                     \
        LGFX_RETURN_IF_ESP_ERR(                                                           \
            ctx,                                                                          \
            port,                                                                         \
            req,                                                                          \
            device_fn((uint8_t) req->target));                                            \
                                                                                          \
        return reply_ok(ctx, port, req, port->atoms.ok);                                 \
    }

#define LGFX_DEFINE_CLIP_XY_WH_HANDLER(handler_name, device_fn)                           \
    term handler_name(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)        \
    {                                                                                     \
        int16_t x = 0;                                                                    \
        int16_t y = 0;                                                                    \
        uint16_t w = 0;                                                                   \
        uint16_t h = 0;                                                                   \
                                                                                          \
        if (!lgfx_decode_i16_at(req, 5, &x)                                               \
            || !lgfx_decode_i16_at(req, 6, &y)                                            \
            || !lgfx_decode_u16_at(req, 7, &w)                                            \
            || w == 0u                                                                    \
            || !lgfx_decode_u16_at(req, 8, &h)                                            \
            || h == 0u) {                                                                 \
            return LGFX_CLIP_BAD_ARGS();                                                  \
        }                                                                                 \
                                                                                          \
        LGFX_RETURN_IF_ESP_ERR(                                                           \
            ctx,                                                                          \
            port,                                                                         \
            req,                                                                          \
            device_fn((uint8_t) req->target, x, y, w, h));                               \
                                                                                          \
        return reply_ok(ctx, port, req, port->atoms.ok);                                 \
    }

LGFX_DEFINE_CLIP_XY_WH_HANDLER(lgfx_handle_setClipRect, lgfx_device_set_clip_rect)
LGFX_DEFINE_CLIP_TARGET_ONLY_HANDLER(lgfx_handle_clearClipRect, lgfx_device_clear_clip_rect)

#undef LGFX_DEFINE_CLIP_XY_WH_HANDLER
#undef LGFX_DEFINE_CLIP_TARGET_ONLY_HANDLER
#undef LGFX_CLIP_BAD_ARGS
