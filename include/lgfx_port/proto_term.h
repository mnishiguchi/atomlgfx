// lgfx_port/include/lgfx_port/proto_term.h
#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "context.h"
#include "term.h" // AtomVM core term API

#include "lgfx_port/lgfx_port.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// Request decode surface
// ----------------------------------------------------------------------------
typedef struct
{
    uint32_t proto_ver;
    term op; // atom
    uint32_t target; // 0..254
    uint32_t flags; // u32
    term request_tuple; // original request tuple
    int arity; // tuple arity
} lgfx_request_t;

// Decode {lgfx, ProtoVer, Op, Target, Flags, ...}
bool lgfx_term_decode_request(
    Context *ctx,
    lgfx_port_t *port,
    term request,
    lgfx_request_t *out,
    term *out_error_reply);

// Reply constructors
term lgfx_reply_ok(Context *ctx, lgfx_port_t *port, term result);
term lgfx_reply_error(Context *ctx, lgfx_port_t *port, term reason_atom);
term lgfx_reply_error_detail(Context *ctx, lgfx_port_t *port, term reason_atom, term detail);

// Helpers for structured tuples (small arities used by getCaps/getLastError)
term lgfx_make_tuple(Context *ctx, int arity, const term *elements);

// Inspect reply shape
bool lgfx_is_error_reply(Context *ctx, lgfx_port_t *port, term reply, term *out_reason);

// ----------------------------------------------------------------------------
// Term utilities
// ----------------------------------------------------------------------------
static inline bool lgfx_validate_i16(int32_t v)
{
    return v >= -32768 && v <= 32767;
}

static inline bool lgfx_validate_u16(uint32_t v)
{
    return v <= 65535u;
}

static inline bool lgfx_validate_color888(uint32_t c)
{
    // 0x00RRGGBB only
    return (c & 0xFF000000u) == 0;
}

static inline bool lgfx_term_to_u32(term t, uint32_t *out)
{
    if (!term_is_integer(t)) {
        return false;
    }

    avm_int_t v = term_to_int(t);
    if (v < 0) {
        return false;
    }

    uint64_t u = (uint64_t) v;
    if (u > UINT32_MAX) {
        return false;
    }

    *out = (uint32_t) u;
    return true;
}

static inline bool lgfx_term_to_i32(term t, int32_t *out)
{
    if (!term_is_integer(t)) {
        return false;
    }

    avm_int_t v = term_to_int(t);
    if (v < (avm_int_t) INT32_MIN || v > (avm_int_t) INT32_MAX) {
        return false;
    }

    *out = (int32_t) v;
    return true;
}

static inline bool lgfx_term_to_i16(term t, int16_t *out)
{
    int32_t v = 0;
    if (!lgfx_term_to_i32(t, &v) || !lgfx_validate_i16(v)) {
        return false;
    }

    *out = (int16_t) v;
    return true;
}

static inline bool lgfx_term_to_u16(term t, uint16_t *out)
{
    uint32_t v = 0;
    if (!lgfx_term_to_u32(t, &v) || !lgfx_validate_u16(v)) {
        return false;
    }

    *out = (uint16_t) v;
    return true;
}

static inline uint16_t lgfx_color888_to_rgb565(uint32_t color888)
{
    const uint8_t r = (uint8_t) ((color888 >> 16) & 0xFFu);
    const uint8_t g = (uint8_t) ((color888 >> 8) & 0xFFu);
    const uint8_t b = (uint8_t) (color888 & 0xFFu);

    return (uint16_t) (((uint16_t) (r & 0xF8u) << 8)
        | ((uint16_t) (g & 0xFCu) << 3)
        | ((uint16_t) (b >> 3)));
}

static inline bool lgfx_term_to_color565(term color_t, uint16_t *out_color565)
{
    uint32_t color888 = 0;

    if (!lgfx_term_to_u32(color_t, &color888)) {
        return false;
    }
    if (!lgfx_validate_color888(color888)) {
        return false;
    }

    *out_color565 = lgfx_color888_to_rgb565(color888);
    return true;
}

// ----------------------------------------------------------------------------
// Reply helpers
// ----------------------------------------------------------------------------

// esp_err -> protocol reply mapping
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

static inline void reply_encode_oom_last_error(lgfx_port_t *port, const lgfx_request_t *req)
{
    lgfx_last_error_set(
        port,
        req->op,
        port->atoms.no_memory,
        req->flags,
        req->target,
        (int32_t) ESP_ERR_NO_MEM);
}

static inline term reply_from_esp_err(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, esp_err_t err)
{
    if (err == ESP_OK) {
        return term_invalid_term();
    }

    term reply = lgfx_reply_from_esp_err(ctx, port, err);

    if (term_is_invalid_term(reply)) {
        reply_encode_oom_last_error(port, req);
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

#define LGFX_RETURN_IF_ESP_ERR(ctx, port, req, esp_expr)            \
    do {                                                            \
        esp_err_t __err = (esp_expr);                               \
        if (__err != ESP_OK) {                                      \
            return reply_from_esp_err((ctx), (port), (req), __err); \
        }                                                           \
    } while (0)

static inline term reply_ok(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term payload)
{
    term reply = lgfx_reply_ok(ctx, port, payload);

    if (term_is_invalid_term(reply)) {
        reply_encode_oom_last_error(port, req);
        return term_invalid_term();
    }

    return reply;
}

static inline term reply_error(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason, int32_t esp_err)
{
    lgfx_last_error_set(port, req->op, reason, req->flags, req->target, esp_err);

    term reply = lgfx_reply_error(ctx, port, reason);

    if (term_is_invalid_term(reply)) {
        reply_encode_oom_last_error(port, req);
        return term_invalid_term();
    }

    return reply;
}

static inline term reply_error_detail(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason, term detail, int32_t esp_err)
{
    lgfx_last_error_set(port, req->op, reason, req->flags, req->target, esp_err);

    term reply = lgfx_reply_error_detail(ctx, port, reason, detail);

    if (term_is_invalid_term(reply)) {
        reply_encode_oom_last_error(port, req);
        return term_invalid_term();
    }

    return reply;
}

#ifdef __cplusplus
}
#endif
