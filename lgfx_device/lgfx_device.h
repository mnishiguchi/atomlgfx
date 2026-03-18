/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_device/lgfx_device.h

#ifndef __LGFX_DEVICE_H__
#define __LGFX_DEVICE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

// Build-time configuration (generated from lgfx_port/cmake/lgfx_port_config.h.in)
#include "lgfx_port/lgfx_port_config.h"

// Shared protocol-level constants (stable wire values, e.g. font preset IDs)
#include "lgfx_port/lgfx_port.h"

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// Build options
// ----------------------------------------------------------------------------
// Must come from the generated config header; no silent defaults.
// If Japanese fonts are compiled out, the Japanese-capable preset returns
// ESP_ERR_NOT_SUPPORTED.
#ifndef LGFX_PORT_ENABLE_JP_FONTS
#error "LGFX_PORT_ENABLE_JP_FONTS must be defined by lgfx_port_config.h"
#endif

#if (LGFX_PORT_ENABLE_JP_FONTS != 0) && (LGFX_PORT_ENABLE_JP_FONTS != 1)
#error "LGFX_PORT_ENABLE_JP_FONTS must be 0 or 1"
#endif

// ----------------------------------------------------------------------------
// Targets
// ----------------------------------------------------------------------------
// Thin C ABI over the pinned LovyanGFX surface.
//
// - 0: LCD singleton
// - 1..254: sprite handles
//
// A valid target is only a domain check. Sprite allocation is validated by the
// target-specific operations and missing sprites typically return ESP_ERR_NOT_FOUND.
// Most operations return ESP_ERR_INVALID_STATE until init_with_open_config()
// succeeds for the owning port.
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
// Scalar color ABI at the device boundary
// ----------------------------------------------------------------------------
//
// Handlers own wire decode.
// The device layer receives already-decoded scalar color arguments.
//
// For primitive, text, and sprite-transparent scalar colors:
//
// - when *_is_index == false:
//     *_value carries RGB565 in the low 16 bits
//
// - when *_is_index == true:
//     *_value carries palette index in the low 8 bits
//
// The device layer is authoritative for semantic checks such as:
//
// - palette-index mode on LCD target
// - palette-index mode on true-color sprite target
// - transparent palette index on non-paletted sprite paths
//
// Palette lifecycle is different:
// - create_palette acts on a sprite target
// - set_palette_color keeps rgb888 because the protocol defines that op on the
//   wire as 0x00RRGGBB
//

// ----------------------------------------------------------------------------
// Open-time panel driver override
// ----------------------------------------------------------------------------
// Parsed by the port layer; keep aligned with LGFXPort and lgfx_port.c.
typedef enum lgfx_panel_driver_id_t
{
    LGFX_PANEL_DRIVER_ID_ILI9488 = 1,
    LGFX_PANEL_DRIVER_ID_ILI9341 = 2,
    LGFX_PANEL_DRIVER_ID_ILI9341_2 = 3,
    LGFX_PANEL_DRIVER_ID_ST7789 = 4,
} lgfx_panel_driver_id_t;

