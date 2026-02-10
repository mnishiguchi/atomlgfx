// ports/include/lgfx_port/term_conv.h
#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "term.h"

#ifdef __cplusplus
extern "C" {
#endif

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

static inline bool lgfx_validate_i16(int32_t v)
{
    return v >= -32768 && v <= 32767;
}

static inline bool lgfx_validate_u16(uint32_t v)
{
    return v <= 65535u;
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

static inline bool lgfx_validate_color888(uint32_t c)
{
    // 0x00RRGGBB only
    return (c & 0xFF000000u) == 0;
}

#ifdef __cplusplus
}
#endif
