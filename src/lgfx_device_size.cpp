// src/lgfx_device_size.cpp
// Size query APIs (LCD or sprite target) with pre-init LCD fallback behavior.

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

extern "C" uint16_t lgfx_device_width(uint8_t target)
{
    if (!lgfx_dev::lock_lcd()) {
        return (target == 0) ? lgfx_dev::panel_width_const() : 0;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (target == 0 && (!lcd || !lgfx_dev::is_initialized_locked())) {
        lgfx_dev::unlock_lcd();
        return lgfx_dev::panel_width_const();
    }

    lgfx::LGFXBase *gfx = lgfx_dev::resolve_target_locked(target);
    uint16_t w = gfx ? static_cast<uint16_t>(gfx->width()) : 0;

    lgfx_dev::unlock_lcd();

    return w ? w : (target == 0 ? lgfx_dev::panel_width_const() : 0);
}

extern "C" uint16_t lgfx_device_height(uint8_t target)
{
    if (!lgfx_dev::lock_lcd()) {
        return (target == 0) ? lgfx_dev::panel_height_const() : 0;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (target == 0 && (!lcd || !lgfx_dev::is_initialized_locked())) {
        lgfx_dev::unlock_lcd();
        return lgfx_dev::panel_height_const();
    }

    lgfx::LGFXBase *gfx = lgfx_dev::resolve_target_locked(target);
    uint16_t h = gfx ? static_cast<uint16_t>(gfx->height()) : 0;

    lgfx_dev::unlock_lcd();

    return h ? h : (target == 0 ? lgfx_dev::panel_height_const() : 0);
}

extern "C" esp_err_t lgfx_device_get_dims(uint16_t *out_w, uint16_t *out_h)
{
    if (!out_w || !out_h) {
        return ESP_ERR_INVALID_ARG;
    }

    // Match width/height behavior: allow querying before init or if lock creation fails.
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

    // Defensive fallback: if LovyanGFX returns 0 for some reason, use panel constants.
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
