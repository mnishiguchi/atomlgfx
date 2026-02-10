// ports/proto_term_decode.c

#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_conv.h"
#include "lgfx_port/term_decode.h"
#include "lgfx_port/term_encode.h" // for lgfx_reply_error()

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