// ----------------------------------------------------------------------------
// Open-time runtime overrides
// ----------------------------------------------------------------------------
// Parsed by the port layer and persisted on the owning port context.
//
// Current ownership model:
// - config persistence is per port
// - live device ownership is singleton-global
// - only one port may own the live singleton at a time
// - init() for a given port reuses that port's persisted snapshot
//
// Keep aligned with:
// - examples/elixir/lib/lgfx_port.ex
// - lgfx_port/lgfx_port.c
//
// Omitted keys keep build defaults. Duplicate keys are allowed; last value wins.
typedef struct lgfx_open_config_overrides_t
{
    uint8_t has_panel_driver;
    lgfx_panel_driver_id_t panel_driver;

    uint8_t has_width;
    uint16_t width;

    uint8_t has_height;
    uint16_t height;

    uint8_t has_offset_x;
    int32_t offset_x;

    uint8_t has_offset_y;
    int32_t offset_y;

    uint8_t has_offset_rotation;
    uint8_t offset_rotation;

    uint8_t has_readable;
    uint8_t readable;

    uint8_t has_invert;
    uint8_t invert;

    uint8_t has_rgb_order;
    uint8_t rgb_order;

    uint8_t has_dlen_16bit;
    uint8_t dlen_16bit;

    uint8_t has_lcd_spi_mode;
    uint8_t lcd_spi_mode;

    uint8_t has_lcd_freq_write_hz;
    uint32_t lcd_freq_write_hz;

    uint8_t has_lcd_freq_read_hz;
    uint32_t lcd_freq_read_hz;

    uint8_t has_lcd_dma_channel;
    int32_t lcd_dma_channel;

    uint8_t has_lcd_spi_3wire;
    uint8_t lcd_spi_3wire;

    uint8_t has_lcd_use_lock;
    uint8_t lcd_use_lock;

    uint8_t has_lcd_bus_shared;
    uint8_t lcd_bus_shared;

    uint8_t has_spi_sclk_gpio;
    int32_t spi_sclk_gpio;

    uint8_t has_spi_mosi_gpio;
    int32_t spi_mosi_gpio;

    uint8_t has_spi_miso_gpio;
    int32_t spi_miso_gpio;

    uint8_t has_lcd_spi_host;
    int32_t lcd_spi_host;

    uint8_t has_lcd_cs_gpio;
    int32_t lcd_cs_gpio;

    uint8_t has_lcd_dc_gpio;
    int32_t lcd_dc_gpio;

    uint8_t has_lcd_rst_gpio;
    int32_t lcd_rst_gpio;

    uint8_t has_lcd_pin_busy;
    int32_t lcd_pin_busy;

    uint8_t has_touch_cs_gpio;
    int32_t touch_cs_gpio;

    uint8_t has_touch_irq_gpio;
    int32_t touch_irq_gpio;

    uint8_t has_touch_spi_host;
    int32_t touch_spi_host;

    uint8_t has_touch_spi_freq_hz;
    uint32_t touch_spi_freq_hz;

    uint8_t has_touch_offset_rotation;
    uint8_t touch_offset_rotation;

    uint8_t has_touch_bus_shared;
    uint8_t touch_bus_shared;
} lgfx_open_config_overrides_t;

// ----------------------------------------------------------------------------
// Lifecycle / LCD-only controls
// ----------------------------------------------------------------------------
//
// init_with_open_config():
// - lazily allocates the singleton device
// - applies the provided per-port snapshot
// - binds live ownership to owner_token
// - calls lcd->begin()
//
// close_for_owner():
// - tears down the owning port's live device state
// - releases sprites, LCD device, and singleton ownership
// - resets globals so the next owner can init deterministically
// - force-unwinds any remaining LovyanGFX write nesting before teardown
//
// get_dims_for_open_config():
// - if owner_token owns a live device, returns current lcd->width/height
// - otherwise returns effective panel dims from the provided snapshot layered
//   over build defaults
//
// startWrite()/endWrite():
// - thin LCD-only alignment with LovyanGFX transaction control
// - nested calls are counted by LovyanGFX internally
// - endWrite() is a no-op when the count is already zero
esp_err_t lgfx_device_init_with_open_config(
    const lgfx_open_config_overrides_t *overrides,
    const void *owner_token);

esp_err_t lgfx_device_close_for_owner(const void *owner_token);

esp_err_t lgfx_device_get_dims_for_open_config(
    const lgfx_open_config_overrides_t *overrides,
    const void *owner_token,
    uint16_t *out_w,
    uint16_t *out_h);

esp_err_t lgfx_device_start_write(void);
esp_err_t lgfx_device_end_write(void);

esp_err_t lgfx_device_set_rotation(uint8_t rotation);
esp_err_t lgfx_device_set_brightness(uint8_t brightness);
esp_err_t lgfx_device_display(void);

// ----------------------------------------------------------------------------
// Common ops (LCD or sprite target)
// ----------------------------------------------------------------------------
esp_err_t lgfx_device_set_color_depth(uint8_t target, uint8_t depth);
esp_err_t lgfx_device_set_pivot(uint8_t target, int16_t px, int16_t py);

// ----------------------------------------------------------------------------
// Clipping (LCD or sprite target)
// ----------------------------------------------------------------------------
//
// Target behavior:
// - target 0 applies to the LCD singleton
// - sprite targets apply to that sprite only
//
// Coordinate behavior:
// - x/y are target-local signed coordinates
// - w/h must be > 0
//
// lgfx_device_clear_clip_rect():
// - removes any active clip rectangle for the target
esp_err_t lgfx_device_set_clip_rect(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h);
esp_err_t lgfx_device_clear_clip_rect(uint8_t target);

