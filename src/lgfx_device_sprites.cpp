// src/lgfx_device_sprites.cpp
// Sprite subsystem APIs and LovyanGFX/M5GFX variant helpers
// (push, region push, rotate/zoom).

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include "esp_log.h"

#include <cmath>
#include <new>
#include <type_traits>

namespace
{
static const char *TAG = "lgfx_device_sprites";

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
// Sprite push helpers (best-effort across LovyanGFX/M5GFX variants)
// Destination-aware when possible: pushSprite(dst, x, y [, transparent])
//
// Note: "fallback" in this file refers to overload availability across library
// variants, not wire/protocol compatibility.
// -----------------------------------------------------------------------------

template <typename S>
static auto sprite_push_plain_impl(S *spr, int16_t x, int16_t y, int)
    -> decltype(spr->pushSprite(x, y), bool())
{
    spr->pushSprite(x, y);
    return true;
}

template <typename S>
static bool sprite_push_plain_impl(S * /*spr*/, int16_t /*x*/, int16_t /*y*/, long)
{
    return false;
}

template <typename S>
static auto sprite_push_trans_impl(S *spr, int16_t x, int16_t y, uint16_t transparent565, int)
    -> decltype(spr->pushSprite(x, y, transparent565), bool())
{
    spr->pushSprite(x, y, transparent565);
    return true;
}

template <typename S>
static bool sprite_push_trans_impl(S * /*spr*/, int16_t /*x*/, int16_t /*y*/, uint16_t /*transparent565*/, long)
{
    return false;
}

// IMPORTANT:
// Do not type-erase dst to LGFXBase* for sprite->sprite calls.
// Some LovyanGFX/M5GFX variants provide overloads that require a more specific dst type
// (e.g., LGFX_Sprite*). Using the concrete pointer type keeps those overloads reachable.
template <typename S, typename DstT>
static auto sprite_push_plain_to_impl(S *spr, DstT *dst, int16_t x, int16_t y, int)
    -> decltype(spr->pushSprite(dst, x, y), bool())
{
    spr->pushSprite(dst, x, y);
    return true;
}

template <typename S, typename DstT>
static bool sprite_push_plain_to_impl(S * /*spr*/, DstT * /*dst*/, int16_t /*x*/, int16_t /*y*/, long)
{
    return false;
}

template <typename S, typename DstT>
static auto sprite_push_trans_to_impl(S *spr, DstT *dst, int16_t x, int16_t y, uint16_t transparent565, int)
    -> decltype(spr->pushSprite(dst, x, y, transparent565), bool())
{
    spr->pushSprite(dst, x, y, transparent565);
    return true;
}

template <typename S, typename DstT>
static bool sprite_push_trans_to_impl(
    S * /*spr*/,
    DstT * /*dst*/,
    int16_t /*x*/,
    int16_t /*y*/,
    uint16_t /*transparent565*/,
    long)
{
    return false;
}

template <typename DstT>
static bool sprite_push_sprite_best_effort(
    lgfx::LGFX_Sprite *spr,
    DstT *dst,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint16_t transparent565,
    bool allow_default_fallback)
{
    if (!spr || !dst) {
        return false;
    }

    if (has_transparent) {
        // Prefer destination-aware overload.
        if (sprite_push_trans_to_impl(spr, dst, x, y, transparent565, 0)) {
            return true;
        }

        // Only allow fallback to the no-dst overload when destination is LCD.
        if (allow_default_fallback) {
            return sprite_push_trans_impl(spr, x, y, transparent565, 0);
        }

        // Preserve semantics: don't silently drop dst or transparency.
        return false;
    }

    if (sprite_push_plain_to_impl(spr, dst, x, y, 0)) {
        return true;
    }

    if (allow_default_fallback) {
        return sprite_push_plain_impl(spr, x, y, 0);
    }

    return false;
}

// -----------------------------------------------------------------------------
// Sprite region push helper (best-effort across LovyanGFX/M5GFX variants)
// Destination-aware when possible: pushSprite(dst, dst_x, dst_y, src_x, src_y, w, h [, transparent])
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

template <typename S, typename DstT>
static auto sprite_push_region_plain_to_impl(
    S *spr,
    DstT *dst,
    int16_t dst_x,
    int16_t dst_y,
    int16_t src_x,
    int16_t src_y,
    uint16_t w,
    uint16_t h,
    int) -> decltype(spr->pushSprite(dst, dst_x, dst_y, src_x, src_y, w, h), bool())
{
    spr->pushSprite(dst, dst_x, dst_y, src_x, src_y, w, h);
    return true;
}

template <typename S, typename DstT>
static bool sprite_push_region_plain_to_impl(
    S * /*spr*/,
    DstT * /*dst*/,
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

template <typename S, typename DstT>
static auto sprite_push_region_trans_to_impl(
    S *spr,
    DstT *dst,
    int16_t dst_x,
    int16_t dst_y,
    int16_t src_x,
    int16_t src_y,
    uint16_t w,
    uint16_t h,
    uint16_t transparent565,
    int) -> decltype(spr->pushSprite(dst, dst_x, dst_y, src_x, src_y, w, h, transparent565), bool())
{
    spr->pushSprite(dst, dst_x, dst_y, src_x, src_y, w, h, transparent565);
    return true;
}

template <typename S, typename DstT>
static bool sprite_push_region_trans_to_impl(
    S * /*spr*/,
    DstT * /*dst*/,
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

        // Do not silently drop transparency. Only allow an overload fallback
        // when this is effectively a full-sprite push and the transparent full-push
        // overload is available.
        if (src_x == 0 && src_y == 0) {
            const int32_t sprite_w = spr->width();
            const int32_t sprite_h = spr->height();

            if (sprite_w >= 0 && sprite_h >= 0
                && static_cast<uint16_t>(sprite_w) == w
                && static_cast<uint16_t>(sprite_h) == h) {
                return sprite_push_trans_impl(spr, dst_x, dst_y, transparent565, 0);
            }
        }

        return false;
    }

    if (sprite_push_region_plain_impl(spr, dst_x, dst_y, src_x, src_y, w, h, 0)) {
        return true;
    }

    // Overload fallback: if caller is effectively pushing the whole sprite,
    // use the widely supported non-region pushSprite overload (some variants lack region overloads).
    if (src_x == 0 && src_y == 0) {
        const int32_t sprite_w = spr->width();
        const int32_t sprite_h = spr->height();

        if (sprite_w >= 0 && sprite_h >= 0
            && static_cast<uint16_t>(sprite_w) == w
            && static_cast<uint16_t>(sprite_h) == h) {
            return sprite_push_plain_impl(spr, dst_x, dst_y, 0);
        }
    }

    return false;
}

template <typename DstT>
static bool sprite_push_region_best_effort_to(
    lgfx::LGFX_Sprite *spr,
    DstT *dst,
    int16_t dst_x,
    int16_t dst_y,
    int16_t src_x,
    int16_t src_y,
    uint16_t w,
    uint16_t h,
    bool has_transparent,
    uint16_t transparent565,
    bool allow_default_fallback)
{
    if (!spr || !dst) {
        return false;
    }

    const bool is_full_sprite = [&]() -> bool {
        if (src_x != 0 || src_y != 0) {
            return false;
        }
        const int32_t sprite_w = spr->width();
        const int32_t sprite_h = spr->height();
        if (sprite_w < 0 || sprite_h < 0) {
            return false;
        }
        return static_cast<uint16_t>(sprite_w) == w && static_cast<uint16_t>(sprite_h) == h;
    }();

    if (has_transparent) {
        // Prefer destination-aware region overload.
        if (sprite_push_region_trans_to_impl(spr, dst, dst_x, dst_y, src_x, src_y, w, h, transparent565, 0)) {
            return true;
        }

        // Preserve semantics: if region is effectively whole-sprite, allow overload fallback
        // to destination-aware full push (still keeping transparency and destination).
        if (is_full_sprite) {
            if (sprite_push_trans_to_impl(spr, dst, dst_x, dst_y, transparent565, 0)) {
                return true;
            }
        }

        // Only allow fallback to the no-dst path when destination is LCD.
        if (allow_default_fallback) {
            return sprite_push_region_best_effort(spr, dst_x, dst_y, src_x, src_y, w, h, true, transparent565);
        }

        return false;
    }

    if (sprite_push_region_plain_to_impl(spr, dst, dst_x, dst_y, src_x, src_y, w, h, 0)) {
        return true;
    }

    if (is_full_sprite) {
        if (sprite_push_plain_to_impl(spr, dst, dst_x, dst_y, 0)) {
            return true;
        }
    }

    if (allow_default_fallback) {
        return sprite_push_region_best_effort(spr, dst_x, dst_y, src_x, src_y, w, h, false, 0);
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

template <typename S, typename DstT>
static auto sprite_push_rotate_zoom_plain_to_impl(
    S *spr,
    DstT *dst,
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

template <typename S, typename DstT>
static bool sprite_push_rotate_zoom_plain_to_impl(
    S * /*spr*/,
    DstT * /*dst*/,
    float /*dst_x*/,
    float /*dst_y*/,
    float /*angle_deg*/,
    float /*zoom_x*/,
    float /*zoom_y*/,
    long)
{
    return false;
}

template <typename S, typename DstT>
static auto sprite_push_rotate_zoom_trans_to_impl(
    S *spr,
    DstT *dst,
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

template <typename S, typename DstT>
static bool sprite_push_rotate_zoom_trans_to_impl(
    S * /*spr*/,
    DstT * /*dst*/,
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

template <typename DstT>
static bool sprite_push_rotate_zoom_best_effort(
    lgfx::LGFX_Sprite *spr,
    DstT *dst,
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

        // Only allow fallback to the no-dst overload when dst is LCD
        // (some variants implement only pushRotateZoom(x,y,...) and imply the parent device).
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
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint16_t transparent_rgb565)
{
    const uint8_t max_handle = lgfx_dev::max_handle_const();
    if (src_handle == 0 || src_handle > max_handle) {
        return ESP_ERR_INVALID_ARG;
    }

    // Destination target must be protocol-valid (0 LCD or sprite handle range).
    if (!lgfx_device_is_valid_target(dst_target)) {
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

    if (dst_target == 0) {
        auto *lcd = lgfx_dev::lcd_device_locked();
        if (!lcd) {
            return ESP_ERR_INVALID_STATE;
        }

        // dst is LCD => allow default (no-dst) fallback.
        const bool pushed = sprite_push_sprite_best_effort(src, lcd, x, y, has_transparent, transparent_rgb565, true);

        if (!pushed) {
            ESP_LOGW(TAG,
                "pushSprite unsupported: src=%u dst=%u(lcd) transparent=%s",
                (unsigned) src_handle, (unsigned) dst_target, has_transparent ? "true" : "false");
        }

        return pushed ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
    }

    auto *dst_spr = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst_spr) {
        return ESP_ERR_NOT_FOUND;
    }

    // dst is sprite => do NOT allow default fallback (would drop destination semantics).
    const bool pushed = sprite_push_sprite_best_effort(src, dst_spr, x, y, has_transparent, transparent_rgb565, false);

    if (!pushed) {
        ESP_LOGW(TAG,
            "pushSprite unsupported: src=%u dst=%u(sprite) transparent=%s",
            (unsigned) src_handle, (unsigned) dst_target, has_transparent ? "true" : "false");
    }

    return pushed ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
}

extern "C" esp_err_t lgfx_device_sprite_push_sprite_region(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t dst_x,
    int16_t dst_y,
    int16_t src_x,
    int16_t src_y,
    uint16_t w,
    uint16_t h,
    bool has_transparent,
    uint16_t transparent565)
{
    const uint8_t max_handle = lgfx_dev::max_handle_const();
    if (src_handle == 0 || src_handle > max_handle || w == 0 || h == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Destination target must be protocol-valid (0 LCD or sprite handle range).
    if (!lgfx_device_is_valid_target(dst_target)) {
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

    if (dst_target == 0) {
        auto *lcd = lgfx_dev::lcd_device_locked();
        if (!lcd) {
            return ESP_ERR_INVALID_STATE;
        }

        const bool pushed = sprite_push_region_best_effort_to(
            src,
            lcd,
            dst_x,
            dst_y,
            src_x,
            src_y,
            w,
            h,
            has_transparent,
            transparent565,
            true);

        if (!pushed) {
            ESP_LOGW(TAG,
                "pushSpriteRegion unsupported: src=%u dst=%u(lcd) transparent=%s dst_xy=(%d,%d) src_xy=(%d,%d) wh=(%u,%u)",
                (unsigned) src_handle, (unsigned) dst_target, has_transparent ? "true" : "false",
                (int) dst_x, (int) dst_y,
                (int) src_x, (int) src_y,
                (unsigned) w, (unsigned) h);
        }

        return pushed ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
    }

    auto *dst_spr = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst_spr) {
        return ESP_ERR_NOT_FOUND;
    }

    const bool pushed = sprite_push_region_best_effort_to(
        src,
        dst_spr,
        dst_x,
        dst_y,
        src_x,
        src_y,
        w,
        h,
        has_transparent,
        transparent565,
        false);

    if (!pushed) {
        ESP_LOGW(TAG,
            "pushSpriteRegion unsupported: src=%u dst=%u(sprite) transparent=%s dst_xy=(%d,%d) src_xy=(%d,%d) wh=(%u,%u)",
            (unsigned) src_handle, (unsigned) dst_target, has_transparent ? "true" : "false",
            (int) dst_x, (int) dst_y,
            (int) src_x, (int) src_y,
            (unsigned) w, (unsigned) h);
    }

    return pushed ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
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

    if (dst_target == 0) {
        auto *lcd = lgfx_dev::lcd_device_locked();
        if (!lcd) {
            return ESP_ERR_INVALID_STATE;
        }

        const bool pushed = sprite_push_rotate_zoom_best_effort(
            src,
            lcd,
            (float) x,
            (float) y,
            angle_deg,
            zoom_x,
            zoom_y,
            has_transparent,
            transparent565,
            true);

        if (!pushed) {
            const int32_t angle_cdeg = (int32_t) std::lroundf(angle_deg * 100.0f);
            const int32_t zx1024 = (int32_t) std::lroundf(zoom_x * 1024.0f);
            const int32_t zy1024 = (int32_t) std::lroundf(zoom_y * 1024.0f);

            ESP_LOGW(TAG,
                "pushRotateZoom unsupported: src=%u dst=%u(lcd) transparent=%s xy=(%d,%d) angle_cdeg=%ld zoom_x1024=(%ld,%ld)",
                (unsigned) src_handle, (unsigned) dst_target, has_transparent ? "true" : "false",
                (int) x, (int) y, (long) angle_cdeg, (long) zx1024, (long) zy1024);
        }

        return pushed ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
    }

    auto *dst_spr = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst_spr) {
        return ESP_ERR_NOT_FOUND;
    }

    const bool pushed = sprite_push_rotate_zoom_best_effort(
        src,
        dst_spr,
        (float) x,
        (float) y,
        angle_deg,
        zoom_x,
        zoom_y,
        has_transparent,
        transparent565,
        false);

    if (!pushed) {
        const int32_t angle_cdeg = (int32_t) std::lroundf(angle_deg * 100.0f);
        const int32_t zx1024 = (int32_t) std::lroundf(zoom_x * 1024.0f);
        const int32_t zy1024 = (int32_t) std::lroundf(zoom_y * 1024.0f);

        ESP_LOGW(TAG,
            "pushRotateZoom unsupported: src=%u dst=%u(sprite) transparent=%s xy=(%d,%d) angle_cdeg=%ld zoom_x1024=(%ld,%ld)",
            (unsigned) src_handle, (unsigned) dst_target, has_transparent ? "true" : "false",
            (int) x, (int) y, (long) angle_cdeg, (long) zx1024, (long) zy1024);
    }

    return pushed ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
}
