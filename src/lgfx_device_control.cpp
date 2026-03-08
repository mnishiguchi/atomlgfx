// src/lgfx_device_control.cpp
// LCD-only control + size/touch APIs that back the current protocol surface.

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <algorithm>
#include <string.h>
#include <utility>

namespace
{
static inline void lgfx_assign_panel_default_dims(uint16_t *out_w, uint16_t *out_h)
{
    *out_w = lgfx_dev::panel_width_const();
    *out_h = lgfx_dev::panel_height_const();
}

static inline uint16_t lgfx_dim_or_fallback(int32_t value, uint16_t fallback)
{
    if (value <= 0 || value > 65535) {
        return fallback;
    }

    return static_cast<uint16_t>(value);
}

static inline bool lgfx_try_u16_dim(int32_t value, uint16_t *out_value)
{
    if (!out_value || value < 0 || value > 65535) {
        return false;
    }

    *out_value = static_cast<uint16_t>(value);
    return true;
}

template <typename Fn>
static esp_err_t lgfx_with_touch_lcd(Fn &&fn)
{
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

    return fn(lcd);
}

static inline void lgfx_set_touch_none(
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size)
{
    *out_touched = false;
    *out_x = 0;
    *out_y = 0;
    *out_size = 0;
}

static inline void lgfx_set_touch_point(
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size,
    const lgfx::touch_point_t &tp)
{
    *out_touched = true;
    *out_x = static_cast<int16_t>(tp.x);
    *out_y = static_cast<int16_t>(tp.y);
    *out_size = static_cast<uint16_t>(tp.size);
}

template <typename QueryFn>
static esp_err_t lgfx_get_touch_common(
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size,
    QueryFn &&query_fn)
{
    if (!out_touched || !out_x || !out_y || !out_size) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_with_touch_lcd([&](lgfx::LGFX_Device *lcd) -> esp_err_t {
        lgfx::touch_point_t tp{};
        const uint32_t count = query_fn(lcd, &tp);

        if (count == 0) {
            lgfx_set_touch_none(out_touched, out_x, out_y, out_size);
            return ESP_OK;
        }

        lgfx_set_touch_point(out_touched, out_x, out_y, out_size, tp);
        return ESP_OK;
    });
}

static inline uint8_t lgfx_touch_calibration_marker_size(lgfx::LGFX_Device *lcd)
{
    const uint16_t w = static_cast<uint16_t>(lcd->width());
    const uint16_t h = static_cast<uint16_t>(lcd->height());
    const uint16_t m = static_cast<uint16_t>(std::max(w, h));

    uint8_t marker_size = static_cast<uint8_t>(m >> 3);
    if (marker_size == 0) {
        marker_size = 1;
    }

    return marker_size;
}
} // namespace

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
        lgfx_assign_panel_default_dims(out_w, out_h);
        return ESP_OK;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd || !lgfx_dev::is_initialized_locked()) {
        lgfx_dev::unlock_lcd();
        lgfx_assign_panel_default_dims(out_w, out_h);
        return ESP_OK;
    }

    const uint16_t w = lgfx_dim_or_fallback(lcd->width(), lgfx_dev::panel_width_const());
    const uint16_t h = lgfx_dim_or_fallback(lcd->height(), lgfx_dev::panel_height_const());

    lgfx_dev::unlock_lcd();

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
        if (!lgfx_try_u16_dim(gfx->width(), &w_out) || !lgfx_try_u16_dim(gfx->height(), &h_out)) {
            dims_ok = false;
        }
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
    return lgfx_get_touch_common(
        out_touched,
        out_x,
        out_y,
        out_size,
        [](lgfx::LGFX_Device *lcd, lgfx::touch_point_t *tp) { return lcd->getTouch(tp, 1); });
}

extern "C" esp_err_t lgfx_device_get_touch_raw(
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size)
{
    return lgfx_get_touch_common(
        out_touched,
        out_x,
        out_y,
        out_size,
        [](lgfx::LGFX_Device *lcd, lgfx::touch_point_t *tp) { return lcd->getTouchRaw(tp, 1); });
}

extern "C" esp_err_t lgfx_device_set_touch_calibrate(const uint16_t params[8])
{
    if (!params) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_with_touch_lcd([&](lgfx::LGFX_Device *lcd) -> esp_err_t {
        uint16_t mutable_params[8];
        memcpy(mutable_params, params, sizeof(mutable_params));
        lcd->setTouchCalibrate(mutable_params);
        return ESP_OK;
    });
}

extern "C" esp_err_t lgfx_device_calibrate_touch(uint16_t out_params[8])
{
    if (!out_params) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_with_touch_lcd([&](lgfx::LGFX_Device *lcd) -> esp_err_t {
        uint16_t fg = 0xFFFF;
        uint16_t bg = 0x0000;
        if (lcd->isEPD()) {
            std::swap(fg, bg);
        }

        lcd->calibrateTouch(out_params, fg, bg, lgfx_touch_calibration_marker_size(lcd));
        return ESP_OK;
    });
}
