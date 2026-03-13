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

/*
 * Simple synchronous wrappers.
 *
 * These wrappers only populate a fixed-size job and call lgfx_worker_call().
 *
 * Keep exceptional wrappers hand-written in lgfx_worker_device.c:
 * - init/close
 * - getters that copy values back out
 * - wrappers that deep-copy variable-length payloads
 */
#define LGFX_WORKER_SIMPLE_DEVICE_STATE_WRAPPERS(X)                                                           \
    X(set_rotation, (lgfx_port_t * port, uint8_t rot), SET_ROTATION, set_rotation, .rot = rot)                \
    X(set_brightness, (lgfx_port_t * port, uint8_t brightness), SET_BRIGHTNESS, set_brightness,               \
        .b = brightness)                                                                                      \
    X(set_color_depth, (lgfx_port_t * port, uint8_t target, uint8_t depth), SET_COLOR_DEPTH, set_color_depth, \
        .target = target, .depth = depth)                                                                     \
    X(display, (lgfx_port_t * port), DISPLAY, display, ._ = 0u)

#define LGFX_WORKER_SIMPLE_PRIMITIVE_WRAPPERS(X)                                                                                                                                      \
    X(fill_screen, (lgfx_port_t * port, uint8_t target, uint16_t color565), FILL_SCREEN, fill_screen,                                                                                 \
        .target = target, .color565 = color565)                                                                                                                                       \
    X(clear, (lgfx_port_t * port, uint8_t target, uint16_t color565), CLEAR, clear,                                                                                                   \
        .target = target, .color565 = color565)                                                                                                                                       \
    X(draw_pixel, (lgfx_port_t * port, uint8_t target, int16_t x, int16_t y, uint16_t color565),                                                                                      \
        DRAW_PIXEL, draw_pixel, .target = target, .x = x, .y = y, .color565 = color565)                                                                                               \
    X(draw_fast_vline, (lgfx_port_t * port, uint8_t target, int16_t x, int16_t y, uint16_t h, uint16_t color565), DRAW_FAST_VLINE, draw_fast_vline, .target = target, .x = x, .y = y, \
        .h = h, .color565 = color565)                                                                                                                                                 \
    X(draw_fast_hline, (lgfx_port_t * port, uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t color565), DRAW_FAST_HLINE, draw_fast_hline, .target = target, .x = x, .y = y, \
        .w = w, .color565 = color565)                                                                                                                                                 \
    X(draw_line, (lgfx_port_t * port, uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color565), DRAW_LINE, draw_line, .target = target, .x0 = x0, .y0 = y0, \
        .x1 = x1, .y1 = y1, .color565 = color565)                                                                                                                                     \
    X(draw_rect, (lgfx_port_t * port, uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color565), DRAW_RECT, draw_rect, .target = target, .x = x, .y = y,       \
        .w = w, .h = h, .color565 = color565)                                                                                                                                         \
    X(fill_rect, (lgfx_port_t * port, uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color565), FILL_RECT, fill_rect, .target = target, .x = x, .y = y,       \
        .w = w, .h = h, .color565 = color565)                                                                                                                                         \
    X(draw_circle, (lgfx_port_t * port, uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t color565), DRAW_CIRCLE, draw_circle, .target = target, .x = x, .y = y,             \
        .r = r, .color565 = color565)                                                                                                                                                 \
    X(fill_circle, (lgfx_port_t * port, uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t color565), FILL_CIRCLE, fill_circle, .target = target, .x = x, .y = y,             \
        .r = r, .color565 = color565)                                                                                                                                                 \
    X(draw_triangle, (lgfx_port_t * port, uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color565), DRAW_TRIANGLE, draw_triangle,   \
        .target = target, .x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2,                                                                                                 \
        .color565 = color565)                                                                                                                                                         \
    X(fill_triangle, (lgfx_port_t * port, uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color565), FILL_TRIANGLE, fill_triangle,   \
        .target = target, .x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, .x2 = x2, .y2 = y2,                                                                                                 \
        .color565 = color565)

#define LGFX_WORKER_SIMPLE_TEXT_WRAPPERS(X)                                                                \
    X(set_text_size, (lgfx_port_t * port, uint8_t target, uint8_t size), SET_TEXT_SIZE, set_text_size,     \
        .target = target, .size = size)                                                                    \
    X(set_text_size_xy, (lgfx_port_t * port, uint8_t target, uint8_t sx, uint8_t sy),                      \
        SET_TEXT_SIZE_XY, set_text_size_xy, .target = target, .sx = sx, .sy = sy)                          \
    X(set_text_datum, (lgfx_port_t * port, uint8_t target, uint8_t datum), SET_TEXT_DATUM, set_text_datum, \
        .target = target, .datum = datum)                                                                  \
    X(set_text_wrap_xy, (lgfx_port_t * port, uint8_t target, bool wrap_x, bool wrap_y),                    \
        SET_TEXT_WRAP_XY, set_text_wrap_xy, .target = target, .wrap_x = wrap_x, .wrap_y = wrap_y)          \
    X(set_text_font, (lgfx_port_t * port, uint8_t target, uint8_t font), SET_TEXT_FONT, set_text_font,     \
        .target = target, .font = font)                                                                    \
    X(set_font_preset, (lgfx_port_t * port, uint8_t target, uint8_t preset), SET_FONT_PRESET,              \
        set_font_preset, .target = target, .preset = preset)                                               \
    X(set_text_color, (lgfx_port_t * port, uint8_t target, uint16_t fg565, bool has_bg, uint16_t bg565),   \
        SET_TEXT_COLOR, set_text_color, .target = target, .fg565 = fg565, .has_bg = has_bg, .bg565 = bg565)

