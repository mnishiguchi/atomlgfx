// src/lgfx_device_sprites.cpp
// Sprite subsystem APIs and sprite region push compatibility helpers.

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <new>

// -----------------------------------------------------------------------------
// Sprite region push helper (best-effort across LovyanGFX/M5GFX variants)
// -----------------------------------------------------------------------------

template <typename S>
static auto sprite_push_region_plain_impl(
    S *spr,
    int16_t dst_x,
    int16_t dst_y,
    int16_t src_x,
    int16_t src_y,
    uint16_t w,
    uint16_t h,
    int) -> decltype(spr->pushSprite(dst_x, dst_y, src_x, src_y, w, h), bool())
{
    spr->pushSprite(dst_x, dst_y, src_x, src_y, w, h);
    return true;
}

template <typename S>
static bool sprite_push_region_plain_impl(
    S * /*spr*/,
    int16_t /*dst_x*/,
    int16_t /*dst_y*/,
    int16_t /*src_x*/,
    int16_t /*src_y*/,
    uint16_t /*w*/,
    uint16_t /*h*/,
    long)
{
    return false;
}

template <typename S>
static auto sprite_push_region_trans_impl(
    S *spr,
    int16_t dst_x,
    int16_t dst_y,
    int16_t src_x,
    int16_t src_y,
    uint16_t w,
    uint16_t h,
    uint16_t transparent565,
    int) -> decltype(spr->pushSprite(dst_x, dst_y, src_x, src_y, w, h, transparent565), bool())
{
    spr->pushSprite(dst_x, dst_y, src_x, src_y, w, h, transparent565);
    return true;
}

template <typename S>
static bool sprite_push_region_trans_impl(
    S * /*spr*/,
    int16_t /*dst_x*/,
    int16_t /*dst_y*/,
    int16_t /*src_x*/,
    int16_t /*src_y*/,
    uint16_t /*w*/,
    uint16_t /*h*/,
    uint16_t /*transparent565*/,
    long)
{
    return false;
}

static bool sprite_push_region_best_effort(
    lgfx::LGFX_Sprite *spr,
    int16_t dst_x,
    int16_t dst_y,
    int16_t src_x,
    int16_t src_y,
    uint16_t w,
    uint16_t h,
    bool has_transparent,
    uint16_t transparent565)
{
    if (has_transparent) {
        if (sprite_push_region_trans_impl(spr, dst_x, dst_y, src_x, src_y, w, h, transparent565, 0)) {
            return true;
        }
        // fallthrough: try plain if "transparent overload" does not exist
    }
    return sprite_push_region_plain_impl(spr, dst_x, dst_y, src_x, src_y, w, h, 0);
}

extern "C" esp_err_t lgfx_device_sprite_create(uint16_t w, uint16_t h, uint8_t color_depth, uint8_t *out_handle)
{
    if (!out_handle || w == 0 || h == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t handle = lgfx_dev::alloc_sprite_handle_locked();
    if (handle == 0) {
        return ESP_ERR_NO_MEM;
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

    // LovyanGFX/M5GFX: createSprite typically returns a buffer pointer (null on failure).
    auto buf = spr->createSprite(w, h);
    if (!buf) {
        delete spr;
        return ESP_ERR_NO_MEM;
    }

    // Publish the sprite into the shared slot table and update the count.
    // (Both helpers require the LCD lock, which is held here.)
    lgfx_dev::set_sprite_locked(handle, spr);
    lgfx_dev::increment_sprite_count_locked();

    *out_handle = handle;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_sprite_delete(uint8_t handle)
{
    const uint8_t max_handle = lgfx_dev::max_handle_const();
    if (handle == 0 || handle > max_handle) {
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

extern "C" esp_err_t lgfx_device_sprite_set_color_depth(uint8_t handle, uint8_t depth)
{
    return lgfx_dev::with_sprite(handle, [&](lgfx::LGFX_Sprite *spr) { spr->setColorDepth(depth); });
}

extern "C" esp_err_t lgfx_device_sprite_create_palette(uint8_t handle)
{
    return lgfx_dev::with_sprite(handle, [&](lgfx::LGFX_Sprite *spr) { spr->createPalette(); });
}

extern "C" esp_err_t lgfx_device_sprite_set_palette_color(uint8_t handle, uint8_t index, uint16_t rgb565)
{
    return lgfx_dev::with_sprite(handle, [&](lgfx::LGFX_Sprite *spr) { spr->setPaletteColor(index, rgb565); });
}

extern "C" esp_err_t lgfx_device_sprite_set_pivot(uint8_t handle, int16_t px, int16_t py)
{
    return lgfx_dev::with_sprite(handle, [&](lgfx::LGFX_Sprite *spr) { spr->setPivot(px, py); });
}

extern "C" esp_err_t lgfx_device_sprite_push_sprite(uint8_t handle, int16_t x, int16_t y, bool has_transparent, uint16_t transparent_rgb565)
{
    return lgfx_dev::with_sprite(handle, [&](lgfx::LGFX_Sprite *spr) {
        if (has_transparent) {
            spr->pushSprite(x, y, transparent_rgb565);
        } else {
            spr->pushSprite(x, y);
        }
    });
}

extern "C" esp_err_t lgfx_device_sprite_push_rotate_zoom(
    uint8_t handle,
    int16_t x,
    int16_t y,
    int16_t angle_deg,
    uint16_t zoomx_q8_8,
    bool has_zoomy,
    uint16_t zoomy_q8_8)
{
    return lgfx_dev::with_sprite(handle, [&](lgfx::LGFX_Sprite *spr) {
        const float angle = (float) angle_deg;
        const float zx = ((float) zoomx_q8_8) / 256.0f;
        const float zy = has_zoomy ? (((float) zoomy_q8_8) / 256.0f) : zx;
        spr->pushRotateZoom(x, y, angle, zx, zy);
    });
}

extern "C" esp_err_t lgfx_device_sprite_push_sprite_region(
    uint8_t sprite_handle,
    int16_t dst_x,
    int16_t dst_y,
    int16_t src_x,
    int16_t src_y,
    uint16_t w,
    uint16_t h,
    bool has_transparent,
    uint16_t transparent565,
    bool *out_pushed)
{
    if (out_pushed) {
        *out_pushed = false;
    }

    const uint8_t max_handle = lgfx_dev::max_handle_const();
    if (sprite_handle == 0 || sprite_handle > max_handle || w == 0 || h == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *spr = lgfx_dev::resolve_sprite_locked(sprite_handle);
    if (!spr) {
        return ESP_ERR_NOT_FOUND;
    }

    const bool pushed = sprite_push_region_best_effort(
        spr, dst_x, dst_y, src_x, src_y, w, h, has_transparent, transparent565);

    if (out_pushed) {
        *out_pushed = pushed;
    }

    return pushed ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
}
