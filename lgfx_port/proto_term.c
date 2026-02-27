// lgfx_port/proto_term.c
#include "lgfx_port/proto_term.h"

#include <stddef.h>
#include <stdint.h>

#include "memory.h"
#include "port.h" // port_create_tuple2, etc.

// -----------------------------------------------------------------------------
// Request decode
// -----------------------------------------------------------------------------

static inline bool return_decode_error(term *out_error_reply, term reply)
{
    if (out_error_reply == NULL) {
        return false;
    }

    if (term_is_invalid_term(reply)) {
        *out_error_reply = term_invalid_term();
        return false;
    }

    *out_error_reply = reply;
    return false;
}

bool lgfx_term_decode_request(
    Context *ctx,
    lgfx_port_t *port,
    term request,
    lgfx_request_t *out,
    term *out_error_reply)
{
    (void) ctx;

    if (out_error_reply != NULL) {
        *out_error_reply = term_invalid_term();
    }

    if (out == NULL) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_proto));
    }

    if (!term_is_tuple(request)) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_proto));
    }

    int arity = term_get_tuple_arity(request);
    if (arity < 5) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_proto));
    }

    // Tuple shape: {lgfx, ProtoVer, Op, Target, Flags, ...}
    term tag = term_get_tuple_element(request, 0);
    if (tag != port->atoms.lgfx) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_proto));
    }

    term ver_t = term_get_tuple_element(request, 1);
    if (!term_is_integer(ver_t)) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_proto));
    }

    avm_int_t ver_i = term_to_int(ver_t);
    if (ver_i < 0 || (uint32_t) ver_i != (uint32_t) LGFX_PORT_PROTO_VER) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_proto));
    }

    term op = term_get_tuple_element(request, 2);
    if (!term_is_atom(op)) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_proto));
    }

    term target_t = term_get_tuple_element(request, 3);
    uint32_t target = 0;
    if (!lgfx_term_to_u32(target_t, &target) || target > 254u) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_target));
    }

    term flags_t = term_get_tuple_element(request, 4);
    uint32_t flags = 0;
    if (!lgfx_term_to_u32(flags_t, &flags)) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_flags));
    }

    out->proto_ver = (uint32_t) ver_i;
    out->op = op;
    out->target = target;
    out->flags = flags;
    out->request_tuple = request;
    out->arity = arity;

    return true;
}

// -----------------------------------------------------------------------------
// Reply encode helpers
// -----------------------------------------------------------------------------

term lgfx_make_tuple(Context *ctx, int arity, const term *elements)
{
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
