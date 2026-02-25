// ports/include/lgfx_port/reply_common.h
#pragma once

#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/esp_err_map.h" // esp_err_t, ESP_ERR_NO_MEM, lgfx_reply_from_esp_err(...)
#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/term_decode.h" // lgfx_request_t
#include "lgfx_port/term_encode.h"

#ifdef __cplusplus
extern "C" {
#endif

// Common reply helpers shared by request handlers.
//
// Purpose:
// - Keep reply behavior consistent across handlers.
// - Ensure last_error is recorded on all error paths.
// - Ensure reply-encoding OOM also updates last_error.
// - Preserve the current esp_err -> protocol reply mapping behavior.

// Evaluate an esp_err_t expression and return an encoded error reply if needed.
// - On ESP_OK: no-op
// - On error: returns reply_from_esp_err(...), which also updates last_error
// - If reply encoding fails (OOM): returns invalid_term and records no_memory
#define LGFX_RETURN_IF_ESP_ERR(ctx, port, req, esp_expr)            \
    do {                                                            \
        esp_err_t __err = (esp_expr);                               \
        if (__err != ESP_OK) {                                      \
            return reply_from_esp_err((ctx), (port), (req), __err); \
        }                                                           \
    } while (0)

static inline void reply_encode_oom_last_error(lgfx_port_t *port, const lgfx_request_t *req)
{
    // The most recent failure is reply encoding (out of memory).
    lgfx_last_error_set(
        port,
        req->op,
        port->atoms.no_memory,
        req->flags,
        req->target,
        (int32_t) ESP_ERR_NO_MEM);
}

static inline term reply_ok(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term payload)
{
    term reply = lgfx_reply_ok(ctx, port, payload);

    if (term_is_invalid_term(reply)) {
        // The operation may already have succeeded, but reply encoding failed.
        reply_encode_oom_last_error(port, req);
        return term_invalid_term();
    }

    return reply;
}

static inline term reply_error(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason, int32_t esp_err)
{
    // Record the intended protocol error before encoding the reply.
    lgfx_last_error_set(port, req->op, reason, req->flags, req->target, esp_err);

    term reply = lgfx_reply_error(ctx, port, reason);

    if (term_is_invalid_term(reply)) {
        // Reply encoding failed (OOM); record that as the latest error.
        reply_encode_oom_last_error(port, req);
        return term_invalid_term();
    }

    return reply;
}

// Useful for handlers that return {error, {Reason, Detail}}.
static inline term reply_error_detail(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason, term detail, int32_t esp_err)
{
    // Record the intended protocol error before encoding the reply.
    lgfx_last_error_set(port, req->op, reason, req->flags, req->target, esp_err);

    term reply = lgfx_reply_error_detail(ctx, port, reason, detail);

    if (term_is_invalid_term(reply)) {
        // Reply encoding failed (OOM); record that as the latest error.
        reply_encode_oom_last_error(port, req);
        return term_invalid_term();
    }

    return reply;
}

static inline term reply_from_esp_err(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, esp_err_t err)
{
    // Decide whether to produce an error reply from err itself, not from encoding outcome.
    if (err == ESP_OK) {
        return term_invalid_term(); // no error reply
    }

    term reply = lgfx_reply_from_esp_err(ctx, port, err);

    // If reply encoding fails (for example OOM), do not let callers treat it as success.
    if (term_is_invalid_term(reply)) {
        reply_encode_oom_last_error(port, req);
        return term_invalid_term();
    }

    term reason = term_invalid_term();
    if (lgfx_is_error_reply(ctx, port, reply, &reason)) {
        lgfx_last_error_set(port, req->op, reason, req->flags, req->target, (int32_t) err);
    } else {
        // Defensive fallback for an unexpected reply shape.
        lgfx_last_error_set(port, req->op, port->atoms.internal, req->flags, req->target, (int32_t) err);
    }

    return reply;
}

#ifdef __cplusplus
}
#endif
