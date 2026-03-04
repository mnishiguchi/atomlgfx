// lgfx_port/lgfx_worker_device.c
//
// Public device wrappers (called from handlers on the port thread)
//
// These wrappers do not decode AtomVM terms and do not touch the AtomVM mailbox.
// They build plain C arguments, enqueue a job, and block until the worker task
// completes the lgfx_device_* call.
//
// Payload ownership / lifetime (variable-length buffers such as draw_string / push_image):
// - Wrappers deep-copy caller-provided bytes into job-owned memory before enqueueing
// - The worker executes the device call using the copied buffer
// - The worker frees the copied buffer before notifying the caller
//
// Synchronous completion is still required because jobs are currently stack-allocated
// in the wrappers and queued by pointer. If timeouts or async completion are added
// later, switch to heap/pool-owned jobs or queue-by-value and define explicit
// ownership rules for both the job object and any payload buffers.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"

#include "lgfx_port/worker.h"

// Canonical job layout (enum + job struct + union members)
#include "lgfx_port/worker_jobs.h"

// Internal helper used by worker_device.c (lgfx_worker_call)
#include "lgfx_port/worker_internal.h"

static esp_err_t lgfx_worker_copy_payload(lgfx_job_t *job, const uint8_t *bytes, size_t len)
{
    if (!job) {
        return ESP_ERR_INVALID_ARG;
    }

    job->owned_payload = NULL;

    if (len == 0) {
        return ESP_OK;
    }

    if (!bytes) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *copy = (uint8_t *) malloc(len);
    if (!copy) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(copy, bytes, len);
    job->owned_payload = copy;
    return ESP_OK;
}

