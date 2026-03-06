// /lgfx_port/validate.c
#include "lgfx_port/validate.h"

#include <stdint.h>

#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"

term lgfx_require_proto_ver(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    if (req->proto_ver != (uint32_t) LGFX_PORT_PROTO_VER) {
        return reply_error(ctx, port, req, port->atoms.bad_proto, 0);
    }

    return term_invalid_term();
}

term lgfx_require_target_domain(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    // Protocol target domain is always 0..254 (0 = LCD, 1..254 = sprite).
    // This check is intentionally independent of per-op target_policy.
    if (req->target > 254u) {
        return reply_error(ctx, port, req, port->atoms.bad_target, 0);
    }

    return term_invalid_term();
}

term lgfx_require_arity_range(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, int min_arity, int max_arity)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    if (min_arity > max_arity) {
        return reply_error(ctx, port, req, port->atoms.internal, 0);
    }

    if (req->arity < min_arity || req->arity > max_arity) {
        return reply_error(ctx, port, req, port->atoms.bad_args, 0);
    }

    return term_invalid_term();
}

term lgfx_require_flags_allowed_mask(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, uint32_t allowed_mask)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    if ((req->flags & ~allowed_mask) != 0u) {
        return reply_error(ctx, port, req, port->atoms.bad_flags, 0);
    }

    return term_invalid_term();
}

static term require_target_zero_reason(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason_atom)
{
    if (req->target != 0u) {
        return reply_error(ctx, port, req, reason_atom, 0);
    }

    return term_invalid_term();
}

static term require_target_sprite_only(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (req->target >= 1u && req->target <= 254u) {
        return term_invalid_term();
    }

    return reply_error(ctx, port, req, port->atoms.bad_target, 0);
}

term lgfx_require_target_policy(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, uint8_t policy)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    switch ((lgfx_op_target_policy_t) policy) {
        case LGFX_OP_TARGET_ANY:
            return term_invalid_term();

        case LGFX_OP_TARGET_BAD_TARGET:
            return require_target_zero_reason(ctx, port, req, port->atoms.bad_target);

        case LGFX_OP_TARGET_UNSUPPORTED:
            return require_target_zero_reason(ctx, port, req, port->atoms.unsupported);

        case LGFX_OP_TARGET_SPRITE_ONLY:
            return require_target_sprite_only(ctx, port, req);

        default:
            return reply_error(ctx, port, req, port->atoms.internal, 0);
    }
}

term lgfx_require_state_policy(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, uint8_t policy)
{
    if (ctx == NULL || port == NULL || req == NULL) {
        return term_invalid_term();
    }

    switch ((lgfx_op_state_policy_t) policy) {
        case LGFX_OP_STATE_ANY:
            return term_invalid_term();

        case LGFX_OP_STATE_REQUIRES_INIT:
            if (port->initialized) {
                return term_invalid_term();
            }

            return reply_error(ctx, port, req, port->atoms.not_initialized, 0);

        default:
            return reply_error(ctx, port, req, port->atoms.internal, 0);
    }
}
