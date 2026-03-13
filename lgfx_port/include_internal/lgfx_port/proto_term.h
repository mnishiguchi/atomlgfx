// lgfx_port/include_internal/lgfx_port/proto_term.h
#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#include "context.h"
#include "term.h" // AtomVM core term API

// -----------------------------------------------------------------------------
// AtomVM compatibility
// -----------------------------------------------------------------------------

// AtomVM deprecated term_from_int32() (unsafe on some targets). In this port we
// only encode small integers that fit in avm_int_t, so use term_from_int().
// Keep call sites unchanged by shadowing the deprecated function name.
#ifndef LGFX_PORT_ALLOW_DEPRECATED_TERM_FROM_INT32
#ifdef term_from_int32
#undef term_from_int32
#endif
#define term_from_int32(v) term_from_int((avm_int_t) (v))
#endif

#include "lgfx_port/protocol.h" // protocol constants / limits

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration; implementations live in proto_term.c.
typedef struct lgfx_port_t lgfx_port_t;

// -----------------------------------------------------------------------------
// Request decode surface
// -----------------------------------------------------------------------------

typedef struct lgfx_request_t
{
    uint32_t proto_ver;
    term op;
    uint32_t target;
    uint32_t flags;
    term request_tuple;
    int arity;
} lgfx_request_t;

// Decode {lgfx, ProtoVer, Op, Target, Flags, ...}.
// Structural decode only; policy validation happens in lgfx_port.c.
bool lgfx_term_decode_request(
    Context *ctx,
    lgfx_port_t *port,
    term request,
    lgfx_request_t *out,
    term *out_error_reply);

// Raw reply constructors; no last_error side-effects.
term lgfx_reply_ok(Context *ctx, lgfx_port_t *port, term result);
term lgfx_reply_error(Context *ctx, lgfx_port_t *port, term reason_atom);
term lgfx_reply_error_detail(Context *ctx, lgfx_port_t *port, term reason_atom, term detail);

// Small tuple helper.
term lgfx_make_tuple(Context *ctx, int arity, const term *elements);

// Reply inspection.
bool lgfx_is_error_reply(Context *ctx, lgfx_port_t *port, term reply, term *out_reason);

// -----------------------------------------------------------------------------
// Term utilities
// -----------------------------------------------------------------------------

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
    // 0x00RRGGBB only.
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

// -----------------------------------------------------------------------------
// Reply helpers with last_error side-effects
// -----------------------------------------------------------------------------

// esp_err -> protocol reply mapping without last_error side-effects.
term lgfx_reply_from_esp_err(Context *ctx, lgfx_port_t *port, esp_err_t err);

// esp_err -> protocol reply mapping with last_error update.
term lgfx_reply_from_esp_err_req(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, esp_err_t err);

// Reply constructors with last_error update.
term lgfx_reply_ok_req(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term payload);
term lgfx_reply_error_req(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason, int32_t esp_err);
term lgfx_reply_error_detail_req(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *req,
    term reason,
    term detail,
    int32_t esp_err);

// Handler-facing wrappers.
#define LGFX_RETURN_IF_ESP_ERR(ctx, port, req, esp_expr)                     \
    do {                                                                     \
        esp_err_t __err = (esp_expr);                                        \
        if (__err != ESP_OK) {                                               \
            return lgfx_reply_from_esp_err_req((ctx), (port), (req), __err); \
        }                                                                    \
    } while (0)

static inline term reply_ok(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term payload)
{
    return lgfx_reply_ok_req(ctx, port, req, payload);
}

static inline term reply_error(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason, int32_t esp_err)
{
    return lgfx_reply_error_req(ctx, port, req, reason, esp_err);
}

static inline term reply_error_detail(
    Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, term reason, term detail, int32_t esp_err)
{
    return lgfx_reply_error_detail_req(ctx, port, req, reason, detail, esp_err);
}

#ifdef __cplusplus
}
#endif
