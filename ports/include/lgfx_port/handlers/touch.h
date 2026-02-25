// ports/include/lgfx_port/handlers/touch.h

#pragma once

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_decode.h"

term lgfx_handle_getTouch(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_getTouchRaw(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_setTouchCalibrate(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_handle_calibrateTouch(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
