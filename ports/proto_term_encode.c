#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "context.h"
#include "memory.h"
#include "term.h"

#include "port.h" // port_create_tuple2, etc.

#include "lgfx_port/term_encode.h"

term lgfx_make_tuple(Context *ctx, int arity, const term *elements)
{
    // Tuple storage is header + arity words (typical on AtomVM heap)
    // If your AtomVM version uses a different sizing macro, adjust here.
    if (memory_ensure_free(ctx, arity + 1) != MEMORY_GC_OK) {
        return term_invalid_term();
    }

    term t = term_alloc_tuple(arity, &ctx->heap);
    for (int i = 0; i < arity; i++) {
        term_put_tuple_element(t, i, elements[i]);
    }
    return t;
}

term lgfx_reply_ok(Context *ctx, lgfx_port_t *port, term result)
{
    return port_create_tuple2(ctx, port->atoms.ok, result);
}

term lgfx_reply_error(Context *ctx, lgfx_port_t *port, term reason_atom)
{
    return port_create_tuple2(ctx, port->atoms.error, reason_atom);
}

term lgfx_reply_error_detail(Context *ctx, lgfx_port_t *port, term reason_atom, term detail)
{
    term elems[2] = { reason_atom, detail };
    term inner = lgfx_make_tuple(ctx, 2, elems);
    if (term_is_invalid_term(inner)) {
        return lgfx_reply_error(ctx, port, port->atoms.no_memory);
    }
    return port_create_tuple2(ctx, port->atoms.error, inner);
}

bool lgfx_is_error_reply(Context *ctx, lgfx_port_t *port, term reply, term *out_reason)
{
    (void) ctx;

    if (!term_is_tuple(reply) || term_get_tuple_arity(reply) != 2) {
        return false;
    }

    term tag = term_get_tuple_element(reply, 0);
    if (tag != port->atoms.error) {
        return false;
    }

    term reason = term_get_tuple_element(reply, 1);

    // Normalize to an atom so it is safe to store across calls.
    // - atom => as-is
    // - {ReasonAtom, Detail} => ReasonAtom
    // - anything else => internal
    if (term_is_tuple(reason) && term_get_tuple_arity(reason) == 2) {
        term maybe_reason_atom = term_get_tuple_element(reason, 0);
        if (term_is_atom(maybe_reason_atom)) {
            reason = maybe_reason_atom;
        } else {
            reason = port->atoms.internal;
        }
    } else if (!term_is_atom(reason)) {
        reason = port->atoms.internal;
    }

    if (out_reason) {
        *out_reason = reason;
    }
    return true;
}
