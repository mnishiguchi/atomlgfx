/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/proto_term.c

#include <stddef.h>
#include <stdint.h>

#include "memory.h"
#include "port.h" // port_create_tuple2, etc.

#include "lgfx_port/lgfx_port_internal.h" // atoms, last_error, struct fields
#include "lgfx_port/proto_term.h"

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
    if (out_error_reply != NULL) {
        *out_error_reply = term_invalid_term();
    }

    // NOTE:
    // - port is required to construct protocol error replies (needs atoms).
    // - If port is NULL, we cannot safely allocate a reply tuple here.
    if (port == NULL) {
        if (out_error_reply != NULL) {
            *out_error_reply = term_invalid_term();
        }
        return false;
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

    // NOTE:
    // Envelope validation (proto_ver match, arity bounds, allowed flags, target
    // policy, init-state) is centralized in lgfx_port.c using ops.def metadata.
    //
    // Here we only perform minimal structural decode for the fixed request
    // header. Op-specific payload decode, including float-aligned numeric
    // payloads, happens later in handlers via handler_decode.h helpers.

    term ver_t = term_get_tuple_element(request, 1);
    uint32_t proto_ver = 0;
    if (!lgfx_term_to_u32(ver_t, &proto_ver)) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_proto));
    }

    term op = term_get_tuple_element(request, 2);
    if (!term_is_atom(op)) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_proto));
    }

    term target_t = term_get_tuple_element(request, 3);
    uint32_t target = 0;
    if (!lgfx_term_to_u32(target_t, &target)) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_target));
    }

    term flags_t = term_get_tuple_element(request, 4);
    uint32_t flags = 0;
    if (!lgfx_term_to_u32(flags_t, &flags)) {
        return return_decode_error(out_error_reply, lgfx_reply_error(ctx, port, port->atoms.bad_flags));
    }

    out->proto_ver = proto_ver;
    out->op = op;
    out->target = target;
    out->flags = flags;
    out->request_tuple = request;
    out->arity = arity;

    return true;
}

// -----------------------------------------------------------------------------
// Reply encode helpers (tuple builders)
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
        // best-effort fallback (may still OOM)
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

// -----------------------------------------------------------------------------
// Reply helpers (esp_err mapping + last_error side-effects)
// -----------------------------------------------------------------------------

term lgfx_reply_from_esp_err(Context *ctx, lgfx_port_t *port, esp_err_t err)
{
    switch (err) {
        case ESP_OK:
            return term_invalid_term(); // means "no error"
        case ESP_ERR_INVALID_ARG:
        case ESP_ERR_INVALID_SIZE:
            return lgfx_reply_error(ctx, port, port->atoms.bad_args);
        case ESP_ERR_NO_MEM:
            return lgfx_reply_error(ctx, port, port->atoms.no_memory);
        case ESP_ERR_INVALID_STATE:
            return lgfx_reply_error(ctx, port, port->atoms.internal);
        case ESP_ERR_NOT_SUPPORTED:
            return lgfx_reply_error(ctx, port, port->atoms.unsupported);
        case ESP_ERR_NOT_FOUND:
            return lgfx_reply_error(ctx, port, port->atoms.bad_target);
        default:
            // Optional detail form: {error, {internal, EspErr}}
            return lgfx_reply_error_detail(ctx, port, port->atoms.internal, term_from_int32((int32_t) err));
    }
}

static inline void encode_oom_last_error(lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_last_error_set(
        port,
        req->op,
        port->atoms.no_memory,
        req->flags,
        req->target,
        (int32_t) ESP_ERR_NO_MEM);
}

term lgfx_reply_from_esp_err_req(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, esp_err_t err)
{
    if (err == ESP_OK) {
        return term_invalid_term();
    }

    term reply = lgfx_reply_from_esp_err(ctx, port, err);

    if (term_is_invalid_term(reply)) {
        // OOM while trying to allocate the reply tuple
        encode_oom_last_error(port, req);
        return term_invalid_term();
    }

    term reason = term_invalid_term();
    if (lgfx_is_error_reply(ctx, port, reply, &reason)) {
        lgfx_last_error_set(port, req->op, reason, req->flags, req->target, (int32_t) err);
    } else {
        lgfx_last_error_set(port, req->op, port->atoms.internal, req->flags, req->target, (int32_t) err);
    }

    return reply;
}

term lgfx_reply_ok_req(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term payload)
{
    term reply = lgfx_reply_ok(ctx, port, payload);

    if (term_is_invalid_term(reply)) {
        encode_oom_last_error(port, req);
        return term_invalid_term();
    }

    return reply;
}

term lgfx_reply_error_req(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason, int32_t esp_err)
{
    lgfx_last_error_set(port, req->op, reason, req->flags, req->target, esp_err);

    term reply = lgfx_reply_error(ctx, port, reason);

    if (term_is_invalid_term(reply)) {
        encode_oom_last_error(port, req);
        return term_invalid_term();
    }

    return reply;
}

term lgfx_reply_error_detail_req(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *req,
    term reason,
    term detail,
    int32_t esp_err)
{
    lgfx_last_error_set(port, req->op, reason, req->flags, req->target, esp_err);

    term reply = lgfx_reply_error_detail(ctx, port, reason, detail);

    if (term_is_invalid_term(reply)) {
        encode_oom_last_error(port, req);
        return term_invalid_term();
    }

    return reply;
}
