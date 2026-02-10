// ports/include/lgfx_port/handlers/text.h
#pragma once

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_decode.h"

#ifdef __cplusplus
extern "C" {
#endif

term lgfx_handle_setTextSize(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_setTextDatum(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_setTextWrap(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_setTextFont(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_setTextColor(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_drawString(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);

#ifdef __cplusplus
}
#endif
