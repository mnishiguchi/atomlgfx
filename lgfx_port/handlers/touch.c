// lgfx_port/handlers/touch.c
#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "esp_err.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/worker.h"

// Request envelope validation (version/arity/flags/target/init-state) is
// centralized in lgfx_port.c via ops.def metadata. Handlers only decode payload fields.

static term make_touch_tuple_or_none(
    Context *ctx,
    lgfx_port_t *port,
    bool touched,
    int16_t x,
    int16_t y,
    uint16_t size)
{
    if (!touched) {
        return port->atoms.none;
    }

    term elems[3] = {
        term_from_int32((int32_t) x),
        term_from_int32((int32_t) y),
        term_from_int32((int32_t) size),
    };

    return lgfx_make_tuple(ctx, 3, elems);
}

static term make_u16_tuple_8(Context *ctx, const uint16_t params[8])
{
    term elems[8] = {
        term_from_int32((int32_t) params[0]),
        term_from_int32((int32_t) params[1]),
        term_from_int32((int32_t) params[2]),
        term_from_int32((int32_t) params[3]),
        term_from_int32((int32_t) params[4]),
        term_from_int32((int32_t) params[5]),
        term_from_int32((int32_t) params[6]),
        term_from_int32((int32_t) params[7]),
    };

    return lgfx_make_tuple(ctx, 8, elems);
}

term lgfx_handle_getTouchRaw(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    bool touched = false;
    int16_t x = 0;
    int16_t y = 0;
    uint16_t size = 0;

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_get_touch_raw(port, &touched, &x, &y, &size));

    term payload = make_touch_tuple_or_none(ctx, port, touched, x, y, size);
    if (term_is_invalid_term(payload)) {
        return reply_error(ctx, port, req, port->atoms.no_memory, (int32_t) ESP_ERR_NO_MEM);
    }

    return reply_ok(ctx, port, req, payload);
}

term lgfx_handle_getTouch(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    bool touched = false;
    int16_t x = 0;
    int16_t y = 0;
    uint16_t size = 0;

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_get_touch(port, &touched, &x, &y, &size));

    term payload = make_touch_tuple_or_none(ctx, port, touched, x, y, size);
    if (term_is_invalid_term(payload)) {
        return reply_error(ctx, port, req, port->atoms.no_memory, (int32_t) ESP_ERR_NO_MEM);
    }

    return reply_ok(ctx, port, req, payload);
}

term lgfx_handle_setTouchCalibrate(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint16_t params[8] = { 0 };

    // {lgfx, ver, setTouchCalibrate, target, flags, P0, P1, P2, P3, P4, P5, P6, P7}
    for (int i = 0; i < 8; i++) {
        uint16_t v = 0;
        if (!lgfx_decode_u16_at(req, 5 + i, &v)) {
            return reply_error(ctx, port, req, port->atoms.bad_args, 0);
        }
        params[i] = v;
    }

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_set_touch_calibrate(port, params));
    return reply_ok(ctx, port, req, port->atoms.ok);
}

term lgfx_handle_calibrateTouch(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req)
{
    uint16_t params[8] = { 0 };

    LGFX_RETURN_IF_ESP_ERR(ctx, port, req, lgfx_worker_device_calibrate_touch(port, params));

    term payload = make_u16_tuple_8(ctx, params);
    if (term_is_invalid_term(payload)) {
        return reply_error(ctx, port, req, port->atoms.no_memory, (int32_t) ESP_ERR_NO_MEM);
    }

    return reply_ok(ctx, port, req, payload);
}