// ----------------------------------------------------------------------------
// Text config (LCD or sprite target)
// ----------------------------------------------------------------------------
// Font preset IDs are protocol-level constants defined in lgfx_port/lgfx_port.h.
//
// setTextSize(scale):
// - accepts positive x256 fixed-point values from the protocol/worker path
// - 256 => 1.0x
// - 384 => 1.5x
// - one-argument form applies the same scale to both axes
// - conversion to the LovyanGFX float API happens here at the device boundary
//
// setTextDatum(datum):
// - accepts raw u8 values in 0..255
// - forwarded as a numeric passthrough to the pinned LovyanGFX text datum API
// - protocol does not define a smaller stable subset
//
// setTextWrap(wrap_x, wrap_y):
// - explicit two-axis form at the device boundary
// - forwarded to the pinned LovyanGFX two-argument API
// - one-argument protocol/wrapper calls default wrap_y=false before calling here
esp_err_t lgfx_device_set_text_size(uint8_t target, uint16_t scale_x256);
esp_err_t lgfx_device_set_text_size_xy(uint8_t target, uint16_t scale_x_x256, uint16_t scale_y_x256);
esp_err_t lgfx_device_set_text_datum(uint8_t target, uint8_t datum);
esp_err_t lgfx_device_set_text_wrap(uint8_t target, bool wrap_x, bool wrap_y);
esp_err_t lgfx_device_set_cursor(uint8_t target, int16_t x, int16_t y);
esp_err_t lgfx_device_get_cursor(uint8_t target, int32_t *out_x, int32_t *out_y);
esp_err_t lgfx_device_print(uint8_t target, const uint8_t *text, size_t text_len);
esp_err_t lgfx_device_println(uint8_t target, const uint8_t *text, size_t text_len);

// setTextFontPreset(preset_id): selects a protocol-owned text font preset.
//
// Behavior:
// - unknown preset IDs return ESP_ERR_INVALID_ARG
// - compiled-out optional presets return ESP_ERR_NOT_SUPPORTED
//
// Current device mapping:
// - ASCII preset uses the pinned LovyanGFX default ASCII font internally and
//   normalizes text scale to 1.0x
// - Japanese-capable preset uses the built-in custom Japanese font subset and
//   normalizes text scale to 1.0x
//
// Text size remains a separate concern controlled by setTextSize().
esp_err_t lgfx_device_set_text_font_preset(uint8_t target, uint8_t preset);

// setTextColor():
// - fg/bg values arrive already decoded from handler wire semantics
// - when *_is_index == false, the corresponding *_value is RGB565
// - when *_is_index == true, the corresponding *_value is palette index
// - has_bg=false means bg fields are ignored
// - device layer is authoritative for target/depth semantic checks
esp_err_t lgfx_device_set_text_color(
    uint8_t target,
    bool fg_is_index,
    uint32_t fg_value,
    bool has_bg,
    bool bg_is_index,
    uint32_t bg_value);

// ----------------------------------------------------------------------------
// Size queries (LCD or sprite target)
// ----------------------------------------------------------------------------
// Returns LCD dims for target 0, or sprite dims for an existing sprite target.
esp_err_t lgfx_device_get_target_dims(uint8_t target, uint16_t *out_w, uint16_t *out_h);

// ----------------------------------------------------------------------------
// Touch (LCD-only by protocol semantics)
// ----------------------------------------------------------------------------
//
// Returned coordinates follow device semantics:
// - get_touch: screen-space coordinates
// - get_touch_raw: controller-space coordinates
//
// If not touched, out_touched=false and other outputs are zeroed.
esp_err_t lgfx_device_get_touch(bool *out_touched, int16_t *out_x, int16_t *out_y, uint16_t *out_size);
esp_err_t lgfx_device_get_touch_raw(bool *out_touched, int16_t *out_x, int16_t *out_y, uint16_t *out_size);
esp_err_t lgfx_device_set_touch_calibrate(const uint16_t params[8]);

// Runs interactive touch calibration and returns the resulting 8x u16 blob.
// Blocking. Returns ESP_ERR_NOT_SUPPORTED if no touch device is attached.
esp_err_t lgfx_device_calibrate_touch(uint16_t out_params[8]);

// ----------------------------------------------------------------------------
// Basic drawing (LCD or sprite target)
// ----------------------------------------------------------------------------
//
// Color-bearing primitive ops are mode-aware at this boundary:
//
// - color_is_index=false:
//     color_value is RGB565
//
// - color_is_index=true:
//     color_value is palette index
//
// The device layer is authoritative for rejecting invalid combinations such as
// palette-index mode on LCD target or true-color sprite targets.
esp_err_t lgfx_device_fill_screen(uint8_t target, bool color_is_index, uint32_t color_value);
esp_err_t lgfx_device_clear(uint8_t target, bool color_is_index, uint32_t color_value);

