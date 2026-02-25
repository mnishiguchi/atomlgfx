// src/lgfx_device.h
#ifndef __LGFX_DEVICE_H__
#define __LGFX_DEVICE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// Build options (set via CMake target_compile_definitions)
// ----------------------------------------------------------------------------
//
// - LGFX_PORT_ENABLE_JP_FONTS=1
//     JP presets are supported (device maps jp_* to LovyanGFX JapanGothic font).
// - LGFX_PORT_ENABLE_JP_FONTS=0
//     JP presets are compiled out. Device must return ESP_ERR_NOT_SUPPORTED for jp_*.
//     ASCII preset remains available.
//
#ifndef LGFX_PORT_ENABLE_JP_FONTS
#define LGFX_PORT_ENABLE_JP_FONTS 1
#endif

// ----------------------------------------------------------------------------
// Targets (shared by most APIs)
// ----------------------------------------------------------------------------
//
// This is a thin C ABI around LovyanGFX:
// - target == 0       : LCD device (singleton)
// - target in 1..254  : sprite handle allocated by lgfx_device_sprite_create()
//                       or lgfx_device_sprite_create_at()
//
// Note:
// - "Valid target" means protocol-valid target range (0 for LCD, 1..254 for sprite slots).
// - Whether a sprite slot is currently allocated is checked by sprite operations and
//   typically returns ESP_ERR_NOT_FOUND when the slot is empty.
// - Most APIs return ESP_ERR_INVALID_STATE if lgfx_device_init() was not called.
bool lgfx_device_is_valid_target(uint8_t target);

// ----------------------------------------------------------------------------
// Caps / feature discovery (LCD-only)
// ----------------------------------------------------------------------------
// Used by ports/handlers/control.c
uint32_t lgfx_device_feature_bits(void);
uint32_t lgfx_device_max_sprites(void);

// ----------------------------------------------------------------------------
// Lifecycle / LCD-only controls
// ----------------------------------------------------------------------------
//
// init(): allocates the device lazily (no C++ global ctors) and calls lcd->begin().
//
// deinit(): idempotent teardown. Releases sprites and the LCD device, resets
//           globals so init() can be called again deterministically, and deletes
//           the mutex when supported by the FreeRTOS config.
//
// close(): protocol-facing alias of deinit().
esp_err_t lgfx_device_init(void);
esp_err_t lgfx_device_deinit(void);
esp_err_t lgfx_device_close(void);

esp_err_t lgfx_device_set_rotation(uint8_t rotation);
esp_err_t lgfx_device_set_brightness(uint8_t brightness);
esp_err_t lgfx_device_display(void);

// ----------------------------------------------------------------------------
// Common ops (LCD or sprite target)
// ----------------------------------------------------------------------------
esp_err_t lgfx_device_set_color_depth(uint8_t target, uint8_t depth);
esp_err_t lgfx_device_set_color(uint8_t target, uint16_t rgb565);
esp_err_t lgfx_device_set_base_color(uint8_t target, uint16_t rgb565);

// ----------------------------------------------------------------------------
// Text config (LCD or sprite target)
// ----------------------------------------------------------------------------
//
// Font preset IDs (stable protocol enum -> device-side mapping).
// Keep these in sync with ports/handlers/text.c decoding + worker mapping.
enum
{
    LGFX_FONT_PRESET_ASCII = 0,
    LGFX_FONT_PRESET_JP_SMALL = 1,
    LGFX_FONT_PRESET_JP_MEDIUM = 2,
    LGFX_FONT_PRESET_JP_LARGE = 3,
};

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
esp_err_t lgfx_device_set_font_preset(uint8_t target, uint8_t preset);

// ----------------------------------------------------------------------------
// Size queries (LCD or sprite target)
// ----------------------------------------------------------------------------
uint16_t lgfx_device_width(uint8_t target);
uint16_t lgfx_device_height(uint8_t target);

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

