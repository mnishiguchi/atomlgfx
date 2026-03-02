// src/lgfx_device_sprites.cpp
// Sprite subsystem APIs and sprite compatibility helpers (region push, rotate/zoom).

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <cmath>
#include <new>
#include <type_traits>

namespace
{

// -----------------------------------------------------------------------------
// Sprite create helpers (normalize LovyanGFX / M5GFX return-type differences)
// -----------------------------------------------------------------------------

template <typename T>
static bool sprite_create_result_ok(T result)
{
    if constexpr (std::is_pointer<T>::value) {
        return result != nullptr;
    } else if constexpr (std::is_same<T, bool>::value) {
        return result;
    } else if constexpr (std::is_integral<T>::value) {
        return result != 0;
    } else {
        return true;
    }
}

template <typename S>
static auto sprite_create_best_effort_impl(S *spr, uint16_t w, uint16_t h, int)
    -> decltype(spr->createSprite(w, h), bool())
{
    using CreateRet = decltype(spr->createSprite(w, h));

    if constexpr (std::is_void<CreateRet>::value) {
        spr->createSprite(w, h);
        // Best effort probe after void-returning createSprite()
        return spr->getBuffer() != nullptr;
    } else {
        auto result = spr->createSprite(w, h);
        return sprite_create_result_ok(result);
    }
}

template <typename S>
static bool sprite_create_best_effort_impl(S * /*spr*/, uint16_t /*w*/, uint16_t /*h*/, long)
{
    return false;
}

template <typename S>
static bool sprite_create_best_effort(S *spr, uint16_t w, uint16_t h)
{
    return sprite_create_best_effort_impl(spr, w, h, 0);
}

// -----------------------------------------------------------------------------
// Palette create helpers (some variants may require an explicit palette size)
// -----------------------------------------------------------------------------

template <typename S>
static auto sprite_create_palette_impl(S *spr, int)
    -> decltype(spr->createPalette(), void())
{
    spr->createPalette();
}

template <typename S>
static auto sprite_create_palette_impl(S *spr, long)
    -> decltype(spr->createPalette(256), void())
{
    spr->createPalette(256);
}

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

        // Do not silently drop transparency. Only allow a compatibility fallback
        // if this is effectively a full-sprite push and the transparent full-push
        // overload is available.
        if (src_x == 0 && src_y == 0) {
            const int32_t sprite_w = spr->width();
            const int32_t sprite_h = spr->height();

            if (sprite_w >= 0 && sprite_h >= 0
                && static_cast<uint16_t>(sprite_w) == w
                && static_cast<uint16_t>(sprite_h) == h) {
                spr->pushSprite(dst_x, dst_y, transparent565);
                return true;
            }
        }