esp_err_t lgfx_device_draw_pixel(uint8_t target, int16_t x, int16_t y, bool color_is_index, uint32_t color_value);
esp_err_t lgfx_device_draw_fast_vline(uint8_t target, int16_t x, int16_t y, uint16_t h, bool color_is_index, uint32_t color_value);
esp_err_t lgfx_device_draw_fast_hline(uint8_t target, int16_t x, int16_t y, uint16_t w, bool color_is_index, uint32_t color_value);
esp_err_t lgfx_device_draw_line(uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool color_is_index, uint32_t color_value);
esp_err_t lgfx_device_draw_rect(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, bool color_is_index, uint32_t color_value);
esp_err_t lgfx_device_fill_rect(uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, bool color_is_index, uint32_t color_value);
esp_err_t lgfx_device_draw_circle(uint8_t target, int16_t x, int16_t y, uint16_t r, bool color_is_index, uint32_t color_value);
esp_err_t lgfx_device_fill_circle(uint8_t target, int16_t x, int16_t y, uint16_t r, bool color_is_index, uint32_t color_value);
esp_err_t lgfx_device_draw_triangle(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    bool color_is_index,
    uint32_t color_value);
esp_err_t lgfx_device_fill_triangle(
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    bool color_is_index,
    uint32_t color_value);

// ----------------------------------------------------------------------------
// Text drawing (LCD or sprite target)
// ----------------------------------------------------------------------------
esp_err_t lgfx_device_draw_string(uint8_t target, int16_t x, int16_t y, const uint8_t *text, size_t text_len);

// ----------------------------------------------------------------------------
// Image transfer (LCD or sprite target)
// ----------------------------------------------------------------------------

// JPEG draw from an in-memory payload.
//
// Worker / protocol path provides:
// - target-local x/y
// - optional max_w / max_h crop bounds (0 means LovyanGFX default behavior)
// - optional off_x / off_y source offsets
// - positive x1024 fixed-point scales
//
// Conversion to the LovyanGFX float API happens here at the device boundary.
// The payload must remain valid for the duration of the call only.
esp_err_t lgfx_device_draw_jpg(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t max_w,
    uint16_t max_h,
    int16_t off_x,
    int16_t off_y,
    int32_t scale_x_x1024,
    int32_t scale_y_x1024,
    const uint8_t *jpeg_bytes,
    size_t jpeg_len);

esp_err_t lgfx_device_push_image_rgb565_strided(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t stride_pixels, // 0 => tightly packed (== w)
    const uint8_t *pixels_be, // RGB565 big-endian pixels: hi, lo
    size_t pixels_len);

// ----------------------------------------------------------------------------
// Sprite subsystem
// ----------------------------------------------------------------------------
esp_err_t lgfx_device_sprite_create_at(uint8_t handle, uint16_t w, uint16_t h, uint8_t color_depth);
esp_err_t lgfx_device_sprite_delete(uint8_t handle);

// Palette lifecycle:
//
// - sprite target only
// - requires an existing paletted sprite target
// - create_palette establishes palette backing for that sprite
// - set_palette_color writes one palette entry using RGB888 input
esp_err_t lgfx_device_sprite_create_palette(uint8_t handle);
esp_err_t lgfx_device_sprite_set_palette_color(uint8_t handle, uint8_t palette_index, uint32_t rgb888);

/*
 * Whole-sprite push to LCD or another sprite.
 *
 * Thin wrapper over pinned LovyanGFX destination-aware overloads.
 *
 * Transparent semantics at this boundary:
 * - has_transparent=false:
 *     no transparent key
 * - has_transparent=true && transparent_is_index=false:
 *     transparent_value is RGB565
 * - has_transparent=true && transparent_is_index=true:
 *     transparent_value is palette index
 *
 * Invalid protocol targets return ESP_ERR_INVALID_ARG.
 * Missing source or destination sprites return ESP_ERR_NOT_FOUND.
 * Device layer remains authoritative for paletted-vs-true-color checks.
 */
esp_err_t lgfx_device_sprite_push_sprite(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value);

/*
 * Rotate/zoom sprite push to LCD or another sprite.
 *
 * Worker/handler layers convert protocol units before calling here:
 * - angle_deg: float degrees
 * - zoom_x / zoom_y: float scale factors (> 0)
 *
 * Transparent semantics match lgfx_device_sprite_push_sprite().
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
    bool transparent_is_index,
    uint32_t transparent_value);

#ifdef __cplusplus
}
#endif

#endif
