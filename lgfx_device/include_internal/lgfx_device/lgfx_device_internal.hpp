// SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
//
// SPDX-License-Identifier: Apache-2.0

// lgfx_device/lgfx_device_internal.hpp
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
#include "lgfx_device/lgfx_device.h"

namespace lgfx_dev
{

struct LgfxRuntimeConfig;

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

// Locked write-session helpers.
// Caller must already hold the LCD lock and have passed ready-state checks.
esp_err_t start_write_locked();
esp_err_t end_write_locked();

// Board-preset runtime hooks.
// These are no-ops when no active board preset needs extra hardware handling.
esp_err_t board_preset_prepare_for_begin(const LgfxRuntimeConfig &config);
esp_err_t board_preset_apply_default_brightness_for_begin(const LgfxRuntimeConfig &config);
esp_err_t board_preset_set_brightness_if_needed(uint8_t brightness);
void board_preset_reset_runtime_state();

// -----------------------------------------------------------------------------
// Shared state accessors (must be called while LCD lock is held unless noted)
// -----------------------------------------------------------------------------

// Returns LCD singleton as a generic LGFX device pointer (or nullptr).
lgfx::LGFX_Device *lcd_device_locked();

// Resolve target 0 => LCD, 1..MAX_HANDLE => sprite. Returns nullptr if invalid/missing.
lgfx::LGFXBase *resolve_target_locked(uint8_t target);

// Resolve logical render target as a generic drawing target.
//
// Current behavior:
// - target 0 resolves to the active native presentation strip while a strip
//   frame is open
// - otherwise target 0 falls back to the live LCD
// - sprite targets resolve normally
//
// This keeps LCD control helpers bound to the real device while allowing
// logical LCD drawing to be redirected through the native presentation layer.
lgfx::LGFXBase *resolve_render_target_locked(uint8_t target);

// Resolve logical render surface for sprite push APIs.
//
// LovyanGFX sprite push APIs require a LovyanGFX* destination rather than a
// generic LGFXBase*.
//
// Current behavior matches resolve_render_target_locked() for LCD target 0:
// - use the active native presentation strip while a strip frame is open
// - otherwise fall back to the live LCD
lgfx::LovyanGFX *resolve_render_surface_locked(uint8_t target);

// Internal presentation / composition state helpers.
//
// Current native presentation model:
// - strip buffers are allocated lazily
// - allocation uses adaptive double strip buffers
// - target 0 drawing is redirected only while a native strip frame is active
// - fallback remains direct LCD rendering when strip allocation is unavailable
bool presentation_enabled_locked();
esp_err_t presentation_reset_locked();
esp_err_t presentation_configure_locked(
    uint16_t lcd_width,
    uint16_t lcd_height,
    uint16_t strip_height);
uint16_t presentation_strip_height_locked();
esp_err_t presentation_ensure_buffers_locked();
esp_err_t presentation_begin_strip_locked(uint16_t y0);
esp_err_t presentation_present_strip_locked();
esp_err_t presentation_rebuild_locked();
esp_err_t presentation_present_locked();
esp_err_t presentation_destroy_buffers_locked();
esp_err_t presentation_set_color_depth_locked(uint8_t depth);
esp_err_t presentation_set_swap_bytes_locked(bool enabled);

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
// Shared wrappers (ordinary sync path)
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
inline esp_err_t with_render_target(uint8_t target, F &&fn)
{
    if (!protocol_valid_target(target)) {
        return ESP_ERR_INVALID_ARG;
    }

    ScopedLcdLock lock;
    esp_err_t err = lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    lgfx::LGFXBase *gfx = resolve_render_target_locked(target);
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

// -----------------------------------------------------------------------------
// Shared wrappers (batch path; caller already holds LCD lock)
// -----------------------------------------------------------------------------

template <typename F>
inline esp_err_t with_lcd_locked(F &&fn)
{
    auto *d = lcd_device_locked();
    if (!d) {
        return ESP_ERR_INVALID_STATE;
    }

    fn(d);
    return ESP_OK;
}

template <typename F>
inline esp_err_t with_target_locked(uint8_t target, F &&fn)
{
    if (!protocol_valid_target(target)) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx::LGFXBase *gfx = resolve_target_locked(target);
    if (!gfx) {
        return ESP_ERR_NOT_FOUND;
    }

    fn(gfx);
    return ESP_OK;
}

template <typename F>
inline esp_err_t with_render_target_locked(uint8_t target, F &&fn)
{
    if (!protocol_valid_target(target)) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx::LGFXBase *gfx = resolve_render_target_locked(target);
    if (!gfx) {
        return ESP_ERR_NOT_FOUND;
    }

    fn(gfx);
    return ESP_OK;
}

template <typename F>
inline esp_err_t with_sprite_locked(uint8_t handle, F &&fn)
{
    if (!lgfx_device_is_sprite_target(handle)) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx::LGFX_Sprite *spr = resolve_sprite_locked(handle);
    if (!spr) {
        return ESP_ERR_NOT_FOUND;
    }

    fn(spr);
    return ESP_OK;
}

// -----------------------------------------------------------------------------
// Internal locked op entry points (batch-only hot path)
// -----------------------------------------------------------------------------

esp_err_t fill_screen_locked(
    uint8_t target,
    bool color_is_index,
    uint32_t color_value);

esp_err_t clear_locked(
    uint8_t target,
    bool color_is_index,
    uint32_t color_value);

esp_err_t fill_rect_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_pixel_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_rect_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_round_rect_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t r,
    bool color_is_index,
    uint32_t color_value);

esp_err_t fill_round_rect_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t r,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_circle_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t r,
    bool color_is_index,
    uint32_t color_value);

esp_err_t fill_circle_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t r,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_ellipse_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t rx,
    uint16_t ry,
    bool color_is_index,
    uint32_t color_value);

esp_err_t fill_ellipse_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t rx,
    uint16_t ry,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_arc_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t r0,
    uint16_t r1,
    float angle0,
    float angle1,
    bool color_is_index,
    uint32_t color_value);

esp_err_t fill_arc_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t r0,
    uint16_t r1,
    float angle0,
    float angle1,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_bezier3_locked(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_bezier4_locked(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    int16_t x3,
    int16_t y3,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_triangle_locked(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    bool color_is_index,
    uint32_t color_value);

esp_err_t fill_triangle_locked(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_fast_vline_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t h,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_fast_hline_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    bool color_is_index,
    uint32_t color_value);

esp_err_t draw_line_locked(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    bool color_is_index,
    uint32_t color_value);

esp_err_t set_clip_rect_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h);

esp_err_t clear_clip_rect_locked(uint8_t target);

esp_err_t set_text_size_locked(
    uint8_t target,
    float scale_x,
    float scale_y);

esp_err_t set_text_datum_locked(
    uint8_t target,
    uint8_t datum);

esp_err_t set_text_wrap_locked(
    uint8_t target,
    bool wrap_x,
    bool wrap_y);

esp_err_t set_text_font_preset_locked(
    uint8_t target,
    lgfx_font_preset_t preset);

esp_err_t set_text_color_locked(
    uint8_t target,
    bool fg_is_index,
    uint32_t fg_value,
    bool has_bg,
    bool bg_is_index,
    uint32_t bg_value);

esp_err_t set_cursor_locked(
    uint8_t target,
    int16_t x,
    int16_t y);

esp_err_t push_sprite_locked(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t dst_x,
    int16_t dst_y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value);

esp_err_t push_rotate_zoom_locked(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t dst_x,
    int16_t dst_y,
    float angle,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value);

} // namespace lgfx_dev

#endif // LGFX_DEVICE_INTERNAL_HPP
