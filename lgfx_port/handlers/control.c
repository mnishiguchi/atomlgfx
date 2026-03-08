// lgfx_port/handlers/control.c
//
// Control-plane handlers:
// - ping / getCaps / getLastError
// - init / close
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "esp_err.h"

#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/worker.h"

static void refresh_cached_dims(lgfx_port_t *port)
{
    uint16_t w = 0;
    uint16_t h = 0;

    if (lgfx_worker_device_get_dims(port, &w, &h) == ESP_OK) {
        port->width = (uint32_t) w;
        port->height = (uint32_t) h;
    }
}

term lgfx_handle_ping(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    return reply_ok(ctx, port, req, port->atoms.pong);
}

term lgfx_handle_getCaps(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint32_t feature_bits = lgfx_port_feature_bits(port);
    uint32_t max_sprites = (uint32_t) lgfx_port_max_sprites(port);

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

term lgfx_handle_getLastError(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
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
        return reply_error(ctx, port, req, port->atoms.no_memory, (int32_t) ESP_ERR_NO_MEM);
    }

    term reply = reply_ok(ctx, port, req, payload);
    if (term_is_invalid_term(reply)) {
        return reply;
    }

    lgfx_last_error_clear(port);
    return reply;
}

term lgfx_handle_init(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (port->initialized) {
        return reply_ok(ctx, port, req, port->atoms.ok);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_init(port));

    port->initialized = true;
    lgfx_last_error_clear(port);
    refresh_cached_dims(port);

    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_close(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    if (!port->initialized) {
        lgfx_last_error_clear(port);
        return reply_ok(ctx, port, req, port->atoms.ok);
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_close(port));

    port->initialized = false;
    port->width = 0;
    port->height = 0;

    lgfx_last_error_clear(port);
    return reply_ok(ctx, port, req, port->atoms.ok);
}
