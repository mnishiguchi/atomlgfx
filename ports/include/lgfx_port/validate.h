// ports/include/lgfx_port/validate.h
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/op_meta.h"
#include "lgfx_port/term_decode.h"
#include "lgfx_port/term_encode.h"

static inline bool lgfx_validate_color_depth(uint32_t d)
{
    switch (d) {
        case 1:
        case 2:
        case 4:
        case 8:
        case 16:
        case 24:
            return true;
        default:
            return false;
    }
}

static inline term lgfx_require_arity_exact(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, int expected_arity)
{
    if (req->arity != expected_arity) {
        lgfx_last_error_set(port, req->op, port->atoms.bad_args, req->flags, req->target, 0);
        return lgfx_reply_error(ctx, port, port->atoms.bad_args);
    }
    return term_invalid_term();
}

static inline term lgfx_require_flags_zero(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (req->flags != 0) {
        lgfx_last_error_set(port, req->op, port->atoms.bad_flags, req->flags, req->target, 0);
        return lgfx_reply_error(ctx, port, port->atoms.bad_flags);
    }
    return term_invalid_term();
}

static inline term lgfx_require_flags_allowed_mask(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, uint32_t allowed_mask)
{
    if ((req->flags & ~allowed_mask) != 0) {
        lgfx_last_error_set(port, req->op, port->atoms.bad_flags, req->flags, req->target, 0);
        return lgfx_reply_error(ctx, port, port->atoms.bad_flags);
    }
    return term_invalid_term();
}

// Some ops want bad_target for Target!=0, others want unsupported.
static inline term lgfx_require_target_zero_reason(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason_atom)
{
    if (req->target != 0) {
        lgfx_last_error_set(port, req->op, reason_atom, req->flags, req->target, 0);
        return lgfx_reply_error(ctx, port, reason_atom);
    }
    return term_invalid_term();
}

static inline term lgfx_require_target_zero_bad_target(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return lgfx_require_target_zero_reason(ctx, port, req, port->atoms.bad_target);
}

static inline term lgfx_require_target_zero_unsupported(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return lgfx_require_target_zero_reason(ctx, port, req, port->atoms.unsupported);
}

static inline term lgfx_require_target_sprite_only(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    // Valid sprite handles are 1..254 (255 is reserved invalid).
    if (req->target >= 1 && req->target <= 254) {
        return term_invalid_term();
    }

    lgfx_last_error_set(port, req->op, port->atoms.bad_target, req->flags, req->target, 0);
    return lgfx_reply_error(ctx, port, port->atoms.bad_target);
}

static inline term lgfx_require_arity_range(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, int min_arity, int max_arity)
{
    if (min_arity > max_arity) {
        lgfx_last_error_set(port, req->op, port->atoms.internal, req->flags, req->target, 0);
        return lgfx_reply_error(ctx, port, port->atoms.internal);
    }

    if (req->arity < min_arity || req->arity > max_arity) {
        lgfx_last_error_set(port, req->op, port->atoms.bad_args, req->flags, req->target, 0);
        return lgfx_reply_error(ctx, port, port->atoms.bad_args);
    }

    return term_invalid_term();
}

static inline term lgfx_require_target_policy(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, lgfx_op_target_policy_t policy)
{
    switch (policy) {
        case LGFX_OP_TARGET_ANY:
            return term_invalid_term();

        case LGFX_OP_TARGET_BAD_TARGET:
            return lgfx_require_target_zero_bad_target(ctx, port, req);

        case LGFX_OP_TARGET_UNSUPPORTED:
            return lgfx_require_target_zero_unsupported(ctx, port, req);

        case LGFX_OP_TARGET_SPRITE_ONLY:
            return lgfx_require_target_sprite_only(ctx, port, req);

        default:
            lgfx_last_error_set(port, req->op, port->atoms.internal, req->flags, req->target, 0);
            return lgfx_reply_error(ctx, port, port->atoms.internal);
    }
}

static inline term lgfx_require_state_policy(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, lgfx_op_state_policy_t policy)
{
    switch (policy) {
        case LGFX_OP_STATE_ANY:
            return term_invalid_term();

        case LGFX_OP_STATE_REQUIRES_INIT:
            if (port->initialized) {
                return term_invalid_term();
            }
            lgfx_last_error_set(port, req->op, port->atoms.not_initialized, req->flags, req->target, 0);
            return lgfx_reply_error(ctx, port, port->atoms.not_initialized);

        default:
            lgfx_last_error_set(port, req->op, port->atoms.internal, req->flags, req->target, 0);
            return lgfx_reply_error(ctx, port, port->atoms.internal);
    }
}
