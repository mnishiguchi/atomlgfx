// src/lgfx_device_sprites.cpp
// Sprite subsystem APIs for the pinned LovyanGFX surface used by the current protocol.

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <cmath>
#include <new>

namespace
{

// Supported LovyanGFX surface for this component:
//
// - createSprite(w, h)
// - pushSprite(dst, x, y [, transparent565])
// - pushRotateZoom(dst, x, y, angle_deg, zoom_x, zoom_y [, transparent565])
//
// If the pinned LovyanGFX submodule changes these signatures, prefer an
// explicit update here over reintroducing compile-time fallback probes.

static bool protocol_valid_target(uint8_t target)
{
    return lgfx_device_is_lcd_target(target) || lgfx_device_is_sprite_target(target);
}

} // namespace

extern "C" esp_err_t lgfx_device_sprite_create_at(uint8_t handle, uint16_t w, uint16_t h, uint8_t color_depth)
{
    if (!lgfx_device_is_sprite_target(handle) || w == 0 || h == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    // Enforce the advertised protocol/runtime sprite capacity even when caller
    // chooses a specific handle directly.
    if (lgfx_dev::sprite_count_locked() >= static_cast<uint32_t>(lgfx_dev::max_sprites_const())) {
        return ESP_ERR_NO_MEM;
    }

    // create_at is deterministic: caller chooses the sprite handle.
    // Reject if the slot is already in use.
    if (lgfx_dev::resolve_sprite_locked(handle) != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    auto *spr = new (std::nothrow) lgfx::LGFX_Sprite(lcd);
    if (!spr) {
        return ESP_ERR_NO_MEM;
    }

    if (color_depth != 0) {
        spr->setColorDepth(color_depth);
    }

    spr->createSprite(w, h);
    if (spr->getBuffer() == nullptr) {
        delete spr;
        return ESP_ERR_NO_MEM;
    }

    lgfx_dev::set_sprite_locked(handle, spr);
    lgfx_dev::increment_sprite_count_locked();

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_sprite_delete(uint8_t handle)
{
    if (!lgfx_device_is_sprite_target(handle)) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *spr = lgfx_dev::resolve_sprite_locked(handle);
    if (!spr) {
        return ESP_ERR_NOT_FOUND;
    }

    // Be explicit about releasing internal buffers.
    spr->deleteSprite();
    delete spr;

    lgfx_dev::clear_sprite_locked(handle);
    lgfx_dev::decrement_sprite_count_locked();

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_sprite_set_pivot(uint8_t handle, int16_t px, int16_t py)
{
    return lgfx_dev::with_sprite(handle, [&](lgfx::LGFX_Sprite *spr) { spr->setPivot(px, py); });
}

extern "C" esp_err_t lgfx_device_sprite_push_sprite(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint16_t transparent_rgb565)
{
    if (!lgfx_device_is_sprite_target(src_handle)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!protocol_valid_target(dst_target)) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *src = lgfx_dev::resolve_sprite_locked(src_handle);
    if (!src) {
        return ESP_ERR_NOT_FOUND;
    }

    if (lgfx_device_is_lcd_target(dst_target)) {
        auto *lcd = lgfx_dev::lcd_device_locked();
        if (!lcd) {
            return ESP_ERR_INVALID_STATE;
        }

        if (has_transparent) {
            src->pushSprite(lcd, x, y, transparent_rgb565);
        } else {
            src->pushSprite(lcd, x, y);
        }

        return ESP_OK;
    }

    auto *dst_spr = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst_spr) {
        return ESP_ERR_NOT_FOUND;
    }

    if (has_transparent) {
        src->pushSprite(dst_spr, x, y, transparent_rgb565);
    } else {
        src->pushSprite(dst_spr, x, y);
    }

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_sprite_push_rotate_zoom(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    float angle_deg,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    uint16_t transparent565)
{
    if (!lgfx_device_is_sprite_target(src_handle)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!protocol_valid_target(dst_target)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!std::isfinite(angle_deg) || !std::isfinite(zoom_x) || !std::isfinite(zoom_y)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!(zoom_x > 0.0f) || !(zoom_y > 0.0f)) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *src = lgfx_dev::resolve_sprite_locked(src_handle);
    if (!src) {
        return ESP_ERR_NOT_FOUND;
    }

    if (lgfx_device_is_lcd_target(dst_target)) {
        auto *lcd = lgfx_dev::lcd_device_locked();
        if (!lcd) {
            return ESP_ERR_INVALID_STATE;
        }

        if (has_transparent) {
            src->pushRotateZoom(lcd, (float) x, (float) y, angle_deg, zoom_x, zoom_y, transparent565);
        } else {
            src->pushRotateZoom(lcd, (float) x, (float) y, angle_deg, zoom_x, zoom_y);
        }

        return ESP_OK;
    }

    auto *dst_spr = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst_spr) {
        return ESP_ERR_NOT_FOUND;
    }

    if (has_transparent) {
        src->pushRotateZoom(dst_spr, (float) x, (float) y, angle_deg, zoom_x, zoom_y, transparent565);
    } else {
        src->pushRotateZoom(dst_spr, (float) x, (float) y, angle_deg, zoom_x, zoom_y);
    }

    return ESP_OK;
}
