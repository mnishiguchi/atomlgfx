// src/lgfx_device_control.cpp
// LCD-only control + size/touch APIs that back the current protocol surface.

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <algorithm>
#include <string.h>
#include <utility>

// -----------------------------------------------------------------------------
// LCD-only control APIs (rotation, brightness, display).
// -----------------------------------------------------------------------------

extern "C" esp_err_t lgfx_device_set_rotation(uint8_t rotation)
{
    if (rotation > 7) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->setRotation(rotation); });
}

extern "C" esp_err_t lgfx_device_set_brightness(uint8_t brightness)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->setBrightness(brightness); });
}

extern "C" esp_err_t lgfx_device_display(void)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->display(); });
}

// -----------------------------------------------------------------------------
// Size queries and common target config.
// -----------------------------------------------------------------------------

extern "C" esp_err_t lgfx_device_get_dims(uint16_t *out_w, uint16_t *out_h)
{
    if (!out_w || !out_h) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lgfx_dev::lock_lcd()) {
        *out_w = lgfx_dev::panel_width_const();
        *out_h = lgfx_dev::panel_height_const();
        return ESP_OK;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd || !lgfx_dev::is_initialized_locked()) {
        lgfx_dev::unlock_lcd();
        *out_w = lgfx_dev::panel_width_const();
        *out_h = lgfx_dev::panel_height_const();
        return ESP_OK;
    }

    uint16_t w = static_cast<uint16_t>(lcd->width());
    uint16_t h = static_cast<uint16_t>(lcd->height());

    lgfx_dev::unlock_lcd();

    if (w == 0) {
        w = lgfx_dev::panel_width_const();
    }
    if (h == 0) {
        h = lgfx_dev::panel_height_const();
    }

    *out_w = w;
    *out_h = h;
    return ESP_OK;
}

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
// Touch (LCD-only by protocol semantics)
// -----------------------------------------------------------------------------

extern "C" esp_err_t lgfx_device_get_touch(
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size)
{
    if (!out_touched || !out_x || !out_y || !out_size) {
        return ESP_ERR_INVALID_ARG;
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

    if (!lcd->touch()) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    lgfx::touch_point_t tp{};
    const uint32_t n = lcd->getTouch(&tp, 1);

    if (n == 0) {
        *out_touched = false;
        *out_x = 0;
        *out_y = 0;
        *out_size = 0;
        return ESP_OK;
    }

    *out_touched = true;
    *out_x = (int16_t) tp.x;
    *out_y = (int16_t) tp.y;
    *out_size = (uint16_t) tp.size;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_get_touch_raw(
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size)
{
    if (!out_touched || !out_x || !out_y || !out_size) {
        return ESP_ERR_INVALID_ARG;
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

    if (!lcd->touch()) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    lgfx::touch_point_t tp{};
    const uint32_t n = lcd->getTouchRaw(&tp, 1);

    if (n == 0) {
        *out_touched = false;
        *out_x = 0;
        *out_y = 0;
        *out_size = 0;
        return ESP_OK;
    }

    *out_touched = true;
    *out_x = (int16_t) tp.x;
    *out_y = (int16_t) tp.y;
    *out_size = (uint16_t) tp.size;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_set_touch_calibrate(const uint16_t params[8])
{
    if (!params) {
        return ESP_ERR_INVALID_ARG;
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

    if (!lcd->touch()) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint16_t mutable_params[8];
    for (int i = 0; i < 8; i++) {
        mutable_params[i] = params[i];
    }

    lcd->setTouchCalibrate(mutable_params);
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_calibrate_touch(uint16_t out_params[8])
{
    if (!out_params) {
        return ESP_ERR_INVALID_ARG;
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

    if (!lcd->touch()) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint16_t fg = 0xFFFF;
    uint16_t bg = 0x0000;
    if (lcd->isEPD()) {
        std::swap(fg, bg);
    }

    const uint16_t w = (uint16_t) lcd->width();
    const uint16_t h = (uint16_t) lcd->height();
    const uint16_t m = (uint16_t) std::max(w, h);

    uint8_t marker_size = (uint8_t) (m >> 3);
    if (marker_size == 0) {
        marker_size = 1;
    }

    lcd->calibrateTouch(out_params, fg, bg, marker_size);
    return ESP_OK;
}
