// src/lgfx_device.h

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
// If JP fonts are compiled out, JP presets return ESP_ERR_NOT_SUPPORTED.
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
//
// get_dims_for_open_config():
// - if owner_token owns a live device, returns current lcd->width/height
// - otherwise returns effective panel dims from the provided snapshot layered
//   over build defaults
esp_err_t lgfx_device_init_with_open_config(
    const lgfx_open_config_overrides_t *overrides,
    const void *owner_token);

esp_err_t lgfx_device_close_for_owner(const void *owner_token);

esp_err_t lgfx_device_get_dims_for_open_config(
    const lgfx_open_config_overrides_t *overrides,
    const void *owner_token,
    uint16_t *out_w,
    uint16_t *out_h);

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
// Font preset IDs are protocol-level constants defined in lgfx_port/lgfx_port.h.
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
//
// setTextFont(font):
// - accepts raw u8 values in 0..255
// - forwarded as a numeric passthrough to the pinned LovyanGFX text font API
// - protocol does not define a smaller stable subset
// - for stable protocol-owned font selection, prefer setTextFontPreset()
esp_err_t lgfx_device_set_text_size(uint8_t target, uint8_t size);
esp_err_t lgfx_device_set_text_size_xy(uint8_t target, uint8_t sx, uint8_t sy);
esp_err_t lgfx_device_set_text_datum(uint8_t target, uint8_t datum);
esp_err_t lgfx_device_set_text_wrap(uint8_t target, bool wrap_x, bool wrap_y);
esp_err_t lgfx_device_set_text_font(uint8_t target, uint8_t font);

// setTextFontPreset(preset_id): selects a protocol-owned text font preset.
//
// Behavior:
// - unknown preset IDs return ESP_ERR_INVALID_ARG
// - compiled-out optional presets return ESP_ERR_NOT_SUPPORTED
//
// Current device mapping:
// - ASCII preset uses setTextFont(1) and normalizes size=1
// - JP presets may use one JP font object scaled via setTextSize()
esp_err_t lgfx_device_set_text_font_preset(uint8_t target, uint8_t preset);

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
esp_err_t lgfx_device_draw_string(uint8_t target, int16_t x, int16_t y, const uint8_t *text, size_t text_len);

// ----------------------------------------------------------------------------
// Image transfer (LCD or sprite target)
// ----------------------------------------------------------------------------
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
esp_err_t lgfx_device_sprite_set_pivot(uint8_t handle, int16_t px, int16_t py);

/*
 * Whole-sprite push to LCD or another sprite.
 *
 * Thin wrapper over pinned LovyanGFX destination-aware overloads:
 * - pushSprite(dst, x, y)
 * - pushSprite(dst, x, y, transparent565)
 *
 * Invalid protocol targets return ESP_ERR_INVALID_ARG.
 * Missing source or destination sprites return ESP_ERR_NOT_FOUND.
 */
esp_err_t lgfx_device_sprite_push_sprite(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint16_t transparent_rgb565);

/*
 * Rotate/zoom sprite push to LCD or another sprite.
 *
 * Worker/handler layers convert protocol units before calling here:
 * - angle_deg: float degrees
 * - zoom_x / zoom_y: float scale factors (> 0)
 *
 * Thin wrapper over pinned LovyanGFX destination-aware overloads:
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
