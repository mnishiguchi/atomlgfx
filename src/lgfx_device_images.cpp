// src/lgfx_device_images.cpp

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <new>
#include <stddef.h>
#include <stdint.h>

static inline uint16_t be16_at(const uint8_t *p)
{
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}

static inline void rgb565_be_to_host_u16(const uint8_t *src_be, uint16_t *dst, size_t n_pixels)
{
    for (size_t i = 0; i < n_pixels; i++) {
        dst[i] = be16_at(src_be + (i * 2u));
    }
}

// Reuse a static row buffer for common widths; fall back to heap allocation for wider rows.
static constexpr size_t MAX_LINE_PIXELS = 480;
static uint16_t linebuf[MAX_LINE_PIXELS];

extern "C" esp_err_t lgfx_device_push_image_rgb565_strided(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t stride_pixels,
    const uint8_t *pixels_be,
    size_t pixels_len)
{
    if (!pixels_be || w == 0 || h == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (stride_pixels == 0) {
        stride_pixels = w;
    }
    if (stride_pixels < w) {
        return ESP_ERR_INVALID_ARG;
    }

    if ((pixels_len & 1u) != 0u) {
        return ESP_ERR_INVALID_SIZE;
    }

    const uint64_t needed64 = (uint64_t) stride_pixels * 2u * (uint64_t) h;
    if (needed64 > (uint64_t) SIZE_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }
    const size_t needed = (size_t) needed64;

    if (pixels_len < needed) {
        return ESP_ERR_INVALID_SIZE;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    lgfx::LGFXBase *gfx = lgfx_dev::resolve_target_locked(target);
    if (!gfx) {
        return ESP_ERR_NOT_FOUND;
    }

    // Input rows are RGB565 big-endian with optional stride padding.
    const size_t row_bytes = (size_t) stride_pixels * 2u;

    uint16_t *rowbuf = nullptr;
    const bool use_static = (w <= MAX_LINE_PIXELS);

    if (use_static) {
        rowbuf = linebuf;
    } else {
        rowbuf = new (std::nothrow) uint16_t[w];
        if (!rowbuf) {
            return ESP_ERR_NO_MEM;
        }
    }

    for (uint16_t row = 0; row < h; row++) {
        const uint8_t *src = pixels_be + ((size_t) row * row_bytes);
        rgb565_be_to_host_u16(src, rowbuf, (size_t) w);
        gfx->pushImage(x, (int16_t) (y + row), w, 1, rowbuf);
    }

    if (!use_static) {
        delete[] rowbuf;
    }

    return ESP_OK;
}