        return false;
    }

    if (sprite_push_region_plain_impl(spr, dst_x, dst_y, src_x, src_y, w, h, 0)) {
        return true;
    }

    // Compatibility fallback: if caller is effectively pushing the whole sprite,
    // use the widely supported non-region pushSprite overload.
    if (src_x == 0 && src_y == 0) {
        const int32_t sprite_w = spr->width();
        const int32_t sprite_h = spr->height();

        if (sprite_w >= 0 && sprite_h >= 0
            && static_cast<uint16_t>(sprite_w) == w
            && static_cast<uint16_t>(sprite_h) == h) {
            spr->pushSprite(dst_x, dst_y);
            return true;
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
// Sprite rotate/zoom helper (best-effort across LovyanGFX/M5GFX variants)
// Now destination-aware (LovyanGFX supports pushRotateZoom(dst, x, y, ...))
// -----------------------------------------------------------------------------

template <typename S>
static auto sprite_push_rotate_zoom_plain_impl(
    S *spr,
    float dst_x,
    float dst_y,
    float angle_deg,
    float zoom_x,
    float zoom_y,
    int) -> decltype(spr->pushRotateZoom(dst_x, dst_y, angle_deg, zoom_x, zoom_y), bool())
{
    spr->pushRotateZoom(dst_x, dst_y, angle_deg, zoom_x, zoom_y);
    return true;
}

template <typename S>
static bool sprite_push_rotate_zoom_plain_impl(
    S * /*spr*/,
    float /*dst_x*/,
    float /*dst_y*/,
    float /*angle_deg*/,
    float /*zoom_x*/,
    float /*zoom_y*/,
    long)
{
    return false;
}

template <typename S>
static auto sprite_push_rotate_zoom_trans_impl(
    S *spr,
    float dst_x,
    float dst_y,
    float angle_deg,
    float zoom_x,
    float zoom_y,
    uint16_t transparent565,
    int) -> decltype(spr->pushRotateZoom(dst_x, dst_y, angle_deg, zoom_x, zoom_y, transparent565), bool())
{
    spr->pushRotateZoom(dst_x, dst_y, angle_deg, zoom_x, zoom_y, transparent565);
    return true;
}

template <typename S>
static bool sprite_push_rotate_zoom_trans_impl(
    S * /*spr*/,
    float /*dst_x*/,
    float /*dst_y*/,
    float /*angle_deg*/,
    float /*zoom_x*/,
    float /*zoom_y*/,
    uint16_t /*transparent565*/,
    long)
{
    return false;
}

template <typename S>
static auto sprite_push_rotate_zoom_plain_to_impl(
    S *spr,
    lgfx::LovyanGFX *dst,
    float dst_x,
    float dst_y,
    float angle_deg,
    float zoom_x,
    float zoom_y,
    int) -> decltype(spr->pushRotateZoom(dst, dst_x, dst_y, angle_deg, zoom_x, zoom_y), bool())
{
    spr->pushRotateZoom(dst, dst_x, dst_y, angle_deg, zoom_x, zoom_y);
    return true;
}

template <typename S>
static bool sprite_push_rotate_zoom_plain_to_impl(
    S * /*spr*/,
    lgfx::LovyanGFX * /*dst*/,
    float /*dst_x*/,
    float /*dst_y*/,
    float /*angle_deg*/,
    float /*zoom_x*/,
    float /*zoom_y*/,
    long)
{
    return false;
}

template <typename S>
static auto sprite_push_rotate_zoom_trans_to_impl(
    S *spr,
    lgfx::LovyanGFX *dst,
    float dst_x,
    float dst_y,
    float angle_deg,
    float zoom_x,
    float zoom_y,
    uint16_t transparent565,
    int) -> decltype(spr->pushRotateZoom(dst, dst_x, dst_y, angle_deg, zoom_x, zoom_y, transparent565), bool())
{
    spr->pushRotateZoom(dst, dst_x, dst_y, angle_deg, zoom_x, zoom_y, transparent565);
    return true;
}

template <typename S>
static bool sprite_push_rotate_zoom_trans_to_impl(
    S * /*spr*/,
    lgfx::LovyanGFX * /*dst*/,
    float /*dst_x*/,
    float /*dst_y*/,
    float /*angle_deg*/,
    float /*zoom_x*/,
    float /*zoom_y*/,
    uint16_t /*transparent565*/,
    long)
{
    return false;
}

static bool sprite_push_rotate_zoom_best_effort(
    lgfx::LGFX_Sprite *spr,
    lgfx::LovyanGFX *dst,
    float dst_x,
    float dst_y,
    float angle_deg,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    uint16_t transparent565,
    bool allow_default_fallback)
{
    if (!spr || !dst) {
        return false;
    }

    if (has_transparent) {
        // Prefer destination-aware overload.
        if (sprite_push_rotate_zoom_trans_to_impl(spr, dst, dst_x, dst_y, angle_deg, zoom_x, zoom_y, transparent565, 0)) {
            return true;
        }

        // Only allow fallback to the no-dst overload when dst is effectively the sprite parent (LCD).
        if (allow_default_fallback) {
            return sprite_push_rotate_zoom_trans_impl(spr, dst_x, dst_y, angle_deg, zoom_x, zoom_y, transparent565, 0);
        }

        // Preserve semantics: don't silently drop dst or transparency.
        return false;
    }

    if (sprite_push_rotate_zoom_plain_to_impl(spr, dst, dst_x, dst_y, angle_deg, zoom_x, zoom_y, 0)) {
        return true;
    }

    if (allow_default_fallback) {
        return sprite_push_rotate_zoom_plain_impl(spr, dst_x, dst_y, angle_deg, zoom_x, zoom_y, 0);
    }

    return false;
}

} // namespace

extern "C" esp_err_t lgfx_device_sprite_create(uint16_t w, uint16_t h, uint8_t color_depth, uint8_t *out_handle)
{
    if (!out_handle || w == 0 || h == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *out_handle = 0;

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t handle = lgfx_dev::alloc_sprite_handle_locked();
    if (handle == 0) {
        return ESP_ERR_NO_MEM;
    }

    auto *spr = new (std::nothrow) lgfx::LGFX_Sprite(lcd);
    if (!spr) {
        return ESP_ERR_NO_MEM;
    }

    if (color_depth != 0) {
        spr->setColorDepth(color_depth);
    }

    if (!sprite_create_best_effort(spr, w, h)) {
        delete spr;
        return ESP_ERR_NO_MEM;
    }

    lgfx_dev::set_sprite_locked(handle, spr);
    lgfx_dev::increment_sprite_count_locked();

    *out_handle = handle;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_sprite_create_at(uint8_t handle, uint16_t w, uint16_t h, uint8_t color_depth)
{
    const uint8_t max_handle = lgfx_dev::max_handle_const();
    if (handle == 0 || handle > max_handle || w == 0 || h == 0) {
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

    if (!sprite_create_best_effort(spr, w, h)) {
        delete spr;
        return ESP_ERR_NO_MEM;
    }

    lgfx_dev::set_sprite_locked(handle, spr);
    lgfx_dev::increment_sprite_count_locked();

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
    return lgfx_dev::with_sprite(handle, [&](lgfx::LGFX_Sprite *spr) {
        sprite_create_palette_impl(spr, 0);
    });
}

extern "C" esp_err_t lgfx_device_sprite_set_palette_color(uint8_t handle, uint8_t index, uint16_t rgb565)
{
    return lgfx_dev::with_sprite(handle, [&](lgfx::LGFX_Sprite *spr) { spr->setPaletteColor(index, rgb565); });
}

extern "C" esp_err_t lgfx_device_sprite_set_pivot(uint8_t handle, int16_t px, int16_t py)
{
    return lgfx_dev::with_sprite(handle, [&](lgfx::LGFX_Sprite *spr) { spr->setPivot(px, py); });
}

extern "C" esp_err_t lgfx_device_sprite_push_sprite(
    uint8_t handle,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint16_t transparent_rgb565)
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
    const uint8_t max_handle = lgfx_dev::max_handle_const();
    if (src_handle == 0 || src_handle > max_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    // Destination target must be protocol-valid (0 LCD or sprite handle range).
    if (!lgfx_device_is_valid_target(dst_target)) {
        return ESP_ERR_INVALID_ARG;
    }

    // Protocol semantics require finite inputs and positive zoom.
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

    lgfx::LovyanGFX *dst = nullptr;

    if (dst_target == 0) {
        dst = lgfx_dev::lcd_device_locked();
        if (!dst) {
            return ESP_ERR_INVALID_STATE;
        }
    } else {
        auto *dst_spr = lgfx_dev::resolve_sprite_locked(dst_target);
        if (!dst_spr) {
            return ESP_ERR_NOT_FOUND;
        }
        dst = static_cast<lgfx::LovyanGFX *>(dst_spr);
    }

    // All sprites are constructed with the LCD as parent in this port.
    // Fallback to the no-dst overload is only valid when destination is LCD.
    const bool allow_default_fallback = (dst_target == 0);

    const bool pushed = sprite_push_rotate_zoom_best_effort(
        src,
        dst,
        (float) x,
        (float) y,
        angle_deg,
        zoom_x,
        zoom_y,
        has_transparent,
        transparent565,
        allow_default_fallback);

    return pushed ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
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
