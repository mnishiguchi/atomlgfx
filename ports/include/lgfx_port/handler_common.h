// ports/include/lgfx_port/handler_common.h
#pragma once

#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/dispatch_table.h"
#include "lgfx_port/esp_err_map.h" // esp_err_t
#include "lgfx_port/term_encode.h"

// Common handler helpers (shared across control/setup/primitives/text/images)
//
// Goals:
// - Keep semantics identical across handlers to prevent drift.
// - Ensure "last_error always set" on error paths.
// - Preserve the current reply_from_esp_err behavior used in setup/text/images/primitives.

// AtomGL-style helper: evaluate an esp_err_t expression and return an encoded error reply if needed.
// - On ESP_OK: does nothing.
// - On error: returns reply_from_esp_err(...), which also updates last_error.
// - If encoding fails (OOM): reply_from_esp_err returns invalid_term (no reply), but last_error is set.
#define LGFX_RETURN_IF_ESP_ERR(ctx, port, req, esp_expr)            \
    do {                                                            \
        esp_err_t __err = (esp_expr);                               \
        if (__err != ESP_OK) {                                      \
            return reply_from_esp_err((ctx), (port), (req), __err); \
        }                                                           \
    } while (0)

static inline term reply_error(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason, int32_t esp_err)
{
    lgfx_last_error_set(port, req->op, reason, req->flags, req->target, esp_err);
    return lgfx_reply_error(ctx, port, reason);
}

static inline term reply_from_esp_err(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, esp_err_t err)
{
    // Decide "success vs error" based on err, not on whether encoding succeeded.
    if (err == ESP_OK) {
        return term_invalid_term(); // no error reply
    }

    term reply = lgfx_reply_from_esp_err(ctx, port, err);

    // If encoding failed (e.g., OOM), do not let callers interpret it as success.
    if (term_is_invalid_term(reply)) {
        lgfx_last_error_set(port, req->op, port->atoms.no_memory, req->flags, req->target, (int32_t) err);
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
