// lgfx_port/lgfx_worker_core.c
//
// Concurrency model
// -----------------
// - Port thread (AtomVM native handler):
//   - Drains the AtomVM mailbox (see lgfx_worker_mailbox.c)
//   - Decodes terms
//   - Builds plain C arguments
//   - Calls lgfx_worker_device_* wrappers synchronously
// - Worker task (FreeRTOS task):
//   - Executes only lgfx_device_* calls with plain C arguments
//   - Does not touch Context*, terms, or mailbox ownership
//
// Variable-length payloads (e.g. draw_string / push_image) are deep-copied by
// wrappers into job-owned memory before enqueueing. The worker frees that memory
// after the device call returns and before notifying the caller.

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_err.h"

#include "lgfx_device.h"
#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/worker.h"

#include "worker_internal.h"

#ifndef LGFX_WORKER_QUEUE_LEN
#define LGFX_WORKER_QUEUE_LEN 32
#endif

#ifndef LGFX_WORKER_TASK_STACK_WORDS
#define LGFX_WORKER_TASK_STACK_WORDS 4096
#endif

#ifndef LGFX_WORKER_TASK_PRIORITY
#define LGFX_WORKER_TASK_PRIORITY 5
#endif

#ifndef LGFX_WORKER_TASK_NAME
#define LGFX_WORKER_TASK_NAME "lgfx_worker"
#endif

typedef struct
{
    QueueHandle_t queue;
    TaskHandle_t task;

    TaskHandle_t stop_notify_task;
    volatile bool stopping;

    lgfx_port_t *port;
} lgfx_worker_t;

static void lgfx_worker_task_main(void *arg);

static void lgfx_worker_cleanup_job_payload(lgfx_job_t *job)
{
    if (!job) {
        return;
    }

    if (job->owned_payload) {
        free(job->owned_payload);
        job->owned_payload = NULL;
    }
}

// Queue a job pointer from the port thread and block until the worker completes.
esp_err_t lgfx_worker_call(lgfx_port_t *port, lgfx_job_t *job)
{
    if (!port || !job || !port->worker) {
        lgfx_worker_cleanup_job_payload(job);
        return ESP_ERR_INVALID_STATE;
    }

    lgfx_worker_t *w = (lgfx_worker_t *) port->worker;
    if (!w || !w->queue || w->stopping) {
        lgfx_worker_cleanup_job_payload(job);
        return ESP_ERR_INVALID_STATE;
    }

    // worker_internal.h keeps this FreeRTOS-free; TaskHandle_t is pointer-like.
    job->notify_task = (void *) xTaskGetCurrentTaskHandle();
    job->err = ESP_FAIL;

    if (!job->notify_task) {
        lgfx_worker_cleanup_job_payload(job);
        return ESP_ERR_INVALID_STATE;
    }

    lgfx_job_t *p = job;
    if (xQueueSend(w->queue, &p, portMAX_DELAY) != pdTRUE) {
        lgfx_worker_cleanup_job_payload(job);
        return ESP_FAIL;
    }

    /*
     * IMPORTANT:
     * Jobs are currently stack-allocated in lgfx_worker_device_* wrappers and
     * enqueued by pointer. A timeout here would allow the wrapper to return and
     * the worker to later dereference a dead stack frame (UAF).
     *
     * Payload-bearing jobs may also carry job-owned heap buffers
     * (job->owned_payload) that are freed by the worker after the device call
     * and before notify. Blocking here preserves simple wrapper semantics and a
     * clear completion boundary.
     *
     * If timeouts are needed later, switch to heap/pool-owned jobs or
     * queue-by-value and define explicit lifetime rules for both the job object
     * and any attached payload buffers.
     */
    (void) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    return job->err;
}

bool lgfx_worker_start(lgfx_port_t *port)
{
    if (!port || !port->ctx) {
        return false;
    }
    if (port->worker) {
        return true;
    }

    lgfx_worker_t *w = (lgfx_worker_t *) calloc(1, sizeof(lgfx_worker_t));
    if (!w) {
        return false;
    }

    w->port = port;
    w->queue = xQueueCreate(LGFX_WORKER_QUEUE_LEN, sizeof(lgfx_job_t *));
    if (!w->queue) {
        free(w);
        return false;
    }

    // Publish worker state before starting the task to avoid races.
    port->worker = (void *) w;

    BaseType_t ok = xTaskCreate(
        lgfx_worker_task_main,
        LGFX_WORKER_TASK_NAME,
        LGFX_WORKER_TASK_STACK_WORDS,
        (void *) w,
        LGFX_WORKER_TASK_PRIORITY,
        &w->task);

    if (ok != pdPASS) {
        port->worker = NULL;
        vQueueDelete(w->queue);
        free(w);
        return false;
    }

    return true;
}

