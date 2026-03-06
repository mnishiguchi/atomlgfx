// lgfx_port/include_internal/lgfx_port/worker.h
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "lgfx_port/lgfx_port.h"

#ifdef __cplusplus
extern "C" {
#endif

// Worker lifecycle.
bool lgfx_worker_start(lgfx_port_t *port);
void lgfx_worker_stop(lgfx_port_t *port);

// Port-thread mailbox drain. Mailbox ownership stays on the port thread.
void lgfx_worker_drain_mailbox(lgfx_port_t *port);

/*
 * Synchronous device wrappers called from handlers on the port thread.
 *
 * These wrappers:
 * - pass plain C arguments to the worker task
 * - block until the worker completes the device call
 * - deep-copy variable-length payloads before enqueueing when needed
 *
 * See docs/WORKER_MODEL.md for concurrency and ownership rules.
 */

// Device lifecycle.
esp_err_t lgfx_worker_device_init(lgfx_port_t *port);
esp_err_t lgfx_worker_device_close(lgfx_port_t *port);

// Dimensions.
esp_err_t lgfx_worker_device_get_dims(lgfx_port_t *port, uint16_t *out_w, uint16_t *out_h);

// Get target dimensions (LCD target 0, sprite handle 1..254).
// For sprite targets, returns ESP_ERR_NOT_FOUND when the handle is unallocated.
esp_err_t lgfx_worker_device_get_target_dims(
    lgfx_port_t *port,
    uint8_t target,
    uint16_t *out_w,
    uint16_t *out_h);

// Device state.
esp_err_t lgfx_worker_device_set_rotation(lgfx_port_t *port, uint8_t rot);
esp_err_t lgfx_worker_device_set_brightness(lgfx_port_t *port, uint8_t brightness);
esp_err_t lgfx_worker_device_set_color_depth(
    lgfx_port_t *port,
    uint8_t target,
    uint8_t depth);
esp_err_t lgfx_worker_device_display(lgfx_port_t *port);

// Drawing primitives.
esp_err_t lgfx_worker_device_fill_screen(
    lgfx_port_t *port,
    uint8_t target,
    uint16_t color565);

esp_err_t lgfx_worker_device_clear(
    lgfx_port_t *port,
    uint8_t target,
    uint16_t color565);

esp_err_t lgfx_worker_device_draw_pixel(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t color565);

esp_err_t lgfx_worker_device_draw_fast_vline(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t h,
    uint16_t color565);

esp_err_t lgfx_worker_device_draw_fast_hline(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t color565);

esp_err_t lgfx_worker_device_draw_line(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    uint16_t color565);

esp_err_t lgfx_worker_device_draw_rect(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t color565);

esp_err_t lgfx_worker_device_fill_rect(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t color565);

esp_err_t lgfx_worker_device_draw_circle(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t r,
    uint16_t color565);

esp_err_t lgfx_worker_device_fill_circle(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t r,
    uint16_t color565);

esp_err_t lgfx_worker_device_draw_triangle(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    uint16_t color565);

esp_err_t lgfx_worker_device_fill_triangle(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x0,
    int16_t y0,
    int16_t x1,
    int16_t y1,
    int16_t x2,
    int16_t y2,
    uint16_t color565);

// Text.
esp_err_t lgfx_worker_device_set_text_size(
    lgfx_port_t *port,
    uint8_t target,
    uint8_t size);

// X/Y overload. sy == 0 means "same as x".
esp_err_t lgfx_worker_device_set_text_size_xy(
    lgfx_port_t *port,
    uint8_t target,
    uint8_t sx,
    uint8_t sy);

esp_err_t lgfx_worker_device_set_text_datum(
    lgfx_port_t *port,
    uint8_t target,
    uint8_t datum);

esp_err_t lgfx_worker_device_set_text_wrap_xy(
    lgfx_port_t *port,
    uint8_t target,
    bool wrap_x,
    bool wrap_y);

esp_err_t lgfx_worker_device_set_text_font(
    lgfx_port_t *port,
    uint8_t target,
    uint8_t font);

esp_err_t lgfx_worker_device_set_font_preset(
    lgfx_port_t *port,
    uint8_t target,
    uint8_t preset);

esp_err_t lgfx_worker_device_set_text_color(
    lgfx_port_t *port,
    uint8_t target,
    uint16_t fg565,
    bool has_bg,
    uint16_t bg565);

esp_err_t lgfx_worker_device_draw_string(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    const uint8_t *bytes,
    uint16_t len);

// Touch (LCD only).
esp_err_t lgfx_worker_device_get_touch(
    lgfx_port_t *port,
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size);

esp_err_t lgfx_worker_device_get_touch_raw(
    lgfx_port_t *port,
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size);

esp_err_t lgfx_worker_device_set_touch_calibrate(
    lgfx_port_t *port,
    const uint16_t params[8]);

// Returns calibration data as 8 x u16.
esp_err_t lgfx_worker_device_calibrate_touch(
    lgfx_port_t *port,
    uint16_t out_params[8]);

// Images.
esp_err_t lgfx_worker_device_push_image_rgb565_strided(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t stride_pixels,
    const uint8_t *bytes,
    size_t len);

// Sprites.
esp_err_t lgfx_worker_device_create_sprite(
    lgfx_port_t *port,
    uint8_t target,
    uint16_t w,
    uint16_t h,
    uint8_t color_depth);

esp_err_t lgfx_worker_device_delete_sprite(lgfx_port_t *port, uint8_t target);

esp_err_t lgfx_worker_device_set_pivot(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y);

// Push a sprite to an LCD or sprite destination target.
esp_err_t lgfx_worker_device_push_sprite(
    lgfx_port_t *port,
    uint8_t src_target,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint16_t transparent565);

// Push a sprite with rotation and zoom to an LCD or sprite destination target.
esp_err_t lgfx_worker_device_push_rotate_zoom(
    lgfx_port_t *port,
    uint8_t src_target,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    float angle_deg,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    uint16_t transparent565);

#ifdef __cplusplus
}
#endif