esp_err_t lgfx_worker_device_init(lgfx_port_t *port)
{
    lgfx_job_t job = { .kind = LGFX_JOB_INIT };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_close(lgfx_port_t *port)
{
    lgfx_job_t job = { .kind = LGFX_JOB_CLOSE };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_get_dims(lgfx_port_t *port, uint16_t *out_w, uint16_t *out_h)
{
    if (!out_w || !out_h) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_job_t job = { .kind = LGFX_JOB_GET_DIMS };
    esp_err_t err = lgfx_worker_call(port, &job);
    if (err == ESP_OK) {
        *out_w = job.a.get_dims.w;
        *out_h = job.a.get_dims.h;
    }
    return err;
}

esp_err_t lgfx_worker_device_get_target_dims(lgfx_port_t *port, uint8_t target, uint16_t *out_w, uint16_t *out_h)
{
    if (!out_w || !out_h) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_job_t job = {
        .kind = LGFX_JOB_GET_TARGET_DIMS,
        .a.get_target_dims = { .target = target, .w = 0, .h = 0 }
    };

    esp_err_t err = lgfx_worker_call(port, &job);
    if (err == ESP_OK) {
        *out_w = job.a.get_target_dims.w;
        *out_h = job.a.get_target_dims.h;
    }
    return err;
}

esp_err_t lgfx_worker_device_set_rotation(lgfx_port_t *port, uint8_t rot)
{
    lgfx_job_t job = { .kind = LGFX_JOB_SET_ROTATION, .a.set_rotation = { .rot = rot } };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_set_brightness(lgfx_port_t *port, uint8_t b)
{
    lgfx_job_t job = { .kind = LGFX_JOB_SET_BRIGHTNESS, .a.set_brightness = { .b = b } };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_set_color_depth(lgfx_port_t *port, uint8_t target, uint8_t depth)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_SET_COLOR_DEPTH,
        .a.set_color_depth = { .target = target, .depth = depth }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_display(lgfx_port_t *port)
{
    lgfx_job_t job = { .kind = LGFX_JOB_DISPLAY };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_get_touch(
    lgfx_port_t *port,
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size)
{
    if (!out_touched || !out_x || !out_y || !out_size) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_job_t job = { .kind = LGFX_JOB_GET_TOUCH };
    esp_err_t err = lgfx_worker_call(port, &job);
    if (err == ESP_OK) {
        *out_touched = job.a.get_touch.touched;
        *out_x = job.a.get_touch.x;
        *out_y = job.a.get_touch.y;
        *out_size = job.a.get_touch.size;
    }
    return err;
}

esp_err_t lgfx_worker_device_get_touch_raw(
    lgfx_port_t *port,
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size)
{
    if (!out_touched || !out_x || !out_y || !out_size) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_job_t job = { .kind = LGFX_JOB_GET_TOUCH_RAW };
    esp_err_t err = lgfx_worker_call(port, &job);
    if (err == ESP_OK) {
        *out_touched = job.a.get_touch_raw.touched;
        *out_x = job.a.get_touch_raw.x;
        *out_y = job.a.get_touch_raw.y;
        *out_size = job.a.get_touch_raw.size;
    }
    return err;
}

esp_err_t lgfx_worker_device_set_touch_calibrate(lgfx_port_t *port, const uint16_t params[8])
{
    if (!params) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_job_t job = { .kind = LGFX_JOB_SET_TOUCH_CALIBRATE };
    memcpy(job.a.set_touch_calibrate.params, params, sizeof(job.a.set_touch_calibrate.params));
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_calibrate_touch(lgfx_port_t *port, uint16_t out_params[8])
{
    if (!out_params) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_job_t job = { .kind = LGFX_JOB_CALIBRATE_TOUCH };
    esp_err_t err = lgfx_worker_call(port, &job);
    if (err == ESP_OK) {
        memcpy(out_params, job.a.calibrate_touch.params, sizeof(job.a.calibrate_touch.params));
    }
    return err;
}

esp_err_t lgfx_worker_device_fill_screen(lgfx_port_t *port, uint8_t target, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_FILL_SCREEN,
        .a.fill_screen = { .target = target, .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_clear(lgfx_port_t *port, uint8_t target, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_CLEAR,
        .a.clear = { .target = target, .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_draw_pixel(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_DRAW_PIXEL,
        .a.draw_pixel = { .target = target, .x = x, .y = y, .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_draw_fast_vline(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t h, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_DRAW_FAST_VLINE,
        .a.draw_fast_vline = { .target = target, .x = x, .y = y, .h = h, .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_draw_fast_hline(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_DRAW_FAST_HLINE,
        .a.draw_fast_hline = { .target = target, .x = x, .y = y, .w = w, .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_draw_line(lgfx_port_t *port, uint8_t target, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_DRAW_LINE,
        .a.draw_line = { .target = target, .x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_draw_rect(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_DRAW_RECT,
        .a.draw_rect = { .target = target, .x = x, .y = y, .w = w, .h = h, .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_fill_rect(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_FILL_RECT,
        .a.fill_rect = { .target = target, .x = x, .y = y, .w = w, .h = h, .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_draw_circle(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_DRAW_CIRCLE,
        .a.draw_circle = { .target = target, .x = x, .y = y, .r = r, .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_fill_circle(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, uint16_t r, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_FILL_CIRCLE,
        .a.fill_circle = { .target = target, .x = x, .y = y, .r = r, .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_draw_triangle(
    lgfx_port_t *port, uint8_t target,
    int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_DRAW_TRIANGLE,
        .a.draw_triangle = {
            .target = target,
            .x0 = x0,
            .y0 = y0,
            .x1 = x1,
            .y1 = y1,
            .x2 = x2,
            .y2 = y2,
            .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_fill_triangle(
    lgfx_port_t *port, uint8_t target,
    int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_FILL_TRIANGLE,
        .a.fill_triangle = {
            .target = target,
            .x0 = x0,
            .y0 = y0,
            .x1 = x1,
            .y1 = y1,
            .x2 = x2,
            .y2 = y2,
            .color565 = color565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_set_text_size(lgfx_port_t *port, uint8_t target, uint8_t size)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_SET_TEXT_SIZE,
        .a.set_text_size = { .target = target, .size = size }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_set_text_size_xy(lgfx_port_t *port, uint8_t target, uint8_t sx, uint8_t sy)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_SET_TEXT_SIZE_XY,
        .a.set_text_size_xy = { .target = target, .sx = sx, .sy = sy }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_set_text_datum(lgfx_port_t *port, uint8_t target, uint8_t datum)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_SET_TEXT_DATUM,
        .a.set_text_datum = { .target = target, .datum = datum }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_set_text_wrap(lgfx_port_t *port, uint8_t target, bool wrap)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_SET_TEXT_WRAP,
        .a.set_text_wrap = { .target = target, .wrap = wrap }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_set_text_wrap_xy(lgfx_port_t *port, uint8_t target, bool wrap_x, bool wrap_y)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_SET_TEXT_WRAP_XY,
        .a.set_text_wrap_xy = { .target = target, .wrap_x = wrap_x, .wrap_y = wrap_y }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_set_text_font(lgfx_port_t *port, uint8_t target, uint8_t font)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_SET_TEXT_FONT,
        .a.set_text_font = { .target = target, .font = font }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_set_font_preset(lgfx_port_t *port, uint8_t target, uint8_t preset)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_SET_FONT_PRESET,
        .a.set_font_preset = { .target = target, .preset = preset }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_set_text_color(lgfx_port_t *port, uint8_t target, uint16_t fg565, bool has_bg, uint16_t bg565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_SET_TEXT_COLOR,
        .a.set_text_color = { .target = target, .fg565 = fg565, .has_bg = has_bg, .bg565 = bg565 }
    };
    return lgfx_worker_call(port, &job);
}

/*
 * Owned-payload contract (drawString):
 * - bytes may point into caller-owned memory (including Erlang binary memory)
 * - This wrapper deep-copies len bytes into job-owned heap memory before enqueueing
 * - The worker executes the device call using the copied buffer and frees it
 *   before notifying the caller
 * - After lgfx_worker_copy_payload() succeeds, the original caller buffer is no
 *   longer needed by the worker path
 */
esp_err_t lgfx_worker_device_draw_string(lgfx_port_t *port, uint8_t target, int16_t x, int16_t y, const uint8_t *bytes, uint16_t len)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_DRAW_STRING,
        .a.draw_string = { .target = target, .x = x, .y = y, .bytes = bytes, .len = len }
    };

    esp_err_t err = lgfx_worker_copy_payload(&job, bytes, (size_t) len);
    if (err != ESP_OK) {
        return err;
    }

    if (job.owned_payload) {
        job.a.draw_string.bytes = job.owned_payload;
    }

    return lgfx_worker_call(port, &job);
}

/*
 * Owned-payload contract (pushImage RGB565 strided):
 * - bytes may point into caller-owned memory (including Erlang binary memory)
 * - This wrapper deep-copies len bytes into job-owned heap memory before enqueueing
 * - The worker executes the device call using the copied buffer and frees it
 *   before notifying the caller
 *
 * Backend/DMA note:
 * - Current cleanup assumes the device path fully consumes the payload before
 *   the device call returns to the worker
 * - If a future backend starts DMA/asynchronous transfer that outlives the
 *   device call, move this path to an explicit DMA-safe ownership model and
 *   free only after the DMA completion barrier
 */
esp_err_t lgfx_worker_device_push_image_rgb565_strided(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    uint16_t stride_pixels,
    const uint8_t *bytes,
    size_t len)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_PUSH_IMAGE_RGB565_STRIDED,
        .a.push_image = {
            .target = target,
            .x = x,
            .y = y,
            .w = w,
            .h = h,
            .stride_pixels = stride_pixels,
            .bytes = bytes,
            .len = len }
    };

    esp_err_t err = lgfx_worker_copy_payload(&job, bytes, len);
    if (err != ESP_OK) {
        return err;
    }

    if (job.owned_payload) {
        job.a.push_image.bytes = job.owned_payload;
    }

    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_create_sprite(
    lgfx_port_t *port,
    uint8_t target,
    uint16_t w,
    uint16_t h,
    uint8_t color_depth)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_CREATE_SPRITE,
        .a.create_sprite = {
            .target = target,
            .w = w,
            .h = h,
            .color_depth = color_depth }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_delete_sprite(lgfx_port_t *port, uint8_t target)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_DELETE_SPRITE,
        .a.delete_sprite = { .target = target }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_set_pivot(
    lgfx_port_t *port,
    uint8_t target,
    int16_t x,
    int16_t y)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_SET_PIVOT,
        .a.set_pivot = {
            .target = target,
            .x = x,
            .y = y }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_push_sprite(
    lgfx_port_t *port,
    uint8_t src_target,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint16_t transparent565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_PUSH_SPRITE,
        .a.push_sprite = {
            .src_target = src_target,
            .dst_target = dst_target,
            .x = x,
            .y = y,
            .has_transparent = has_transparent,
            .transparent565 = transparent565 }
    };
    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_push_sprite_region(
    lgfx_port_t *port,
    uint8_t src_target,
    uint8_t dst_target,
    int16_t dst_x,
    int16_t dst_y,
    int16_t src_x,
    int16_t src_y,
    uint16_t w,
    uint16_t h,
    bool has_transparent,
    uint16_t transparent565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_PUSH_SPRITE_REGION,
        .a.push_sprite_region = {
            .src_target = src_target,
            .dst_target = dst_target,
            .dst_x = dst_x,
            .dst_y = dst_y,
            .src_x = src_x,
            .src_y = src_y,
            .w = w,
            .h = h,
            .has_transparent = has_transparent,
            .transparent565 = transparent565 }
    };
    return lgfx_worker_call(port, &job);
}

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
    uint16_t transparent565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_PUSH_ROTATE_ZOOM,
        .a.push_rotate_zoom = {
            .src_target = src_target,
            .dst_target = dst_target,
            .x = x,
            .y = y,
            .angle_deg = angle_deg,
            .zoom_x = zoom_x,
            .zoom_y = zoom_y,
            .has_transparent = has_transparent,
            .transparent565 = transparent565 }
    };
    return lgfx_worker_call(port, &job);
}