// ----------------------------------------------------------------------------
// Primitives (LCD or sprite target)
// ----------------------------------------------------------------------------
esp_err_t lgfx_device_draw_round_rect(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t rgb565);
esp_err_t lgfx_device_fill_round_rect(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t r, uint16_t rgb565);
esp_err_t lgfx_device_draw_circle(uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t rgb565);
esp_err_t lgfx_device_fill_circle(uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t rgb565);
esp_err_t lgfx_device_draw_ellipse(uint8_t target, int16_t x, int16_t y, uint16_t rx, uint16_t ry, uint16_t rgb565);
esp_err_t lgfx_device_fill_ellipse(uint8_t target, int16_t x, int16_t y, uint16_t rx, uint16_t ry, uint16_t rgb565);
esp_err_t lgfx_device_draw_triangle(uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t rgb565);
esp_err_t lgfx_device_fill_triangle(uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t rgb565);
esp_err_t lgfx_device_draw_bezier_q(uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t rgb565);
esp_err_t lgfx_device_draw_bezier_c(uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3, uint16_t rgb565);
esp_err_t lgfx_device_draw_arc(uint8_t target, int16_t x, int16_t y, uint16_t r0, uint16_t r1, int16_t a0, int16_t a1, uint16_t rgb565);
esp_err_t lgfx_device_fill_arc(uint8_t target, int16_t x, int16_t y, uint16_t r0, uint16_t r1, int16_t a0, int16_t a1, uint16_t rgb565);
esp_err_t lgfx_device_draw_gradient_line(uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c0, uint16_t c1);

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
// LCD write-path helpers (LCD-only by protocol semantics)
// ----------------------------------------------------------------------------
//
// These APIs are intended to be used in the "in_write" state managed by the
// protocol validator/handler layer.

// pushPixels (LCD-only)
esp_err_t lgfx_device_push_pixels_rgb565(const uint8_t *pixels_be, size_t pixels_len);

// set address window (LCD-only)
esp_err_t lgfx_device_set_addr_window(int16_t x, int16_t y, uint16_t w, uint16_t h);

// Bus / transaction helpers (LCD-only)
esp_err_t lgfx_device_begin_transaction(void);
esp_err_t lgfx_device_end_transaction(void);
esp_err_t lgfx_device_start_write(void);
esp_err_t lgfx_device_end_write(void);

// write* ops (LCD-only)
esp_err_t lgfx_device_write_pixel(int16_t x, int16_t y, uint16_t rgb565);
esp_err_t lgfx_device_write_fast_vline(int16_t x, int16_t y, uint16_t h, uint16_t rgb565);
esp_err_t lgfx_device_write_fast_hline(int16_t x, int16_t y, uint16_t w, uint16_t rgb565);
esp_err_t lgfx_device_write_fill_rect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t rgb565);

// ----------------------------------------------------------------------------
// Sprite subsystem
// ----------------------------------------------------------------------------
esp_err_t lgfx_device_sprite_create(uint16_t w, uint16_t h, uint8_t color_depth, uint8_t *out_handle);
esp_err_t lgfx_device_sprite_create_at(uint8_t handle, uint16_t w, uint16_t h, uint8_t color_depth);
esp_err_t lgfx_device_sprite_delete(uint8_t handle);
esp_err_t lgfx_device_sprite_set_color_depth(uint8_t handle, uint8_t depth);
esp_err_t lgfx_device_sprite_create_palette(uint8_t handle);
esp_err_t lgfx_device_sprite_set_palette_color(uint8_t handle, uint8_t index, uint16_t rgb565);
esp_err_t lgfx_device_sprite_set_pivot(uint8_t handle, int16_t px, int16_t py);
esp_err_t lgfx_device_sprite_push_sprite(
    uint8_t handle,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint16_t transparent_rgb565);

/*
 * Rotate + zoom sprite push.
 *
 * Worker/handler layers convert protocol wire values to float before calling here:
 * - angle_deg: degrees as float
 * - zoom_x / zoom_y: scale factors as float (must be > 0)
 *
 * Transparent-color overload is optional in some LovyanGFX/M5GFX variants.
 * This API uses best-effort dispatch and returns ESP_ERR_NOT_SUPPORTED if no
 * compatible pushRotateZoom overload is available.
 */
esp_err_t lgfx_device_sprite_push_rotate_zoom(
    uint8_t handle,
    int16_t x,
    int16_t y,
    float angle_deg,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    uint16_t transparent565);

esp_err_t lgfx_device_sprite_push_sprite_region(
    uint8_t sprite_handle,
    int16_t dst_x,
    int16_t dst_y,
    int16_t src_x,
    int16_t src_y,
    uint16_t w,
    uint16_t h,
    bool has_transparent,
    uint16_t transparent565,
    bool *out_pushed);

#ifdef __cplusplus
}
#endif

#endif
