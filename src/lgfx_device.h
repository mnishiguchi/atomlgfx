// src/lgfx_device.h
#ifndef __LGFX_DEVICE_H__
#define __LGFX_DEVICE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

// Build-time configuration (generated from lgfx_port/cmake/lgfx_port_config.h.in)
#include "lgfx_port/lgfx_port_config.h"

// Shared protocol-level constants (stable wire values, e.g., font preset IDs)
#include "lgfx_port/lgfx_port.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// Build options (must come from generated config header; no silent defaults)
// ----------------------------------------------------------------------------
//
// - LGFX_PORT_ENABLE_JP_FONTS=1
//     JP presets are supported (device maps jp_* to a JP font object).
// - LGFX_PORT_ENABLE_JP_FONTS=0
//     JP presets are compiled out. Device must return ESP_ERR_NOT_SUPPORTED for jp_*.
//     ASCII preset remains available.
//
#ifndef LGFX_PORT_ENABLE_JP_FONTS
#error "LGFX_PORT_ENABLE_JP_FONTS must be defined by lgfx_port_config.h"
#endif

#if (LGFX_PORT_ENABLE_JP_FONTS != 0) && (LGFX_PORT_ENABLE_JP_FONTS != 1)
#error "LGFX_PORT_ENABLE_JP_FONTS must be 0 or 1"
#endif

// ----------------------------------------------------------------------------
// Targets (shared by current protocol-backed APIs)
// ----------------------------------------------------------------------------
//
// This is a thin C ABI around the pinned LovyanGFX surface:
//
// - target == LGFX_DEVICE_TARGET_LCD (0)
//     LCD device (singleton)
//
// - target in LGFX_DEVICE_TARGET_MIN_SPRITE..LGFX_DEVICE_TARGET_MAX_SPRITE (1..254)
//     sprite handle allocated by lgfx_device_sprite_create_at()
//
// Notes:
// - “Valid target” means protocol-valid target range (0 for LCD, 1..254 for sprite slots).
// - Whether a sprite slot is currently allocated is checked by sprite operations and
//   typically returns ESP_ERR_NOT_FOUND when the slot is empty.
// - Most APIs return ESP_ERR_INVALID_STATE if lgfx_device_init() was not called.
//
#define LGFX_DEVICE_TARGET_LCD ((uint8_t) 0)
#define LGFX_DEVICE_TARGET_MIN_SPRITE ((uint8_t) 1)
#define LGFX_DEVICE_TARGET_MAX_SPRITE ((uint8_t) 254)

static inline bool lgfx_device_is_lcd_target(uint8_t target)
{
    return target == LGFX_DEVICE_TARGET_LCD;
}

static inline bool lgfx_device_is_sprite_target(uint8_t target)
{
    return (target >= LGFX_DEVICE_TARGET_MIN_SPRITE) && (target <= LGFX_DEVICE_TARGET_MAX_SPRITE);
}

// ----------------------------------------------------------------------------
// Lifecycle / LCD-only controls
// ----------------------------------------------------------------------------
//
// init(): allocates the device lazily (no C++ global ctors) and calls lcd->begin().
//
// close(): protocol-facing teardown. Releases sprites and the LCD device, resets
//          globals so init() can be called again deterministically, and deletes
//          the mutex when supported by the FreeRTOS config.
//
esp_err_t lgfx_device_init(void);
esp_err_t lgfx_device_close(void);

esp_err_t lgfx_device_set_rotation(uint8_t rotation);
esp_err_t lgfx_device_set_brightness(uint8_t brightness);
esp_err_t lgfx_device_display(void);

// ----------------------------------------------------------------------------
// Common ops (LCD or sprite target)
// ----------------------------------------------------------------------------
esp_err_t lgfx_device_set_color_depth(uint8_t target, uint8_t depth);

// ----------------------------------------------------------------------------
// Text config (LCD or sprite target)
// ----------------------------------------------------------------------------
//
// Font preset IDs are protocol-level constants (stable wire values) and are
// defined in include/lgfx_port/lgfx_port.h:
//
// - LGFX_FONT_PRESET_ASCII
// - LGFX_FONT_PRESET_JP_SMALL
// - LGFX_FONT_PRESET_JP_MEDIUM
// - LGFX_FONT_PRESET_JP_LARGE
//
esp_err_t lgfx_device_set_text_size(uint8_t target, uint8_t size);
esp_err_t lgfx_device_set_text_size_xy(uint8_t target, uint8_t sx, uint8_t sy);
esp_err_t lgfx_device_set_text_datum(uint8_t target, uint8_t datum);
esp_err_t lgfx_device_set_text_wrap(uint8_t target, bool wrap_x, bool wrap_y);
esp_err_t lgfx_device_set_text_font(uint8_t target, uint8_t font);

// setFontPreset(preset_id): selects a driver-defined font preset.
//
// Behavior contract:
// - Unknown preset IDs return ESP_ERR_INVALID_ARG
// - Optional presets that are compiled out return ESP_ERR_NOT_SUPPORTED
//
// Mapping strategy (current):
// - ASCII preset uses setTextFont(1) and normalizes size=1
// - JP presets may use a single JP font object and scale it with setTextSize()
//
esp_err_t lgfx_device_set_font_preset(uint8_t target, uint8_t preset);

