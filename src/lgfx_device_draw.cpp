// src/lgfx_device_draw.cpp
// Primitives + image transfer APIs that back the current protocol surface.

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <new>
#include <stddef.h>
#include <stdint.h>

// -----------------------------------------------------------------------------
// RGB565 helpers (local)
// -----------------------------------------------------------------------------

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

static constexpr size_t MAX_LINE_PIXELS = 480;
static uint16_t linebuf[MAX_LINE_PIXELS];

// -----------------------------------------------------------------------------
// Common ops (LCD or sprite target)
// -----------------------------------------------------------------------------

extern "C" esp_err_t lgfx_device_get_target_dims(uint8_t target, uint16_t *out_w, uint16_t *out_h)
{
    if (!out_w || !out_h) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t w_out = 0;
    uint16_t h_out = 0;
    bool dims_ok = true;

    esp_err_t err = lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
        const int32_t w = gfx->width();
        const int32_t h = gfx->height();

        if (w < 0 || h < 0 || w > 65535 || h > 65535) {
            // Should not happen, but keep the contract strict.
            dims_ok = false;
            return;
        }

        w_out = static_cast<uint16_t>(w);
        h_out = static_cast<uint16_t>(h);
    });

    if (err != ESP_OK) {
        return err;
    }

    if (!dims_ok) {
        return ESP_ERR_INVALID_STATE;
    }

    *out_w = w_out;
    *out_h = h_out;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_set_color_depth(uint8_t target, uint8_t depth)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setColorDepth(depth); });
}

// -----------------------------------------------------------------------------
// Basic drawing + primitives (LCD or sprite target)
// -----------------------------------------------------------------------------

extern "C" esp_err_t lgfx_device_fill_screen(uint8_t target, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->fillScreen(rgb565); });
}

extern "C" esp_err_t lgfx_device_clear(uint8_t target, uint16_t rgb565)
{
    return lgfx_device_fill_screen(target, rgb565);
}

extern "C" esp_err_t lgfx_device_draw_pixel(uint8_t target, int16_t x, int16_t y, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawPixel(x, y, rgb565); });
}

extern "C" esp_err_t lgfx_device_draw_fast_vline(uint8_t target, int16_t x, int16_t y, uint16_t h, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawFastVLine(x, y, h, rgb565); });
}

extern "C" esp_err_t lgfx_device_draw_fast_hline(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawFastHLine(x, y, w, rgb565); });
}

extern "C" esp_err_t lgfx_device_draw_line(uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawLine(x0, y0, x1, y1, rgb565); });
}

extern "C" esp_err_t lgfx_device_draw_rect(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawRect(x, y, w, h, rgb565); });
}

extern "C" esp_err_t lgfx_device_fill_rect(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->fillRect(x, y, w, h, rgb565); });
}

extern "C" esp_err_t lgfx_device_draw_circle(uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawCircle(x, y, r, rgb565); });
}

extern "C" esp_err_t lgfx_device_fill_circle(uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->fillCircle(x, y, r, rgb565); });
}

extern "C" esp_err_t lgfx_device_draw_triangle(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawTriangle(x0, y0, x1, y1, x2, y2, rgb565); });
}

extern "C" esp_err_t lgfx_device_fill_triangle(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->fillTriangle(x0, y0, x1, y1, x2, y2, rgb565); });
}

// -----------------------------------------------------------------------------
// Image transfer (LCD or sprite target)
// -----------------------------------------------------------------------------

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
