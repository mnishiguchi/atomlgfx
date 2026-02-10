// src/lgfx_device_primitives.cpp
// Basic drawing and primitives APIs (LCD or sprite target).

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

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

extern "C" esp_err_t lgfx_device_fill_screen(uint8_t target, uint16_t rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->fillScreen(rgb565); });
}

extern "C" esp_err_t lgfx_device_clear(uint8_t target, uint16_t rgb565)
{
    // same as fillScreen for predictability
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
