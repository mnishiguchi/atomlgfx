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

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "lgfx_device.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/worker.h"
#include "lgfx_port/worker_internal.h"
#include "lgfx_port/worker_jobs.h"

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
static esp_err_t lgfx_worker_exec_push_rotate_zoom(lgfx_job_t *job);

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

static esp_err_t lgfx_worker_exec_push_rotate_zoom(lgfx_job_t *job)
{
    if (!job) {
        return ESP_ERR_INVALID_ARG;
    }

    const float angle_deg = job->a.push_rotate_zoom.angle_deg;
    const float zoom_x = job->a.push_rotate_zoom.zoom_x;
    const float zoom_y = job->a.push_rotate_zoom.zoom_y;

    // Basic worker-side validation. Device layer will also validate and perform
    // backend compatibility dispatch.
    if (!isfinite(angle_deg) || !isfinite(zoom_x) || !isfinite(zoom_y)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (zoom_x <= 0.0f || zoom_y <= 0.0f) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_device_sprite_push_rotate_zoom(
        job->a.push_rotate_zoom.src_target,
        job->a.push_rotate_zoom.dst_target,
        job->a.push_rotate_zoom.x,
        job->a.push_rotate_zoom.y,
        angle_deg,
        zoom_x,
        zoom_y,
        job->a.push_rotate_zoom.has_transparent,
        job->a.push_rotate_zoom.transparent565);
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

    // worker_jobs.h keeps this FreeRTOS-free; TaskHandle_t is pointer-like.
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

        // The boring rule: worker_jobs.def owns the job->kind dispatch body.
        switch (job->kind) {
            case LGFX_JOB__INVALID:
                job->err = ESP_ERR_INVALID_ARG;
                break;

#define X(kind, member, fields, exec_body) \
    case LGFX_JOB_##kind:                  \
        exec_body;                         \
        break;
#include "lgfx_port/worker_jobs.def"
#undef X

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
