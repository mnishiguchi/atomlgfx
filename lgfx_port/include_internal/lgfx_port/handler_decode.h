// lgfx_port/include_internal/lgfx_port/handler_decode.h
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "term.h"

#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/protocol.h"
#include "lgfx_port/worker.h"

// ----------------------------------------------------------------------------
// Tiny decode helpers for handlers
//
// Handlers should only decode payload fields. Envelope validation is centralized
// in lgfx_port.c via ops.def metadata.
//
// Conventions:
// - Return bool for decode success/failure.
// - Callers map failure to {error, bad_args} (or other) consistently.
// ----------------------------------------------------------------------------

static inline term lgfx_req_elem(const lgfx_request_t *req, int index)
{
    return term_get_tuple_element(req->request_tuple, index);
}

// ----------------------------------------------------------------------------
// Tiny shared state helpers for handlers
// ----------------------------------------------------------------------------

static inline void lgfx_refresh_cached_dims(lgfx_port_t *port)
{
    uint16_t w = 0;
    uint16_t h = 0;

    if (lgfx_worker_device_get_dims(port, &w, &h) == ESP_OK) {
        port->width = (uint32_t) w;
        port->height = (uint32_t) h;
    }
}

static inline bool lgfx_decode_u32_at(const lgfx_request_t *req, int index, uint32_t *out)
{
    return lgfx_term_to_u32(lgfx_req_elem(req, index), out);
}

static inline bool lgfx_decode_i32_at(const lgfx_request_t *req, int index, int32_t *out)
{
    return lgfx_term_to_i32(lgfx_req_elem(req, index), out);
}

static inline bool lgfx_decode_u16_at(const lgfx_request_t *req, int index, uint16_t *out)
{
    return lgfx_term_to_u16(lgfx_req_elem(req, index), out);
}

static inline bool lgfx_decode_text_scale_x256_at(const lgfx_request_t *req, int index, uint16_t *out)
{
    uint32_t value = 0;
    if (!out || !lgfx_decode_u32_at(req, index, &value) || !lgfx_validate_text_scale_x256(value)) {
        return false;
    }

    *out = (uint16_t) value;
    return true;
}

static inline bool lgfx_decode_i16_at(const lgfx_request_t *req, int index, int16_t *out)
{
    return lgfx_term_to_i16(lgfx_req_elem(req, index), out);
}

static inline bool lgfx_decode_u8_at(const lgfx_request_t *req, int index, uint8_t *out)
{
    uint32_t v = 0;
    if (!lgfx_decode_u32_at(req, index, &v) || v > 255u) {
        return false;
    }
    *out = (uint8_t) v;
    return true;
}

static inline bool lgfx_decode_color565_at(const lgfx_request_t *req, int index, uint16_t *out)
{
    return lgfx_term_to_color565(lgfx_req_elem(req, index), out);
}

static inline bool lgfx_decode_rgb888_at(const lgfx_request_t *req, int index, uint32_t *out)
{
    uint32_t rgb888 = 0;
    if (!out || !lgfx_decode_u32_at(req, index, &rgb888) || !lgfx_validate_rgb888(rgb888)) {
        return false;
    }

    *out = rgb888;
    return true;
}

static inline bool lgfx_decode_palette_index_at(const lgfx_request_t *req, int index, uint8_t *out)
{
    return lgfx_decode_u8_at(req, index, out);
}

static inline bool lgfx_decode_binary_at(const lgfx_request_t *req, int index, const uint8_t **out_bytes, size_t *out_len)
{
    term t = lgfx_req_elem(req, index);
    if (!term_is_binary(t)) {
        return false;
    }

    size_t len = (size_t) term_binary_size(t);
    if (len > (size_t) LGFX_PORT_MAX_BINARY_BYTES) {
        return false;
    }

    if (out_bytes) {
        *out_bytes = (const uint8_t *) term_binary_data(t);
    }
    if (out_len) {
        *out_len = len;
    }

    return true;
}

static inline bool lgfx_decode_bool_term(const lgfx_port_t *port, term t, bool *out_value)
{
    if (!port || !out_value) {
        return false;
    }

    // Prefer atom true/false if available; otherwise accept 0/1.
    if (term_is_atom(t)) {
        if (t == port->atoms.true_) {
            *out_value = true;
            return true;
        }
        if (t == port->atoms.false_) {
            *out_value = false;
            return true;
        }
        return false;
    }

    uint32_t v = 0;
    if (!lgfx_term_to_u32(t, &v) || (v != 0 && v != 1)) {
        return false;
    }

    *out_value = (v == 1);
    return true;
}

static inline bool lgfx_req_has_flag(const lgfx_request_t *req, uint32_t flag)
{
    return req && ((req->flags & flag) != 0u);
}

static inline bool lgfx_decode_color_or_index_at(
    const lgfx_request_t *req,
    int index,
    uint32_t index_flag,
    bool *out_is_index,
    uint32_t *out_value)
{
    if (!req || !out_is_index || !out_value) {
        return false;
    }

    if (lgfx_req_has_flag(req, index_flag)) {
        uint8_t palette_index = 0;
        if (!lgfx_decode_palette_index_at(req, index, &palette_index)) {
            return false;
        }

        *out_is_index = true;
        *out_value = (uint32_t) palette_index;
        return true;
    }

    uint16_t rgb565 = 0;
    if (!lgfx_decode_color565_at(req, index, &rgb565)) {
        return false;
    }

    *out_is_index = false;
    *out_value = (uint32_t) rgb565;
    return true;
}
