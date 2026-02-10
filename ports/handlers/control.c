// ports/handlers/control.c
#include "lgfx_port/handlers/control.h"

#include <stdint.h>

#include "lgfx_port/caps.h"
#include "lgfx_port/handler_common.h"
#include "lgfx_port/proto_caps.h"
#include "lgfx_port/term_encode.h"
#include "term.h"

#ifndef LGFX_PORT_SUPPORTS_LAST_ERROR
#define LGFX_PORT_SUPPORTS_LAST_ERROR 1
#endif

// Envelope checks (version/arity/flags/target/init-state) are centralized in
// lgfx_port.c via ops.def metadata. Handlers here only decode payload fields.

static term do_get_caps(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint32_t feature_bits = lgfx_proto_feature_bits();

    uint32_t max_sprites = (feature_bits & (uint32_t) LGFX_CAP_SPRITE) ? (uint32_t) LGFX_PORT_MAX_SPRITES : 0u;

    term elems[5] = {
        port->atoms.caps,
        term_from_int32((int32_t) LGFX_PORT_PROTO_VER),
        term_from_int32((int32_t) LGFX_PORT_MAX_BINARY_BYTES),
        term_from_int32((int32_t) max_sprites),
        term_from_int32((int32_t) feature_bits)
    };

    term payload = lgfx_make_tuple(ctx, 5, elems);
    if (term_is_invalid_term(payload)) {
        // Important: this must set last_error too.
        return reply_error(ctx, port, req, port->atoms.no_memory, 0);
    }

    return lgfx_reply_ok(ctx, port, payload);
}

static term do_get_last_error(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
#if LGFX_PORT_SUPPORTS_LAST_ERROR
    // Snapshot first; clear only after we successfully encode + return the payload.
    lgfx_last_error_t e = port->last_error;

    term elems[6] = {
        port->atoms.last_error,
        e.last_op,
        e.reason,
        term_from_int32((int32_t) e.flags),
        term_from_int32((int32_t) e.target),
        term_from_int32((int32_t) e.esp_err)
    };

    term payload = lgfx_make_tuple(ctx, 6, elems);
    if (term_is_invalid_term(payload)) {
        // Important: this must set last_error too.
        return reply_error(ctx, port, req, port->atoms.no_memory, 0);
    }

    lgfx_last_error_clear(port);
    return lgfx_reply_ok(ctx, port, payload);
#else
    return reply_error(ctx, port, req, port->atoms.unsupported, 0);
#endif
}

term lgfx_handle_ping(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    (void) req;
    return lgfx_reply_ok(ctx, port, port->atoms.pong);
}

term lgfx_handle_getCaps(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_get_caps(ctx, port, req);
}

term lgfx_handle_getLastError(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_get_last_error(ctx, port, req);
}

// Envelope (arity/flags/target/init-state) is validated centrally in lgfx_port.c from ops.def metadata.
term lgfx_handle_width(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    (void) req;
    return lgfx_reply_ok(ctx, port, term_from_int32((int32_t) port->width));
}

term lgfx_handle_height(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    (void) req;
    return lgfx_reply_ok(ctx, port, term_from_int32((int32_t) port->height));
}