// ----------------------------------------------------------------------------
// Size queries (LCD or sprite target)
// ----------------------------------------------------------------------------
//
// Target-aware dims query.
// - target == 0: LCD
// - target != 0: sprite handle (must exist, else ESP_ERR_NOT_FOUND)
//
esp_err_t lgfx_device_get_target_dims(uint8_t target, uint16_t *out_w, uint16_t *out_h);

// Convenience: get LCD dimensions in one call (used by worker to populate port cache)
// Returns PANEL_W/H before init, or the current lcd->width/height after init/rotation.
esp_err_t lgfx_device_get_dims(uint16_t *out_w, uint16_t *out_h);

// ----------------------------------------------------------------------------
// Touch (LCD-only by protocol semantics)
// ----------------------------------------------------------------------------
//
// Returned coordinates follow the device implementation:
// - get_touch: screen-space coordinates (after calibration mapping if configured)
// - get_touch_raw: raw coordinates (controller space)
//
// If not touched:
// - out_touched=false, other outputs set to 0 (best-effort convenience)
//
esp_err_t lgfx_device_get_touch(bool *out_touched, int16_t *out_x, int16_t *out_y, uint16_t *out_size);
esp_err_t lgfx_device_get_touch_raw(bool *out_touched, int16_t *out_x, int16_t *out_y, uint16_t *out_size);
esp_err_t lgfx_device_set_touch_calibrate(const uint16_t params[8]);

// Runs LovyanGFX interactive touch calibration and returns the resulting 8x u16 blob.
// Notes:
// - This call is blocking/interactive (user taps on-screen markers).
// - Returns ESP_ERR_NOT_SUPPORTED if no touch device is attached.
esp_err_t lgfx_device_calibrate_touch(uint16_t out_params[8]);

// ----------------------------------------------------------------------------
// Basic drawing (LCD or sprite target)
// ----------------------------------------------------------------------------
esp_err_t lgfx_device_fill_screen(uint8_t target, uint16_t rgb565);
esp_err_t lgfx_device_clear(uint8_t target, uint16_t rgb565);

esp_err_t lgfx_device_draw_pixel(uint8_t target, int16_t x, int16_t y, uint16_t rgb565);
esp_err_t lgfx_device_draw_fast_vline(uint8_t target, int16_t x, int16_t y, uint16_t h, uint16_t rgb565);
esp_err_t lgfx_device_draw_fast_hline(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t rgb565);
esp_err_t lgfx_device_draw_line(uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t rgb565);
esp_err_t lgfx_device_draw_rect(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t rgb565);
esp_err_t lgfx_device_fill_rect(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t rgb565);
esp_err_t lgfx_device_draw_circle(uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t rgb565);
esp_err_t lgfx_device_fill_circle(uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t rgb565);
esp_err_t lgfx_device_draw_triangle(uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t rgb565);
esp_err_t lgfx_device_fill_triangle(uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t rgb565);

// ----------------------------------------------------------------------------
// Text drawing (LCD or sprite target)
// ----------------------------------------------------------------------------
esp_err_t lgfx_device_set_text_color(uint8_t target, uint16_t fg_rgb565, bool has_bg, uint16_t bg_rgb565);
esp_err_t lgfx_device_draw_string(uint8_t target, int16_t x, int16_t y, const uint8_t *text, uint16_t text_len);

// ----------------------------------------------------------------------------
// Image transfer (LCD or sprite target)
// ----------------------------------------------------------------------------
esp_err_t lgfx_device_push_image_rgb565_strided(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t stride_pixels, // 0 means tightly packed (== w)
    const uint8_t *pixels_be, // RGB565 big-endian per pixel (hi, lo)
    size_t pixels_len);

// ----------------------------------------------------------------------------
// Sprite subsystem
// ----------------------------------------------------------------------------
esp_err_t lgfx_device_sprite_create_at(uint8_t handle, uint16_t w, uint16_t h, uint8_t color_depth);
esp_err_t lgfx_device_sprite_delete(uint8_t handle);
esp_err_t lgfx_device_sprite_set_pivot(uint8_t handle, int16_t px, int16_t py);

/*
 * Sprite push (whole-sprite, destination-aware).
 *
 * Destination:
 * - dst_target == 0: LCD
 * - dst_target != 0: sprite handle (must exist)
 *
 * This is a thin wrapper over the pinned LovyanGFX destination-aware overloads:
 * - pushSprite(dst, x, y)
 * - pushSprite(dst, x, y, transparent565)
 *
 * For invalid protocol targets this returns ESP_ERR_INVALID_ARG.
 * For missing source or destination sprites this returns ESP_ERR_NOT_FOUND.
 */
esp_err_t lgfx_device_sprite_push_sprite(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint16_t transparent_rgb565);

/*
 * Rotate + zoom sprite push (destination-aware).
 *
 * Worker/handler layers convert protocol wire values to float before calling here:
 * - angle_deg: degrees as float
 * - zoom_x / zoom_y: scale factors as float (must be > 0)
 *
 * Destination:
 * - dst_target == 0: LCD
 * - dst_target != 0: sprite handle (must exist)
 *
 * This is a thin wrapper over the pinned LovyanGFX destination-aware overloads:
 * - pushRotateZoom(dst, x, y, angle_deg, zoom_x, zoom_y)
 * - pushRotateZoom(dst, x, y, angle_deg, zoom_x, zoom_y, transparent565)
 */
esp_err_t lgfx_device_sprite_push_rotate_zoom(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    float angle_deg,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    uint16_t transparent_rgb565);

#ifdef __cplusplus
}
#endif

#endif
