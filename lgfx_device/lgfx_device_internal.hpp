// src/lgfx_device_internal.hpp
//
// Internal-only shared contract for split lgfx_device implementation files.
// Mutable singleton state is owned by src/lgfx_device_state.cpp.
//
// This header is C++-only and must not be exposed as public API.

#ifndef LGFX_DEVICE_INTERNAL_HPP
#define LGFX_DEVICE_INTERNAL_HPP

#include <stdint.h>

#include <LovyanGFX.hpp>

#include "esp_err.h"
#include "lgfx_device.h"

namespace lgfx_dev
{

// -----------------------------------------------------------------------------
// Shared constants / metadata (owned by lgfx_device_state.cpp)
// -----------------------------------------------------------------------------

uint16_t max_sprites_const();

// -----------------------------------------------------------------------------
// Shared lock / lifecycle helpers (owned by lgfx_device_state.cpp)
// -----------------------------------------------------------------------------
//
// Lifecycle terms used internally:
//
// - published
//     A singleton LGFX device object exists and has an owner token.
//     This is stronger than "some port opened", but weaker than "ready".
//
// - ready
//     The published singleton completed begin() successfully and may be used for
//     drawing / touch / sprite operations.
//
// Per-port open_config persistence lives on the port side (`lgfx_port_t`).
// This header only exposes process-global singleton helpers.
//

esp_err_t ensure_published();

bool lock_lcd();
void unlock_lcd();

class ScopedLcdLock
{
public:
    ScopedLcdLock() = default;
    ScopedLcdLock(const ScopedLcdLock &) = delete;
    ScopedLcdLock &operator=(const ScopedLcdLock &) = delete;

