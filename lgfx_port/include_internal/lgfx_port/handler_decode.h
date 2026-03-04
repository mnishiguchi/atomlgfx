// lgfx_port/include_internal/lgfx_port/handler_decode.h
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "term.h"

#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/proto_term.h"
#include "lgfx_port/protocol.h"

// ----------------------------------------------------------------------------
// Tiny decode helpers for handlers (low-risk LOC win)
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
