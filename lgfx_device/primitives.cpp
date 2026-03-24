// SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
//
// SPDX-License-Identifier: Apache-2.0

// lgfx_device/primitives.cpp

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <cmath>

namespace
{

static inline bool lgfx_arc_angle_is_valid(float angle)
{
    return std::isfinite(angle);
}

static inline uint32_t lgfx_rgb565_to_rgb888(uint16_t color565)
{
    const uint8_t r5 = static_cast<uint8_t>((color565 >> 11) & 0x1Fu);
    const uint8_t g6 = static_cast<uint8_t>((color565 >> 5) & 0x3Fu);
    const uint8_t b5 = static_cast<uint8_t>(color565 & 0x1Fu);

    const uint8_t r8 = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
    const uint8_t g8 = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
    const uint8_t b8 = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));

    return (static_cast<uint32_t>(r8) << 16)
        | (static_cast<uint32_t>(g8) << 8)
        | static_cast<uint32_t>(b8);
}

static inline uint32_t lgfx_resolve_draw_scalar_color(bool color_is_index, uint32_t color_value)
{
    if (color_is_index) {
        return color_value;
    }

    return lgfx_rgb565_to_rgb888(static_cast<uint16_t>(color_value & 0xFFFFu));
}

template <typename DrawFn>
static esp_err_t lgfx_with_validated_target_color(
    uint8_t target,
    bool color_is_index,
    uint32_t color_value,
    DrawFn &&draw_fn)
{
    esp_err_t validation_err = ESP_OK;

    esp_err_t err = lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
        validation_err = lgfx_dev::validate_target_scalar_color(target, gfx, color_is_index, color_value);
        if (validation_err != ESP_OK) {
            return;
        }

        const uint32_t resolved_color = lgfx_resolve_draw_scalar_color(color_is_index, color_value);
        draw_fn(gfx, resolved_color);
    });

    if (err != ESP_OK) {
        return err;
    }

    return validation_err;
}

} // namespace

extern "C" esp_err_t lgfx_device_fill_screen(uint8_t target, bool color_is_index, uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) { gfx->fillScreen(scalar_color); });
}

extern "C" esp_err_t lgfx_device_clear(uint8_t target, bool color_is_index, uint32_t color_value)
{
    return lgfx_device_fill_screen(target, color_is_index, color_value);
}

extern "C" esp_err_t lgfx_device_draw_pixel(
    uint8_t target,
    int16_t x,
    int16_t y,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) { gfx->drawPixel(x, y, scalar_color); });
}

extern "C" esp_err_t lgfx_device_draw_fast_vline(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t h,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) { gfx->drawFastVLine(x, y, h, scalar_color); });
}

extern "C" esp_err_t lgfx_device_draw_fast_hline(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) { gfx->drawFastHLine(x, y, w, scalar_color); });
}

extern "C" esp_err_t lgfx_device_draw_line(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) { gfx->drawLine(x0, y0, x1, y1, scalar_color); });
}

extern "C" esp_err_t lgfx_device_draw_rect(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) { gfx->drawRect(x, y, w, h, scalar_color); });
}

extern "C" esp_err_t lgfx_device_fill_rect(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) { gfx->fillRect(x, y, w, h, scalar_color); });
}

extern "C" esp_err_t lgfx_device_draw_round_rect(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t r,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) {
            gfx->drawRoundRect(x, y, w, h, r, scalar_color);
        });
}

extern "C" esp_err_t lgfx_device_fill_round_rect(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t r,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) {
            gfx->fillRoundRect(x, y, w, h, r, scalar_color);
        });
}

extern "C" esp_err_t lgfx_device_draw_circle(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t r,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) { gfx->drawCircle(x, y, r, scalar_color); });
}

extern "C" esp_err_t lgfx_device_fill_circle(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t r,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) { gfx->fillCircle(x, y, r, scalar_color); });
}

extern "C" esp_err_t lgfx_device_draw_ellipse(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t rx,
    uint16_t ry,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) {
            gfx->drawEllipse(x, y, rx, ry, scalar_color);
        });
}

extern "C" esp_err_t lgfx_device_fill_ellipse(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t rx,
    uint16_t ry,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) {
            gfx->fillEllipse(x, y, rx, ry, scalar_color);
        });
}

extern "C" esp_err_t lgfx_device_draw_arc(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t r0,
    uint16_t r1,
    float angle0,
    float angle1,
    bool color_is_index,
    uint32_t color_value)
{
    if (!lgfx_arc_angle_is_valid(angle0) || !lgfx_arc_angle_is_valid(angle1)) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) {
            gfx->drawArc(x, y, r0, r1, angle0, angle1, scalar_color);
        });
}

extern "C" esp_err_t lgfx_device_fill_arc(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t r0,
    uint16_t r1,
    float angle0,
    float angle1,
    bool color_is_index,
    uint32_t color_value)
{
    if (!lgfx_arc_angle_is_valid(angle0) || !lgfx_arc_angle_is_valid(angle1)) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) {
            gfx->fillArc(x, y, r0, r1, angle0, angle1, scalar_color);
        });
}

extern "C" esp_err_t lgfx_device_draw_bezier3(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) {
            gfx->drawBezier(x0, y0, x1, y1, x2, y2, scalar_color);
        });
}

extern "C" esp_err_t lgfx_device_draw_bezier4(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    int16_t x3,
    int16_t y3,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) {
            gfx->drawBezier(x0, y0, x1, y1, x2, y2, x3, y3, scalar_color);
        });
}

extern "C" esp_err_t lgfx_device_draw_triangle(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) {
            gfx->drawTriangle(x0, y0, x1, y1, x2, y2, scalar_color);
        });
}

extern "C" esp_err_t lgfx_device_fill_triangle(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    bool color_is_index,
    uint32_t color_value)
{
    return lgfx_with_validated_target_color(
        target,
        color_is_index,
        color_value,
        [&](lgfx::LGFXBase *gfx, uint32_t scalar_color) {
            gfx->fillTriangle(x0, y0, x1, y1, x2, y2, scalar_color);
        });
}
