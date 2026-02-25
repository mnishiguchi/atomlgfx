// ports/include/lgfx_port/worker.h
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "lgfx_port/lgfx_port.h"

#ifdef __cplusplus
extern "C" {
#endif

bool lgfx_worker_start(lgfx_port_t *port);
void lgfx_worker_stop(lgfx_port_t *port);

/*
 * Port-thread mailbox drain:
 * - Pull messages from the AtomVM mailbox
 * - Forward NormalMessage payloads to the port handler
 * - Dispose mailbox messages on the port thread
 *
 * The worker task itself remains term-free.
 */
void lgfx_worker_drain_mailbox(lgfx_port_t *port);

/*
 * Worker wrappers (called on the port thread by command handlers).
 *
 * These wrappers do not decode AtomVM terms and do not touch the AtomVM mailbox.
 * They build plain C arguments, enqueue a job, and block until the worker task
 * completes the lgfx_device_* call.
 *
 * Payload ownership / lifetime (variable-length buffers such as draw_string / push_image):
 * - Wrappers deep-copy caller-provided bytes into job-owned memory before enqueueing
 * - The worker executes the device call using the copied buffer
 * - The worker frees the copied buffer before notifying the caller
 *
 * Synchronous completion is still required because jobs are currently stack-allocated
 * in the wrappers and queued by pointer. If timeouts or async completion are added
 * later, switch to heap/pool-owned jobs or queue-by-value and define explicit
 * ownership rules for both the job object and any payload buffers.
 */

esp_err_t lgfx_worker_device_init(lgfx_port_t *port);
esp_err_t lgfx_worker_device_close(lgfx_port_t *port);
esp_err_t lgfx_worker_device_get_dims(lgfx_port_t *port, uint16_t *out_w, uint16_t *out_h);
esp_err_t lgfx_worker_device_set_rotation(lgfx_port_t *port, uint8_t rot);
esp_err_t lgfx_worker_device_set_brightness(lgfx_port_t *port, uint8_t b);
esp_err_t lgfx_worker_device_set_color_depth(lgfx_port_t *port, uint8_t target, uint8_t depth);
esp_err_t lgfx_worker_device_display(lgfx_port_t *port);

esp_err_t lgfx_worker_device_fill_screen(lgfx_port_t *port, uint8_t target, uint16_t color565);
esp_err_t lgfx_worker_device_clear(lgfx_port_t *port, uint8_t target, uint16_t color565);
esp_err_t lgfx_worker_device_draw_pixel(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t color565);

// Primitives (used by ports/handlers/primitives.c)
esp_err_t lgfx_worker_device_draw_fast_vline(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t h, uint16_t color565);
esp_err_t lgfx_worker_device_draw_fast_hline(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t color565);
esp_err_t lgfx_worker_device_draw_line(lgfx_port_t *port, uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color565);
esp_err_t lgfx_worker_device_draw_rect(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color565);
esp_err_t lgfx_worker_device_fill_rect(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color565);
esp_err_t lgfx_worker_device_draw_circle(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t color565);
esp_err_t lgfx_worker_device_fill_circle(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t color565);
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

// Text (used by ports/handlers/text.c)
esp_err_t lgfx_worker_device_set_text_size(lgfx_port_t *port, uint8_t target, uint8_t size);
esp_err_t lgfx_worker_device_set_text_datum(lgfx_port_t *port, uint8_t target, uint8_t datum);
esp_err_t lgfx_worker_device_set_text_wrap(lgfx_port_t *port, uint8_t target, bool wrap);
esp_err_t lgfx_worker_device_set_text_font(lgfx_port_t *port, uint8_t target, uint8_t font);

/*
 * Font preset wrapper:
 * - `preset` is a small stable protocol enum (e.g. ascii / jp_small / jp_medium)
 * - The device layer maps preset IDs to actual LovyanGFX font objects / fallbacks
 * - Unknown preset IDs return ESP_ERR_INVALID_ARG
 * - Unconfigured optional presets return ESP_ERR_NOT_SUPPORTED
 */
esp_err_t lgfx_worker_device_set_font_preset(lgfx_port_t *port, uint8_t target, uint8_t preset);

esp_err_t lgfx_worker_device_set_text_color(lgfx_port_t *port, uint8_t target, uint16_t fg565, bool has_bg, uint16_t bg565);

/*
 * Touch (LCD-only by protocol semantics)
 *
 * Returned coordinates follow the device implementation:
 * - get_touch: screen-space coordinates (after calibration mapping if configured)
 * - get_touch_raw: raw coordinates (controller space)
 *
 * If not touched:
 * - out_touched=false, other outputs set to 0 (best-effort convenience)
 */
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

/*
 * Touch calibration (LCD-only)
 *
 * Runs the interactive device calibration flow (LovyanGFX calibrateTouch) and
 * returns the resulting 8x u16 calibration blob.
 *
 * Notes:
 * - This is blocking and user-interactive (tapping corner markers).
 * - If touch is not attached/configured, device layer should return ESP_ERR_NOT_SUPPORTED.
 */
esp_err_t lgfx_worker_device_calibrate_touch(
    lgfx_port_t *port,
    uint16_t out_params[8]);

/*
 * Payload ownership contract (draw_string):
 * - bytes may point to caller-owned memory (including Erlang binary memory)
 * - This wrapper deep-copies len bytes into job-owned memory before enqueueing
 * - The worker executes the device call using the copied buffer
 * - The worker frees the copied buffer before notifying the caller
 *
 * This wrapper blocks until the worker completes the device call.
 */
esp_err_t lgfx_worker_device_draw_string(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    const uint8_t *bytes,
    uint16_t len);

/*
 * Payload ownership contract (push_image RGB565 strided):
 * - bytes may point to caller-owned memory (including Erlang binary memory)
 * - This wrapper deep-copies len bytes into job-owned memory before enqueueing
 * - The worker executes the device call using the copied buffer
 * - The worker frees the copied buffer before notifying the caller
 *
 * This wrapper blocks until the worker completes the device call.
 *
 * DMA note:
 * - Current cleanup assumes the device path fully consumes the payload before the
 *   device call returns
 * - If a future backend starts DMA/asynchronous transfer that outlives the device
 *   call, move to an explicit DMA-safe ownership model and free only after the
 *   DMA completion barrier
 */
esp_err_t lgfx_worker_device_push_image_rgb565_strided(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t stride_pixels, // 0 => tightly packed
    const uint8_t *bytes,
    size_t len);

// Sprite ops (used by ports/handlers/sprites.c)
//
// create_sprite:
// - `target` is sprite handle (1..254)
// - `color_depth` is the LovyanGFX sprite color depth
// - When protocol omits color depth, the handler should pass the configured default
//   (for example LGFX_PORT_SPRITE_DEFAULT_DEPTH)
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

esp_err_t lgfx_worker_device_push_sprite(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint16_t transparent565);

esp_err_t lgfx_worker_device_push_sprite_region(
    lgfx_port_t *port,
    uint8_t target,
    int16_t dst_x,
    int16_t dst_y,
    int16_t src_x,
    int16_t src_y,
    uint16_t w,
    uint16_t h,
    bool has_transparent,
    uint16_t transparent565);

/*
 * Rotate/zoom wrapper contract:
 * - angle_deg / zoom_x / zoom_y are worker-friendly float values
 * - Worker validates and converts them to the device ABI before calling
 *   lgfx_device_sprite_push_rotate_zoom():
 *   - angle_deg -> int16_t integer degrees (rounded)
 *   - zoom_x / zoom_y -> Q8.8 fixed-point (256 == 1.0x)
 *   - has_zoomy is derived from the converted zoom values
 * - has_transparent / transparent565 are kept in the wrapper signature for
 *   protocol compatibility, but the current device ABI does not support
 *   transparent-color rotate/zoom
 * - If has_transparent is true, the worker returns ESP_ERR_NOT_SUPPORTED
 */
esp_err_t lgfx_worker_device_push_rotate_zoom(
    lgfx_port_t *port,
    uint8_t target,
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
