#pragma once

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_decode.h"

#ifdef __cplusplus
extern "C" {
#endif

term lgfx_handle_init(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_close(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_setRotation(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_setBrightness(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_setColorDepth(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_display(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);

#ifdef __cplusplus
}
#endif
