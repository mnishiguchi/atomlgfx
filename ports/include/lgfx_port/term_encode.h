#pragma once

#include "context.h"
#include "term.h"
#include <stdbool.h>

#include "lgfx_port/lgfx_port.h"

#ifdef __cplusplus
extern "C" {
#endif

term lgfx_reply_ok(Context *ctx, lgfx_port_t *port, term result);
term lgfx_reply_error(Context *ctx, lgfx_port_t *port, term reason_atom);

// {error, {Reason, Detail}}
term lgfx_reply_error_detail(Context *ctx, lgfx_port_t *port, term reason_atom, term detail);

// Helpers for structured tuples (small arities used by getCaps/getLastError)
term lgfx_make_tuple(Context *ctx, int arity, const term *elements);

bool lgfx_is_error_reply(Context *ctx, lgfx_port_t *port, term reply, term *out_reason);

#ifdef __cplusplus
}
#endif
