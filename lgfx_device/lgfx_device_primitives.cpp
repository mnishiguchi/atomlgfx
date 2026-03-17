// src/lgfx_device_primitives.cpp

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

namespace
{

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

        draw_fn(gfx, color_value);
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