    void lock();
    bool is_locked() const;
    ~ScopedLcdLock();

private:
    bool locked_ = false;
};

// Acquires the LCD mutex and requires the singleton to be both published and
// ready (`begin()` completed).
esp_err_t lock_ready(ScopedLcdLock &lock);

// LovyanGFX-style write session helpers.
// These do not hold the mutex across multiple protocol calls; they only forward
// startWrite()/endWrite() to the live singleton under the normal ready checks.
esp_err_t start_write();
esp_err_t end_write();

// -----------------------------------------------------------------------------
// Shared state accessors (must be called while LCD lock is held unless noted)
// -----------------------------------------------------------------------------

// Returns LCD singleton as a generic LGFX device pointer (or nullptr).
lgfx::LGFX_Device *lcd_device_locked();

// Resolve target 0 => LCD, 1..MAX_HANDLE => sprite. Returns nullptr if invalid/missing.
lgfx::LGFXBase *resolve_target_locked(uint8_t target);

// Resolve sprite handle only (1..MAX_HANDLE). Returns nullptr if invalid/missing.
lgfx::LGFX_Sprite *resolve_sprite_locked(uint8_t handle);

// Sprite slot mutation helpers (for sprite create/delete split into sprites.cpp).
void set_sprite_locked(uint8_t handle, lgfx::LGFX_Sprite *spr);
void clear_sprite_locked(uint8_t handle);

void increment_sprite_count_locked();
void decrement_sprite_count_locked();
uint32_t sprite_count_locked();

// Teardown helper: deletes all sprite buffers + objects and resets registry.
void destroy_all_sprites_locked();

// -----------------------------------------------------------------------------
// Shared scalar color / palette helpers
// -----------------------------------------------------------------------------
//
// Important semantic rule:
// - palette-index mode is explicit
// - target color depth alone must not implicitly enable palette-index semantics
// - actual palette presence is required before index-bearing scalar arguments
//   become valid on sprite targets
//
// That keeps the device-layer checks aligned with the protocol contract:
// createPalette is the lifecycle step that enables palette-index usage.
//

static inline bool scalar_rgb565_is_valid(uint32_t value)
{
    return value <= 0xFFFFu;
}

static inline bool scalar_rgb888_is_valid(uint32_t value)
{
    return value <= 0xFFFFFFu;
}

static inline bool protocol_valid_target(uint8_t target)
{
    return lgfx_device_is_lcd_target(target) || lgfx_device_is_sprite_target(target);
}

static inline bool depth_supports_palette_storage(uint8_t depth)
{
    switch (depth) {
        case 1:
        case 2:
        case 4:
        case 8:
            return true;
        default:
            return false;
    }
}

static inline uint32_t palette_index_max_for_depth(uint8_t depth)
{
    switch (depth) {
        case 1:
            return 1u;
        case 2:
            return 3u;
        case 4:
            return 15u;
        case 8:
            return 255u;
        default:
            return 0u;
    }
}

static inline bool gfx_has_palette(const lgfx::LGFXBase *gfx)
{
    if (!gfx) {
        return false;
    }

    const uint8_t depth = static_cast<uint8_t>(gfx->getColorDepth());
    return depth_supports_palette_storage(depth) && gfx->hasPalette();
}

static inline bool gfx_uses_palette_indices(const lgfx::LGFXBase *gfx)
{
    return gfx_has_palette(gfx);
}

static inline uint32_t gfx_palette_index_max(const lgfx::LGFXBase *gfx)
{
    if (!gfx_has_palette(gfx)) {
        return 0u;
    }

    return palette_index_max_for_depth(static_cast<uint8_t>(gfx->getColorDepth()));
}

static inline bool sprite_supports_palette_storage(const lgfx::LGFX_Sprite *spr)
{
    if (!spr) {
        return false;
    }

    return depth_supports_palette_storage(static_cast<uint8_t>(spr->getColorDepth()));
}

static inline bool sprite_has_palette(const lgfx::LGFX_Sprite *spr)
{
    return spr && sprite_supports_palette_storage(spr) && spr->hasPalette();
}

static inline bool sprite_uses_palette_indices(const lgfx::LGFX_Sprite *spr)
{
    return sprite_has_palette(spr);
}

static inline uint32_t sprite_palette_index_max(const lgfx::LGFX_Sprite *spr)
{
    if (!sprite_has_palette(spr)) {
        return 0u;
    }

    return palette_index_max_for_depth(static_cast<uint8_t>(spr->getColorDepth()));
}

static inline esp_err_t validate_target_scalar_color(
    uint8_t target,
    lgfx::LGFXBase *gfx,
    bool color_is_index,
    uint32_t color_value)
{
    if (!protocol_valid_target(target)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!gfx) {
        return ESP_ERR_INVALID_STATE;
    }

    // RGB565 scalar colors are always explicit and valid on any real target,
    // including paletted sprites. The protocol uses flags to opt into
    // palette-index semantics; target depth alone must not reinterpret colors.
    if (!color_is_index) {
        return scalar_rgb565_is_valid(color_value) ? ESP_OK : ESP_ERR_INVALID_ARG;
    }

    // Palette-index scalar colors are never valid on LCD target.
    if (lgfx_device_is_lcd_target(target)) {
        return ESP_ERR_INVALID_ARG;
    }

    // Palette-index mode on sprite targets requires an actual palette to exist.
    if (!gfx_uses_palette_indices(gfx)) {
        return ESP_ERR_INVALID_ARG;
    }

    return (color_value <= gfx_palette_index_max(gfx))
        ? ESP_OK
        : ESP_ERR_INVALID_ARG;
}

static inline esp_err_t validate_sprite_transparent_scalar(
    const lgfx::LGFX_Sprite *src,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value)
{
    if (!has_transparent) {
        return ESP_OK;
    }

    if (!src) {
        return ESP_ERR_INVALID_STATE;
    }

    // Default transparent scalar is RGB565 and remains valid regardless of
    // sprite depth. Palette-index transparent mode is only valid when the
    // source sprite actually has a palette.
    if (!transparent_is_index) {
        return scalar_rgb565_is_valid(transparent_value) ? ESP_OK : ESP_ERR_INVALID_ARG;
    }

    if (!sprite_uses_palette_indices(src)) {
        return ESP_ERR_INVALID_ARG;
    }

    return (transparent_value <= sprite_palette_index_max(src))
        ? ESP_OK
        : ESP_ERR_INVALID_ARG;
}

// -----------------------------------------------------------------------------
// Shared wrappers (inline templates)
// -----------------------------------------------------------------------------

template <typename F>
inline esp_err_t with_lcd(F &&fn)
{
    ScopedLcdLock lock;
    esp_err_t err = lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *d = lcd_device_locked();
    if (!d) {
        return ESP_ERR_INVALID_STATE;
    }

    fn(d);
    return ESP_OK;
}

template <typename F>
inline esp_err_t with_target(uint8_t target, F &&fn)
{
    if (!protocol_valid_target(target)) {
        return ESP_ERR_INVALID_ARG;
    }

    ScopedLcdLock lock;
    esp_err_t err = lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    lgfx::LGFXBase *gfx = resolve_target_locked(target);
    if (!gfx) {
        return ESP_ERR_NOT_FOUND;
    }

    fn(gfx);
    return ESP_OK;
}

template <typename F>
inline esp_err_t with_sprite(uint8_t handle, F &&fn)
{
    if (!lgfx_device_is_sprite_target(handle)) {
        return ESP_ERR_INVALID_ARG;
    }

    ScopedLcdLock lock;
    esp_err_t err = lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    lgfx::LGFX_Sprite *spr = resolve_sprite_locked(handle);
    if (!spr) {
        return ESP_ERR_NOT_FOUND;
    }

    fn(spr);
    return ESP_OK;
}

} // namespace lgfx_dev

#endif // LGFX_DEVICE_INTERNAL_HPP
