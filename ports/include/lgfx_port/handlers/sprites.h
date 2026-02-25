// ports/include/lgfx_port/handlers/sprites.h
#pragma once

#include "context.h"
#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_decode.h"
#include "term.h"

#ifdef __cplusplus
extern "C" {
#endif

term lgfx_handle_createSprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_deleteSprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_setPivot(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_pushSprite(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_pushSpriteRegion(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_pushRotateZoom(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);

#ifdef __cplusplus
}
#endif
