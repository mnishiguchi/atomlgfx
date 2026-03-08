// src/lgfx_device_primitives.cpp
// Drawing primitive APIs that back the current protocol surface.

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

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