#define LGFX_WORKER_SIMPLE_SPRITE_WRAPPERS(X)                                                                                                                                                                                                                  \
    X(create_sprite, (lgfx_port_t * port, uint8_t target, uint16_t w, uint16_t h, uint8_t color_depth),                                                                                                                                                        \
        CREATE_SPRITE, create_sprite, .target = target, .w = w, .h = h, .color_depth = color_depth)                                                                                                                                                            \
    X(delete_sprite, (lgfx_port_t * port, uint8_t target), DELETE_SPRITE, delete_sprite, .target = target)                                                                                                                                                     \
    X(set_pivot, (lgfx_port_t * port, uint8_t target, int16_t x, int16_t y), SET_PIVOT, set_pivot,                                                                                                                                                             \
        .target = target, .x = x, .y = y)                                                                                                                                                                                                                      \
    X(push_sprite, (lgfx_port_t * port, uint8_t src_target, uint8_t dst_target, int16_t x, int16_t y, bool has_transparent, uint16_t transparent565), PUSH_SPRITE, push_sprite,                                                                                \
        .src_target = src_target, .dst_target = dst_target, .x = x, .y = y,                                                                                                                                                                                    \
        .has_transparent = has_transparent, .transparent565 = transparent565)                                                                                                                                                                                  \
    X(push_rotate_zoom, (lgfx_port_t * port, uint8_t src_target, uint8_t dst_target, int16_t x, int16_t y, int32_t angle_x100, int32_t zoom_x_x1024, int32_t zoom_y_x1024, bool has_transparent, uint16_t transparent565), PUSH_ROTATE_ZOOM, push_rotate_zoom, \
        .src_target = src_target, .dst_target = dst_target, .x = x, .y = y,                                                                                                                                                                                    \
        .angle_x100 = angle_x100, .zoom_x_x1024 = zoom_x_x1024, .zoom_y_x1024 = zoom_y_x1024,                                                                                                                                                                  \
        .has_transparent = has_transparent, .transparent565 = transparent565)

#define LGFX_WORKER_SIMPLE_DEVICE_WRAPPERS(X)   \
    LGFX_WORKER_SIMPLE_DEVICE_STATE_WRAPPERS(X) \
    LGFX_WORKER_SIMPLE_PRIMITIVE_WRAPPERS(X)    \
    LGFX_WORKER_SIMPLE_TEXT_WRAPPERS(X)         \
    LGFX_WORKER_SIMPLE_SPRITE_WRAPPERS(X)

bool lgfx_worker_start(lgfx_port_t *port);
void lgfx_worker_stop(lgfx_port_t *port);

/*
 * Synchronous worker wrappers called from handlers on the port thread.
 *
 * These wrappers block until the worker finishes the device call.
 * Variable-length payload wrappers deep-copy before enqueueing.
 *
 * Exceptional cases:
 * - init/close use the calling lgfx_port_t as owner token
 * - init/get_dims use the port's persisted open_config_overrides snapshot
 * - getters copy outputs back to caller pointers
 *
 * See docs/WORKER_MODEL.md for concurrency and ownership rules.
 */

esp_err_t lgfx_worker_device_init(lgfx_port_t *port);
esp_err_t lgfx_worker_device_close(lgfx_port_t *port);

/*
 * Before init, returns dimensions implied by that port's persisted
 * open-config snapshot. If the same port already owns the live singleton,
 * returns current lcd->width/height.
 */
esp_err_t lgfx_worker_device_get_dims(lgfx_port_t *port, uint16_t *out_w, uint16_t *out_h);

/* LCD target 0, sprite handle 1..254. */
esp_err_t lgfx_worker_device_get_target_dims(
    lgfx_port_t *port,
    uint8_t target,
    uint16_t *out_w,
    uint16_t *out_h);

#define LGFX_WORKER_DECLARE_SIMPLE_WRAPPER(fn_name, params, ...) \
    esp_err_t lgfx_worker_device_##fn_name params;

LGFX_WORKER_SIMPLE_DEVICE_WRAPPERS(LGFX_WORKER_DECLARE_SIMPLE_WRAPPER)

#undef LGFX_WORKER_DECLARE_SIMPLE_WRAPPER

esp_err_t lgfx_worker_device_draw_string(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    const uint8_t *bytes,
    size_t len);

/* Touch is LCD-only. */
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

/* Returns 8 x u16 calibration values. */
esp_err_t lgfx_worker_device_calibrate_touch(
    lgfx_port_t *port,
    uint16_t out_params[8]);

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

#ifdef __cplusplus
}
#endif
