// SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
//
// SPDX-License-Identifier: Apache-2.0

// lgfx_device/sprites.cpp

#include "lgfx_device/lgfx_device.h"
#include "lgfx_device/lgfx_device_internal.hpp"

#include <cmath>
#include <new>

namespace
{

static inline bool lgfx_rotate_zoom_args_are_valid(float angle, float zoom_x, float zoom_y)
{
    return std::isfinite(angle)
        && std::isfinite(zoom_x)
        && std::isfinite(zoom_y)
        && zoom_x > 0.0f
        && zoom_y > 0.0f;
}

static esp_err_t lgfx_push_sprite_locked_impl(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value)
{
    if (!lgfx_device_is_sprite_target(src_handle)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lgfx_dev::protocol_valid_target(dst_target)) {
        return ESP_ERR_INVALID_ARG;
    }

    auto *src = lgfx_dev::resolve_sprite_locked(src_handle);
    if (!src) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = lgfx_dev::validate_sprite_transparent_scalar(
        src,
        has_transparent,
        transparent_is_index,
        transparent_value);
    if (err != ESP_OK) {
        return err;
    }

    if (lgfx_device_is_lcd_target(dst_target)) {
        auto *render_surface = lgfx_dev::resolve_render_surface_locked(dst_target);
        if (!render_surface) {
            return ESP_ERR_INVALID_STATE;
        }

        if (has_transparent) {
            src->pushSprite(render_surface, x, y, static_cast<uint32_t>(transparent_value));
        } else {
            src->pushSprite(render_surface, x, y);
        }

        return ESP_OK;
    }

    auto *dst_spr = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst_spr) {
        return ESP_ERR_NOT_FOUND;
    }

    if (has_transparent) {
        src->pushSprite(dst_spr, x, y, static_cast<uint32_t>(transparent_value));
    } else {
        src->pushSprite(dst_spr, x, y);
    }

    return ESP_OK;
}

static esp_err_t lgfx_push_rotate_zoom_locked_impl(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    float angle,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value)
{
    if (!lgfx_device_is_sprite_target(src_handle)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lgfx_dev::protocol_valid_target(dst_target)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lgfx_rotate_zoom_args_are_valid(angle, zoom_x, zoom_y)) {
        return ESP_ERR_INVALID_ARG;
    }

    auto *src = lgfx_dev::resolve_sprite_locked(src_handle);
    if (!src) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = lgfx_dev::validate_sprite_transparent_scalar(
        src,
        has_transparent,
        transparent_is_index,
        transparent_value);
    if (err != ESP_OK) {
        return err;
    }

    if (lgfx_device_is_lcd_target(dst_target)) {
        auto *render_surface = lgfx_dev::resolve_render_surface_locked(dst_target);
        if (!render_surface) {
            return ESP_ERR_INVALID_STATE;
        }

        if (has_transparent) {
            src->pushRotateZoom(
                render_surface,
                static_cast<float>(x),
                static_cast<float>(y),
                angle,
                zoom_x,
                zoom_y,
                static_cast<uint32_t>(transparent_value));
        } else {
            src->pushRotateZoom(
                render_surface,
                static_cast<float>(x),
                static_cast<float>(y),
                angle,
                zoom_x,
                zoom_y);
        }

        return ESP_OK;
    }

    auto *dst_spr = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst_spr) {
        return ESP_ERR_NOT_FOUND;
    }

    if (has_transparent) {
        src->pushRotateZoom(
            dst_spr,
            static_cast<float>(x),
            static_cast<float>(y),
            angle,
            zoom_x,
            zoom_y,
            static_cast<uint32_t>(transparent_value));
    } else {
        src->pushRotateZoom(
            dst_spr,
            static_cast<float>(x),
            static_cast<float>(y),
            angle,
            zoom_x,
            zoom_y);
    }

    return ESP_OK;
}

} // namespace

// -----------------------------------------------------------------------------
// Internal locked hot-path helpers (batch path)
// -----------------------------------------------------------------------------

esp_err_t lgfx_dev::push_sprite_locked(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t dst_x,
    int16_t dst_y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value)
{
    return lgfx_push_sprite_locked_impl(
        src_handle,
        dst_target,
        dst_x,
        dst_y,
        has_transparent,
        transparent_is_index,
        transparent_value);
}

esp_err_t lgfx_dev::push_rotate_zoom_locked(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t dst_x,
    int16_t dst_y,
    float angle,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value)
{
    return lgfx_push_rotate_zoom_locked_impl(
        src_handle,
        dst_target,
        dst_x,
        dst_y,
        angle,
        zoom_x,
        zoom_y,
        has_transparent,
        transparent_is_index,
        transparent_value);
}

// -----------------------------------------------------------------------------
// Public C ABI (ordinary sync path)
// -----------------------------------------------------------------------------

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

    // Enforce advertised sprite capacity even with caller-selected handles.
    if (lgfx_dev::sprite_count_locked() >= static_cast<uint32_t>(lgfx_dev::max_sprites_const())) {
        return ESP_ERR_NO_MEM;
    }

    // Caller-selected handle; reject occupied slots.
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

    // Release internal sprite buffers before deleting the object.
    spr->deleteSprite();
    delete spr;

    lgfx_dev::clear_sprite_locked(handle);
    lgfx_dev::decrement_sprite_count_locked();

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_sprite_create_palette(uint8_t handle)
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

    if (!lgfx_dev::sprite_supports_palette_storage(spr)) {
        return ESP_ERR_INVALID_ARG;
    }

    spr->createPalette();

    if (!lgfx_dev::sprite_uses_palette_indices(spr)) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_sprite_set_palette_color(uint8_t handle, uint8_t palette_index, uint32_t rgb888)
{
    if (!lgfx_device_is_sprite_target(handle) || !lgfx_dev::scalar_rgb888_is_valid(rgb888)) {
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

    if (!lgfx_dev::sprite_uses_palette_indices(spr)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (palette_index > lgfx_dev::sprite_palette_index_max(spr)) {
        return ESP_ERR_INVALID_ARG;
    }

    spr->setPaletteColor(palette_index, rgb888);
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_set_pivot(uint8_t target, int16_t px, int16_t py)
{
    return lgfx_dev::with_render_target(target, [&](lgfx::LGFXBase *gfx) {
        gfx->setPivot(px, py);
    });
}

extern "C" esp_err_t lgfx_device_sprite_push_sprite(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value)
{
    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    return lgfx_dev::push_sprite_locked(
        src_handle,
        dst_target,
        x,
        y,
        has_transparent,
        transparent_is_index,
        transparent_value);
}

extern "C" esp_err_t lgfx_device_sprite_push_rotate_zoom(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    float angle,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value)
{
    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    return lgfx_dev::push_rotate_zoom_locked(
        src_handle,
        dst_target,
        x,
        y,
        angle,
        zoom_x,
        zoom_y,
        has_transparent,
        transparent_is_index,
        transparent_value);
}
