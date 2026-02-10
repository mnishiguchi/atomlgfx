#pragma once

#include "esp_err.h"
#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_encode.h"
#include "term.h"
#include <stdint.h>

static inline term lgfx_reply_from_esp_err(Context *ctx, lgfx_port_t *port, esp_err_t err)
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
