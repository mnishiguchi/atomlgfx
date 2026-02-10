// ports/include/lgfx_port/dispatch_table.h
#pragma once

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_decode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef term (*lgfx_handler_fn)(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);

lgfx_handler_fn lgfx_dispatch_lookup(lgfx_port_t *port, term op);

#ifdef __cplusplus
}
#endif
