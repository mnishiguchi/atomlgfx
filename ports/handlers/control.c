// ports/handlers/control.c
#include "lgfx_port/handlers/control.h"

#include <stdint.h>

#include "lgfx_port/caps.h"
#include "lgfx_port/reply_common.h"
#include "lgfx_port/proto_caps.h"
#include "lgfx_port/term_encode.h"
#include "term.h"

// Request envelope validation (version/arity/flags/target/init-state) is
// centralized in lgfx_port.c via ops.def metadata. Handlers only decode payload fields.

static term do_get_caps(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint32_t feature_bits = lgfx_proto_feature_bits();
    uint32_t max_sprites = (uint32_t) lgfx_proto_max_sprites();

    term elems[5] = {
        port->atoms.caps,
        term_from_int32((int32_t) LGFX_PORT_PROTO_VER),
        term_from_int32((int32_t) LGFX_PORT_MAX_BINARY_BYTES),
        term_from_int32((int32_t) max_sprites),
        term_from_int32((int32_t) feature_bits)
    };

    term payload = lgfx_make_tuple(ctx, 5, elems);
    if (term_is_invalid_term(payload)) {
        return reply_error(ctx, port, req, port->atoms.no_memory, (int32_t) ESP_ERR_NO_MEM);
    }

    return reply_ok(ctx, port, req, payload);
}

static term do_get_last_error(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
#if LGFX_PORT_SUPPORTS_LAST_ERROR
    // Snapshot first. Clear only after the final {ok, Payload} reply is encoded.
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
        // Payload encoding failed; record no_memory as the latest error.
        return reply_error(ctx, port, req, port->atoms.no_memory, (int32_t) ESP_ERR_NO_MEM);
    }

    term reply = reply_ok(ctx, port, req, payload);
    if (term_is_invalid_term(reply)) {
        // reply_ok already recorded last_error = no_memory.
        return reply;
    }

    lgfx_last_error_clear(port);
    return reply;
#else
    return reply_error(ctx, port, req, port->atoms.unsupported, 0);
#endif
}

term lgfx_handle_ping(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return reply_ok(ctx, port, req, port->atoms.pong);
}

term lgfx_handle_getCaps(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_get_caps(ctx, port, req);
}

term lgfx_handle_getLastError(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return do_get_last_error(ctx, port, req);
}

// Request envelope validation is centralized in lgfx_port.c via ops.def metadata.
term lgfx_handle_width(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return reply_ok(ctx, port, req, term_from_int32((int32_t) port->width));
}

term lgfx_handle_height(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return reply_ok(ctx, port, req, term_from_int32((int32_t) port->height));
}
