// lgfx_port/worker_internal.h
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

// We need lgfx_port_t for worker call signatures.
#include "lgfx_port/lgfx_port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    LGFX_JOB_INIT = 1,
    LGFX_JOB_CLOSE,
    LGFX_JOB_GET_DIMS,
    LGFX_JOB_SET_ROTATION,
    LGFX_JOB_SET_BRIGHTNESS,
    LGFX_JOB_SET_COLOR_DEPTH,
    LGFX_JOB_DISPLAY,

    // Touch (LCD-only)
    LGFX_JOB_GET_TOUCH,
    LGFX_JOB_GET_TOUCH_RAW,
    LGFX_JOB_SET_TOUCH_CALIBRATE,
    LGFX_JOB_CALIBRATE_TOUCH,

    LGFX_JOB_FILL_SCREEN,
    LGFX_JOB_CLEAR,
    LGFX_JOB_DRAW_PIXEL,
    LGFX_JOB_DRAW_FAST_VLINE,
    LGFX_JOB_DRAW_FAST_HLINE,
    LGFX_JOB_DRAW_LINE,
    LGFX_JOB_DRAW_RECT,
    LGFX_JOB_FILL_RECT,
    LGFX_JOB_DRAW_CIRCLE,
    LGFX_JOB_FILL_CIRCLE,
    LGFX_JOB_DRAW_TRIANGLE,
    LGFX_JOB_FILL_TRIANGLE,

    LGFX_JOB_SET_TEXT_SIZE,
    LGFX_JOB_SET_TEXT_DATUM,
    LGFX_JOB_SET_TEXT_WRAP,
    LGFX_JOB_SET_TEXT_FONT,
    LGFX_JOB_SET_FONT_PRESET,
    LGFX_JOB_SET_TEXT_COLOR,
    LGFX_JOB_DRAW_STRING,

    LGFX_JOB_PUSH_IMAGE_RGB565_STRIDED,

    // Sprite ops
    LGFX_JOB_CREATE_SPRITE,
    LGFX_JOB_DELETE_SPRITE,
    LGFX_JOB_SET_PIVOT,
    LGFX_JOB_PUSH_SPRITE,
    LGFX_JOB_PUSH_SPRITE_REGION,
    LGFX_JOB_PUSH_ROTATE_ZOOM,
} lgfx_job_kind_t;

typedef struct
{
    lgfx_job_kind_t kind;

    // Completion (set/read across port thread <-> worker task).
    // Keep this FreeRTOS-free in the header: TaskHandle_t is pointer-like.
    void *notify_task;
    esp_err_t err;

    // Optional heap-owned payload for variable-length byte buffers.
    // Wrappers may deep-copy caller-provided bytes here before enqueueing.
    // The worker frees this after the device call (before notify).
    uint8_t *owned_payload;

    union
    {
        struct
        {
            uint16_t w;
            uint16_t h;
        } get_dims;

        struct
        {
            uint8_t rot;
        } set_rotation;

        struct
        {
            uint8_t b;
        } set_brightness;

        struct
        {
            uint8_t target;
            uint8_t depth;
        } set_color_depth;

        // Touch (LCD-only)
        struct
        {
            bool touched;
            int16_t x;
            int16_t y;
            uint16_t size;
        } get_touch;

        struct
        {
            bool touched;
            int16_t x;
            int16_t y;
            uint16_t size;
        } get_touch_raw;

        struct
        {
            uint16_t params[8];
        } set_touch_calibrate;

        struct
        {
            uint16_t params[8];
        } calibrate_touch;

        struct
        {
            uint8_t target;
            uint16_t color565;
        } fill_screen;

        struct
        {
            uint8_t target;
            uint16_t color565;
        } clear;

        struct
        {
            uint8_t target;
            int16_t x;
            int16_t y;
            uint16_t color565;
        } draw_pixel;

        struct
        {
            uint8_t target;
            int16_t x;
            int16_t y;
            uint16_t h;
            uint16_t color565;
        } draw_fast_vline;

        struct
        {
            uint8_t target;
            int16_t x;
            int16_t y;
            uint16_t w;
            uint16_t color565;
        } draw_fast_hline;

        struct
        {
            uint8_t target;
            int16_t x0;
            int16_t y0;
            int16_t x1;
            int16_t y1;
            uint16_t color565;
        } draw_line;

        struct
        {
            uint8_t target;
            int16_t x;
            int16_t y;
            uint16_t w;
            uint16_t h;
            uint16_t color565;
        } draw_rect;

        struct
        {
            uint8_t target;
            int16_t x;
            int16_t y;
            uint16_t w;
            uint16_t h;
            uint16_t color565;
        } fill_rect;

        struct
        {
            uint8_t target;
            int16_t x;
            int16_t y;
            uint16_t r;
            uint16_t color565;
        } draw_circle;

        struct
        {
            uint8_t target;
            int16_t x;
            int16_t y;
            uint16_t r;
            uint16_t color565;
        } fill_circle;

        struct
        {
            uint8_t target;
            int16_t x0;
            int16_t y0;
            int16_t x1;
            int16_t y1;
            int16_t x2;
            int16_t y2;
            uint16_t color565;
        } draw_triangle;

        struct
        {
            uint8_t target;
            int16_t x0;
            int16_t y0;
            int16_t x1;
            int16_t y1;
            int16_t x2;
            int16_t y2;
            uint16_t color565;
        } fill_triangle;

        struct
        {
            uint8_t target;
            uint8_t size;
        } set_text_size;

        struct
        {
            uint8_t target;
            uint8_t datum;
        } set_text_datum;

        struct
        {
            uint8_t target;
            bool wrap;
        } set_text_wrap;

        struct
        {
            uint8_t target;
            uint8_t font;
        } set_text_font;

        struct
        {
            uint8_t target;
            uint8_t preset;
        } set_font_preset;

        struct
        {
            uint8_t target;
            uint16_t fg565;
            bool has_bg;
            uint16_t bg565;
        } set_text_color;

        struct
        {
            uint8_t target;
            int16_t x;
            int16_t y;
            const uint8_t *bytes;
            uint16_t len;
        } draw_string;

        struct
        {
            uint8_t target;
            int16_t x;
            int16_t y;
            uint16_t w;
            uint16_t h;
            uint16_t stride_pixels; // 0 => tightly packed
            const uint8_t *bytes;
            size_t len;
        } push_image;

        // Sprite ops
        struct
        {
            uint8_t target;
            uint16_t w;
            uint16_t h;
            uint8_t color_depth;
        } create_sprite;

        struct
        {
            uint8_t target;
        } delete_sprite;

        struct
        {
            uint8_t target;
            int16_t x;
            int16_t y;
        } set_pivot;

        struct
        {
            uint8_t target;
            int16_t x;
            int16_t y;
            bool has_transparent;
            uint16_t transparent565;
        } push_sprite;

        struct
        {
            uint8_t target;
            int16_t dst_x;
            int16_t dst_y;
            int16_t src_x;
            int16_t src_y;
            uint16_t w;
            uint16_t h;
            bool has_transparent;
            uint16_t transparent565;
        } push_sprite_region;

        struct
        {
            uint8_t src_target;
            uint8_t dst_target;
            int16_t x;
            int16_t y;
            float angle_deg;
            float zoom_x;
            float zoom_y;
            bool has_transparent;
            uint16_t transparent565;
        } push_rotate_zoom;
    } a;
} lgfx_job_t;

// Internal helper used by worker_device.c
esp_err_t lgfx_worker_call(lgfx_port_t *port, lgfx_job_t *job);

#ifdef __cplusplus
}
#endif
