#pragma once

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_decode.h"

// Declare all handler functions referenced by ops.def.
// ops.def entries are X(op_name, handler_fn, atom_str, ...)
#define X(_op, _handler, _atom_str, ...) \
    term _handler(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);

#include "lgfx_port/ops.def"
#undef X
