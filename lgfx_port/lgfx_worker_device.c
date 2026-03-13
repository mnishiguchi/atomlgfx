// lgfx_port/lgfx_worker_device.c
// Port-thread worker wrappers.
// Ownership and completion rules: see docs/WORKER_MODEL.md.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"

#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/worker.h"
#include "lgfx_port/worker_internal.h"
#include "lgfx_port/worker_jobs.h"

#define LGFX_WORKER_CALL_ARGS(job_kind, member, ...) \
    do {                                             \
        lgfx_job_t job = {                           \
            .kind = LGFX_JOB_##job_kind,             \
            .a.member = { __VA_ARGS__ }              \
        };                                           \
        return lgfx_worker_call(port, &job);         \
    } while (0)

/*
 * Most wrappers here are simple pass-throughs:
 * - build a fixed-size job
 * - call the worker synchronously
 *
 * Keep exceptional cases hand-written:
 * - init/close use owner-token + persisted open-config snapshot
 * - getters copy outputs back to caller pointers
 * - variable-length payload calls deep-copy before enqueueing
 */
#define LGFX_WORKER_DEFINE_SIMPLE_WRAPPER(fn_name, params, job_kind, member, ...) \
    esp_err_t lgfx_worker_device_##fn_name params                                 \
    {                                                                             \
        LGFX_WORKER_CALL_ARGS(job_kind, member, __VA_ARGS__);                     \
    }

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
    if (!port) {
        return ESP_ERR_INVALID_STATE;
    }

    lgfx_job_t job = {
        .kind = LGFX_JOB_INIT,
        .a.init = {
            .owner_token = (const void *) port,
            .open_config_overrides = port->open_config_overrides }
    };

    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_close(lgfx_port_t *port)
{
    if (!port) {
        return ESP_ERR_INVALID_STATE;
    }

    lgfx_job_t job = {
        .kind = LGFX_JOB_CLOSE,
        .a.close = {
            .owner_token = (const void *) port }
    };

    return lgfx_worker_call(port, &job);
}

esp_err_t lgfx_worker_device_get_dims(lgfx_port_t *port, uint16_t *out_w, uint16_t *out_h)
{
    if (!port || !out_w || !out_h) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_job_t job = {
        .kind = LGFX_JOB_GET_DIMS,
        .a.get_dims = {
            .owner_token = (const void *) port,
            .open_config_overrides = port->open_config_overrides,
            .w = 0,
            .h = 0 }
    };

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

LGFX_WORKER_SIMPLE_DEVICE_WRAPPERS(LGFX_WORKER_DEFINE_SIMPLE_WRAPPER)

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

/*
 * Owned-payload contract (draw_string):
 * - bytes may point into caller-owned memory
 * - this wrapper deep-copies before enqueueing
 * - the worker frees the copied payload after the device call and before notify
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
 * Owned-payload contract (push_image):
 * - bytes may point into caller-owned memory
 * - this wrapper deep-copies before enqueueing
 * - the worker frees the copied payload after the device call and before notify
 *
 * Current cleanup assumes the device path fully consumes the payload before the
 * device call returns. If a future backend introduces async DMA, move this path
 * to an explicit DMA-safe ownership model.
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

#undef LGFX_WORKER_DEFINE_SIMPLE_WRAPPER
#undef LGFX_WORKER_CALL_ARGS