void lgfx_worker_stop(lgfx_port_t *port)
{
    if (!port || !port->worker) {
        return;
    }

    lgfx_worker_t *w = (lgfx_worker_t *) port->worker;
    if (!w || !w->queue || w->stopping) {
        return;
    }

    w->stopping = true;

    // Prepare join notification before sending the stop sentinel to avoid races.
    w->stop_notify_task = xTaskGetCurrentTaskHandle();

    // Stop sentinel: NULL job pointer.
    lgfx_job_t *stop = NULL;
    (void) xQueueSend(w->queue, &stop, portMAX_DELAY);

    // Wait for worker task shutdown acknowledgement.
    (void) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Worker task exits via vTaskDelete(NULL) after sending the ack.
    vQueueDelete(w->queue);
    port->worker = NULL;
    free(w);
}

// -----------------------------------------------------------------------------
// Worker task: executes only device calls with plain C arguments.
// -----------------------------------------------------------------------------

static void lgfx_worker_task_main(void *arg)
{
    lgfx_worker_t *w = (lgfx_worker_t *) arg;
    lgfx_port_t *port = w ? w->port : NULL;

    if (!w || !port || !w->queue) {
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        lgfx_job_t *job = NULL;
        if (xQueueReceive(w->queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (!job) {
            break; // stop sentinel
        }

        switch (job->kind) {
            case LGFX_JOB_INIT:
                job->err = lgfx_device_init();
                break;

            case LGFX_JOB_CLOSE:
                job->err = lgfx_device_close();
                break;

            case LGFX_JOB_GET_DIMS:
                job->err = lgfx_device_get_dims(&job->a.get_dims.w, &job->a.get_dims.h);
                break;

            case LGFX_JOB_SET_ROTATION:
                job->err = lgfx_device_set_rotation(job->a.set_rotation.rot);
                break;

            case LGFX_JOB_SET_BRIGHTNESS:
                job->err = lgfx_device_set_brightness(job->a.set_brightness.b);
                break;

            case LGFX_JOB_SET_COLOR_DEPTH:
                job->err = lgfx_device_set_color_depth(
                    job->a.set_color_depth.target,
                    job->a.set_color_depth.depth);
                break;

            case LGFX_JOB_DISPLAY:
                job->err = lgfx_device_display();
                break;

            case LGFX_JOB_GET_TOUCH:
                job->err = lgfx_device_get_touch(
                    &job->a.get_touch.touched,
                    &job->a.get_touch.x,
                    &job->a.get_touch.y,
                    &job->a.get_touch.size);
                break;

            case LGFX_JOB_GET_TOUCH_RAW:
                job->err = lgfx_device_get_touch_raw(
                    &job->a.get_touch_raw.touched,
                    &job->a.get_touch_raw.x,
                    &job->a.get_touch_raw.y,
                    &job->a.get_touch_raw.size);
                break;

            case LGFX_JOB_SET_TOUCH_CALIBRATE:
                job->err = lgfx_device_set_touch_calibrate(job->a.set_touch_calibrate.params);
                break;

            case LGFX_JOB_CALIBRATE_TOUCH:
                job->err = lgfx_device_calibrate_touch(job->a.calibrate_touch.params);
                break;

            case LGFX_JOB_FILL_SCREEN:
                job->err = lgfx_device_fill_screen(
                    job->a.fill_screen.target,
                    job->a.fill_screen.color565);
                break;

            case LGFX_JOB_CLEAR:
                job->err = lgfx_device_clear(
                    job->a.clear.target,
                    job->a.clear.color565);
                break;

            case LGFX_JOB_DRAW_PIXEL:
                job->err = lgfx_device_draw_pixel(
                    job->a.draw_pixel.target,
                    job->a.draw_pixel.x,
                    job->a.draw_pixel.y,
                    job->a.draw_pixel.color565);
                break;

            case LGFX_JOB_DRAW_FAST_VLINE:
                job->err = lgfx_device_draw_fast_vline(
                    job->a.draw_fast_vline.target,
                    job->a.draw_fast_vline.x,
                    job->a.draw_fast_vline.y,
                    job->a.draw_fast_vline.h,
                    job->a.draw_fast_vline.color565);
                break;

            case LGFX_JOB_DRAW_FAST_HLINE:
                job->err = lgfx_device_draw_fast_hline(
                    job->a.draw_fast_hline.target,
                    job->a.draw_fast_hline.x,
                    job->a.draw_fast_hline.y,
                    job->a.draw_fast_hline.w,
                    job->a.draw_fast_hline.color565);
                break;

            case LGFX_JOB_DRAW_LINE:
                job->err = lgfx_device_draw_line(
                    job->a.draw_line.target,
                    job->a.draw_line.x0,
                    job->a.draw_line.y0,
                    job->a.draw_line.x1,
                    job->a.draw_line.y1,
                    job->a.draw_line.color565);
                break;

            case LGFX_JOB_DRAW_RECT:
                job->err = lgfx_device_draw_rect(
                    job->a.draw_rect.target,
                    job->a.draw_rect.x,
                    job->a.draw_rect.y,
                    job->a.draw_rect.w,
                    job->a.draw_rect.h,
                    job->a.draw_rect.color565);
                break;

            case LGFX_JOB_FILL_RECT:
                job->err = lgfx_device_fill_rect(
                    job->a.fill_rect.target,
                    job->a.fill_rect.x,
                    job->a.fill_rect.y,
                    job->a.fill_rect.w,
                    job->a.fill_rect.h,
                    job->a.fill_rect.color565);
                break;

            case LGFX_JOB_DRAW_CIRCLE:
                job->err = lgfx_device_draw_circle(
                    job->a.draw_circle.target,
                    job->a.draw_circle.x,
                    job->a.draw_circle.y,
                    job->a.draw_circle.r,
                    job->a.draw_circle.color565);
                break;

            case LGFX_JOB_FILL_CIRCLE:
                job->err = lgfx_device_fill_circle(
                    job->a.fill_circle.target,
                    job->a.fill_circle.x,
                    job->a.fill_circle.y,
                    job->a.fill_circle.r,
                    job->a.fill_circle.color565);
                break;

            case LGFX_JOB_DRAW_TRIANGLE:
                job->err = lgfx_device_draw_triangle(
                    job->a.draw_triangle.target,
                    job->a.draw_triangle.x0, job->a.draw_triangle.y0,
                    job->a.draw_triangle.x1, job->a.draw_triangle.y1,
                    job->a.draw_triangle.x2, job->a.draw_triangle.y2,
                    job->a.draw_triangle.color565);
                break;

            case LGFX_JOB_FILL_TRIANGLE:
                job->err = lgfx_device_fill_triangle(
                    job->a.fill_triangle.target,
                    job->a.fill_triangle.x0, job->a.fill_triangle.y0,
                    job->a.fill_triangle.x1, job->a.fill_triangle.y1,
                    job->a.fill_triangle.x2, job->a.fill_triangle.y2,
                    job->a.fill_triangle.color565);
                break;

            case LGFX_JOB_SET_TEXT_SIZE:
                job->err = lgfx_device_set_text_size(
                    job->a.set_text_size.target,
                    job->a.set_text_size.size);
                break;

            case LGFX_JOB_SET_TEXT_DATUM:
                job->err = lgfx_device_set_text_datum(
                    job->a.set_text_datum.target,
                    job->a.set_text_datum.datum);
                break;

            case LGFX_JOB_SET_TEXT_WRAP:
                // Protocol currently carries one wrap flag; apply it to both axes.
                job->err = lgfx_device_set_text_wrap(
                    job->a.set_text_wrap.target,
                    job->a.set_text_wrap.wrap,
                    job->a.set_text_wrap.wrap);
                break;

            case LGFX_JOB_SET_TEXT_FONT:
                job->err = lgfx_device_set_text_font(
                    job->a.set_text_font.target,
                    job->a.set_text_font.font);
                break;

            case LGFX_JOB_SET_FONT_PRESET:
                job->err = lgfx_device_set_font_preset(
                    job->a.set_font_preset.target,
                    job->a.set_font_preset.preset);
                break;

            case LGFX_JOB_SET_TEXT_COLOR:
                job->err = lgfx_device_set_text_color(
                    job->a.set_text_color.target,
                    job->a.set_text_color.fg565,
                    job->a.set_text_color.has_bg,
                    job->a.set_text_color.bg565);
                break;

            case LGFX_JOB_DRAW_STRING:
                job->err = lgfx_device_draw_string(
                    job->a.draw_string.target,
                    job->a.draw_string.x,
                    job->a.draw_string.y,
                    job->a.draw_string.bytes,
                    job->a.draw_string.len);
                break;

            case LGFX_JOB_PUSH_IMAGE_RGB565_STRIDED:
                job->err = lgfx_device_push_image_rgb565_strided(
                    job->a.push_image.target,
                    job->a.push_image.x,
                    job->a.push_image.y,
                    job->a.push_image.w,
                    job->a.push_image.h,
                    job->a.push_image.stride_pixels,
                    job->a.push_image.bytes,
                    job->a.push_image.len);
                break;

            case LGFX_JOB_CREATE_SPRITE:
                job->err = lgfx_device_sprite_create_at(
                    job->a.create_sprite.target,
                    job->a.create_sprite.w,
                    job->a.create_sprite.h,
                    job->a.create_sprite.color_depth);
                break;

            case LGFX_JOB_DELETE_SPRITE:
                job->err = lgfx_device_sprite_delete(
                    job->a.delete_sprite.target);
                break;

            case LGFX_JOB_SET_PIVOT:
                job->err = lgfx_device_sprite_set_pivot(
                    job->a.set_pivot.target,
                    job->a.set_pivot.x,
                    job->a.set_pivot.y);
                break;

            case LGFX_JOB_PUSH_SPRITE:
                job->err = lgfx_device_sprite_push_sprite(
                    job->a.push_sprite.target,
                    job->a.push_sprite.x,
                    job->a.push_sprite.y,
                    job->a.push_sprite.has_transparent,
                    job->a.push_sprite.transparent565);
                break;

            case LGFX_JOB_PUSH_SPRITE_REGION:
                job->err = lgfx_device_sprite_push_sprite_region(
                    job->a.push_sprite_region.target,
                    job->a.push_sprite_region.dst_x,
                    job->a.push_sprite_region.dst_y,
                    job->a.push_sprite_region.src_x,
                    job->a.push_sprite_region.src_y,
                    job->a.push_sprite_region.w,
                    job->a.push_sprite_region.h,
                    job->a.push_sprite_region.has_transparent,
                    job->a.push_sprite_region.transparent565,
                    NULL);
                break;

            case LGFX_JOB_PUSH_ROTATE_ZOOM: {
                const float angle_deg = job->a.push_rotate_zoom.angle_deg;
                const float zoom_x = job->a.push_rotate_zoom.zoom_x;
                const float zoom_y = job->a.push_rotate_zoom.zoom_y;

                // Basic worker-side validation. Device layer will also validate and perform
                // backend compatibility dispatch.
                if (!isfinite(angle_deg) || !isfinite(zoom_x) || !isfinite(zoom_y)) {
                    job->err = ESP_ERR_INVALID_ARG;
                    break;
                }

                if (zoom_x <= 0.0f || zoom_y <= 0.0f) {
                    job->err = ESP_ERR_INVALID_ARG;
                    break;
                }

                job->err = lgfx_device_sprite_push_rotate_zoom(
                    job->a.push_rotate_zoom.src_target,
                    job->a.push_rotate_zoom.dst_target,
                    job->a.push_rotate_zoom.x,
                    job->a.push_rotate_zoom.y,
                    angle_deg,
                    zoom_x,
                    zoom_y,
                    job->a.push_rotate_zoom.has_transparent,
                    job->a.push_rotate_zoom.transparent565);
                break;
            }

            default:
                job->err = ESP_ERR_INVALID_ARG;
                break;
        }

        /*
         * Free wrapper-owned payload bytes after the device call returns and
         * before notifying the caller.
         *
         * If a future backend introduces async DMA that outlives the device
         * call, move payload release to the DMA completion barrier instead.
         */
        lgfx_worker_cleanup_job_payload(job);

        if (job->notify_task) {
            xTaskNotifyGive((TaskHandle_t) job->notify_task);
        }
    }

    if (w->stop_notify_task) {
        xTaskNotifyGive(w->stop_notify_task);
    }

    vTaskDelete(NULL);
}
