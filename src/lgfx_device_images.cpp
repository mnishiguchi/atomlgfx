// src/lgfx_device_images.cpp
// Image transfer APIs and RGB565 big-endian conversion helpers.

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <limits.h> // SIZE_MAX
#include <new>
#include <stddef.h>
#include <stdint.h>

// ----------------------------------------------------------------------------
// RGB565 helpers (local to images file)
// ----------------------------------------------------------------------------

// Convert RGB565 big-endian bytes to host u16
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

// Keep this sized to at least max panel dimension for chunked pushPixels and common pushImage rows.
// Current piyopiyo-pcb v1.5 panel is 320x480, so 480 is sufficient.
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

    // Backward-compatible contract: stride==0 means "tightly packed" (stride=w).
    if (stride_pixels == 0) {
        stride_pixels = w;
    }
    if (stride_pixels < w) {
        return ESP_ERR_INVALID_ARG;
    }

    // RGB565 must be an even number of bytes.
    if ((pixels_len & 1u) != 0u) {
        return ESP_ERR_INVALID_SIZE;
    }

    // Validate that the provided buffer is large enough for stride*h pixels.
    // Use 64-bit math to avoid overflow on 32-bit size_t targets.
    const uint64_t needed64 = (uint64_t) stride_pixels * 2u * (uint64_t) h;
    if (needed64 > (uint64_t) SIZE_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }
    const size_t needed = (size_t) needed64;

    // Allow extra trailing bytes, but reject too-small buffers.
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

    // Fast path: use static linebuf if possible, otherwise allocate a one-row buffer.
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

        // Convert only the visible width (ignore stride padding pixels).
        rgb565_be_to_host_u16(src, rowbuf, (size_t) w);

        // Draw one scanline. This avoids allocating w*h buffers.
        gfx->pushImage(x, (int16_t) (y + row), w, 1, rowbuf);
    }

    if (!use_static) {
        delete[] rowbuf;
    }

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_set_addr_window(int16_t x, int16_t y, uint16_t w, uint16_t h)
{
    if (w == 0 || h == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Port semantics enforce "in_write" in the validator/handler layer.
    // LovyanGFX setAddrWindow returns void; we just forward it under the LCD lock.
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

    // pushPixels is LCD-only in the handler, so use the LCD singleton directly.
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
