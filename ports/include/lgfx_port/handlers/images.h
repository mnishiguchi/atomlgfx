#pragma once

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_decode.h"

#ifdef __cplusplus
extern "C" {
#endif

term lgfx_handle_pushImage(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);

#ifdef __cplusplus
}
#endif
