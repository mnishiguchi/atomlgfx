// src/lgfx_device_touch.cpp

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <algorithm>
#include <utility>

#include <LovyanGFX.hpp>

#include "esp_err.h"

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

    // LovyanGFX API requires mutable uint16_t*.
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

    uint16_t fg = 0xFFFF; // white
    uint16_t bg = 0x0000; // black
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
