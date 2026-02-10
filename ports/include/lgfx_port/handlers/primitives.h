#pragma once

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_decode.h"

#ifdef __cplusplus
extern "C" {
#endif

term lgfx_handle_fillScreen(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_clear(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_drawPixel(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_drawFastVLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_drawFastHLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_drawLine(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_drawRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_fillRect(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_drawCircle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_fillCircle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_drawTriangle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_fillTriangle(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);

#ifdef __cplusplus
}
#endif
