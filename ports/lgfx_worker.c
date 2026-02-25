// ports/lgfx_worker.c
//
// Concurrency model
// -----------------
// - Port thread (AtomVM native handler):
//   - Drains the AtomVM mailbox
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
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_err.h"

#include "lgfx_device.h"
#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/worker.h"

// AtomVM headers are used only by the port-thread mailbox drain path.
// The worker task itself remains term-free.
#include "context.h"
#include "mailbox.h"
#include "term.h"

// Port-thread callback implemented in ports/lgfx_port.c.
extern void lgfx_port_handle_mailbox_message(Context *ctx, lgfx_port_t *port, term msg);

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
    TaskHandle_t notify_task;
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
            uint8_t target;
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

/*
 * Deep-copy helper for variable-length payloads:
 * - Copies caller-provided bytes into job-owned heap memory
 * - Worker frees job->owned_payload after the device call completes
 * - len == 0 is allowed and performs no allocation
 */
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

// Queue a job pointer from the port thread and block until the worker completes.
static esp_err_t lgfx_worker_call(lgfx_port_t *port, lgfx_job_t *job)
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

    job->notify_task = xTaskGetCurrentTaskHandle();
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

/*
 * Drain the AtomVM mailbox on the port thread and forward each message term to
 * the port handler. Mailbox message ownership/disposal stays on the port thread.
 * The worker task never touches msg_term or mailbox objects.
 */
void lgfx_worker_drain_mailbox(lgfx_port_t *port)
{
    if (!port || !port->ctx) {
        return;
    }

    Context *ctx = port->ctx;
    Mailbox *mbox = &ctx->mailbox;
    Heap *heap = &ctx->heap;

    while (true) {
        MailboxMessage *m = mailbox_take_message(mbox);
        if (!m) {
            break;
        }

        if (m->type == NormalMessage) {
            Message *mm = (Message *) m;
            term msg_term = mm->message;

            lgfx_port_handle_mailbox_message(ctx, port, msg_term);
        }

        mailbox_message_dispose(m, heap);
    }
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
                    job->a.push_rotate_zoom.target,
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
            xTaskNotifyGive(job->notify_task);
        }
    }

    if (w->stop_notify_task) {
        xTaskNotifyGive(w->stop_notify_task);
    }

    vTaskDelete(NULL);
}

// -----------------------------------------------------------------------------
// Public device wrappers (called from handlers on the port thread)
// -----------------------------------------------------------------------------

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
    uint8_t target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint16_t transparent565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_PUSH_SPRITE,
        .a.push_sprite = {
            .target = target,
            .x = x,
            .y = y,
            .has_transparent = has_transparent,
            .transparent565 = transparent565 }
    };
    return lgfx_worker_call(port, &job);
}

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
    uint16_t transparent565)
{
    lgfx_job_t job = {
        .kind = LGFX_JOB_PUSH_SPRITE_REGION,
        .a.push_sprite_region = {
            .target = target,
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
    uint8_t target,
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
            .target = target,
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
