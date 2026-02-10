#pragma once

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_decode.h"

#ifdef __cplusplus
extern "C" {
#endif

term lgfx_handle_ping(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_getCaps(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_getLastError(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_width(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_height(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);

#ifdef __cplusplus
}
#endif
