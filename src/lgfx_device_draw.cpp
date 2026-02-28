// src/lgfx_device_draw.cpp
// Primitives + image transfer + LCD write-path helpers.

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <limits.h>
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

extern "C" esp_err_t lgfx_device_set_color_depth(uint8_t target, uint8_t depth)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setColorDepth(depth); });
}

extern "C" esp_err_t lgfx_device_set_color(uint8_t target, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setColor(rgb565); });
}

extern "C" esp_err_t lgfx_device_set_base_color(uint8_t target, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setBaseColor(rgb565); });
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

extern "C" esp_err_t lgfx_device_draw_round_rect(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawRoundRect(x, y, w, h, r, rgb565); });
}

extern "C" esp_err_t lgfx_device_fill_round_rect(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->fillRoundRect(x, y, w, h, r, rgb565); });
}

extern "C" esp_err_t lgfx_device_draw_circle(uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawCircle(x, y, r, rgb565); });
}

extern "C" esp_err_t lgfx_device_fill_circle(uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->fillCircle(x, y, r, rgb565); });
}

extern "C" esp_err_t lgfx_device_draw_ellipse(uint8_t target, int16_t x, int16_t y, uint16_t rx, uint16_t ry, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawEllipse(x, y, rx, ry, rgb565); });
}

extern "C" esp_err_t lgfx_device_fill_ellipse(uint8_t target, int16_t x, int16_t y, uint16_t rx, uint16_t ry, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->fillEllipse(x, y, rx, ry, rgb565); });
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

extern "C" esp_err_t lgfx_device_draw_bezier_q(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawBezier(x0, y0, x1, y1, x2, y2, rgb565); });
}

extern "C" esp_err_t lgfx_device_draw_bezier_c(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    int16_t x3,
    int16_t y3,
    uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawBezier(x0, y0, x1, y1, x2, y2, x3, y3, rgb565); });
}

extern "C" esp_err_t lgfx_device_draw_arc(uint8_t target, int16_t x, int16_t y, uint16_t r0, uint16_t r1, int16_t a0, int16_t a1, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawArc(x, y, r0, r1, a0, a1, rgb565); });
}

extern "C" esp_err_t lgfx_device_fill_arc(uint8_t target, int16_t x, int16_t y, uint16_t r0, uint16_t r1, int16_t a0, int16_t a1, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->fillArc(x, y, r0, r1, a0, a1, rgb565); });
}

extern "C" esp_err_t lgfx_device_draw_gradient_line(uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c0, uint16_t c1)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawGradientLine(x0, y0, x1, y1, c0, c1); });
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

// -----------------------------------------------------------------------------
// LCD write-path helpers (LCD-only by protocol semantics)
// -----------------------------------------------------------------------------

extern "C" esp_err_t lgfx_device_set_addr_window(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    if (w == 0 || h == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) {
        d->setAddrWindow((int32_t) x, (int32_t) y, (int32_t) w, (int32_t) h);
    });
}

extern "C" esp_err_t lgfx_device_push_pixels_rgb565(const uint8_t *pixels_be, size_t len_bytes)
{
    if (!pixels_be) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((len_bytes & 1u) != 0u) {
        return ESP_ERR_INVALID_SIZE;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    const size_t n_pixels = len_bytes / 2u;

    size_t i = 0;
    while (i < n_pixels) {
        const size_t chunk = ((n_pixels - i) > MAX_LINE_PIXELS) ? MAX_LINE_PIXELS : (n_pixels - i);
        rgb565_be_to_host_u16(pixels_be + (i * 2u), linebuf, chunk);
        lcd->pushPixels(linebuf, chunk);
        i += chunk;
    }

    return ESP_OK;
}

// Transaction helpers (LCD-only)

extern "C" esp_err_t lgfx_device_begin_transaction(void)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->beginTransaction(); });
}

extern "C" esp_err_t lgfx_device_end_transaction(void)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->endTransaction(); });
}

extern "C" esp_err_t lgfx_device_start_write(void)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->startWrite(); });
}

extern "C" esp_err_t lgfx_device_end_write(void)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->endWrite(); });
}

// write* ops (LCD-only)

extern "C" esp_err_t lgfx_device_write_pixel(int16_t x, int16_t y, uint16_t rgb565)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->writePixel(x, y, rgb565); });
}

extern "C" esp_err_t lgfx_device_write_fast_vline(int16_t x, int16_t y, uint16_t h, uint16_t rgb565)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->writeFastVLine(x, y, h, rgb565); });
}

extern "C" esp_err_t lgfx_device_write_fast_hline(int16_t x, int16_t y, uint16_t w, uint16_t rgb565)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->writeFastHLine(x, y, w, rgb565); });
}

extern "C" esp_err_t lgfx_device_write_fill_rect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t rgb565)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->writeFillRect(x, y, w, h, rgb565); });
}
