/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/worker_core.c
// Worker task core.
// Concurrency and ownership model: see docs/WORKER_MODEL.md.

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

esp_err_t lgfx_worker_call(lgfx_port_t *port, lgfx_job_t *job)
{
    if (!port || !job || !port->worker) {
        lgfx_worker_cleanup_job_payload(job);
        return ESP_ERR_INVALID_STATE;
    }

    lgfx_worker_t *worker = (lgfx_worker_t *) port->worker;
    if (!worker || !worker->queue || worker->stopping) {
        lgfx_worker_cleanup_job_payload(job);
        return ESP_ERR_INVALID_STATE;
    }

    job->notify_task = (void *) xTaskGetCurrentTaskHandle();
    job->err = ESP_FAIL;

    if (!job->notify_task) {
        lgfx_worker_cleanup_job_payload(job);
        return ESP_ERR_INVALID_STATE;
    }

    lgfx_job_t *queued_job = job;
    if (xQueueSend(worker->queue, &queued_job, portMAX_DELAY) != pdTRUE) {
        lgfx_worker_cleanup_job_payload(job);
        return ESP_FAIL;
    }

    /*
     * IMPORTANT:
     * Jobs are currently stack-allocated in lgfx_worker_device_* wrappers and
     * enqueued by pointer. A timeout here would let the wrapper return while the
     * worker still holds a pointer into a dead stack frame.
     *
     * Payload-bearing jobs may also carry job-owned heap buffers that the worker
     * frees after the device call and before notify.
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

    lgfx_worker_t *worker = (lgfx_worker_t *) calloc(1, sizeof(lgfx_worker_t));
    if (!worker) {
        return false;
    }

    worker->port = port;
    worker->queue = xQueueCreate(LGFX_WORKER_QUEUE_LEN, sizeof(lgfx_job_t *));
    if (!worker->queue) {
        free(worker);
        return false;
    }

    // Publish worker state before starting the task to avoid races.
    port->worker = (void *) worker;

    BaseType_t ok = xTaskCreate(
        lgfx_worker_task_main,
        LGFX_WORKER_TASK_NAME,
        LGFX_WORKER_TASK_STACK_WORDS,
        (void *) worker,
        LGFX_WORKER_TASK_PRIORITY,
        &worker->task);

    if (ok != pdPASS) {
        port->worker = NULL;
        vQueueDelete(worker->queue);
        free(worker);
        return false;
    }

    return true;
}

void lgfx_worker_stop(lgfx_port_t *port)
{
    if (!port || !port->worker) {
        return;
    }

    lgfx_worker_t *worker = (lgfx_worker_t *) port->worker;
    if (!worker || !worker->queue || worker->stopping) {
        return;
    }

    worker->stopping = true;

    // Set join notification before enqueueing the stop sentinel.
    worker->stop_notify_task = xTaskGetCurrentTaskHandle();

    lgfx_job_t *stop = NULL;
    (void) xQueueSend(worker->queue, &stop, portMAX_DELAY);

    (void) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    vQueueDelete(worker->queue);
    port->worker = NULL;
    free(worker);
}

static void lgfx_worker_task_main(void *arg)
{
    lgfx_worker_t *worker = (lgfx_worker_t *) arg;
    lgfx_port_t *port = worker ? worker->port : NULL;

    if (!worker || !port || !worker->queue) {
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        lgfx_job_t *job = NULL;
        if (xQueueReceive(worker->queue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (!job) {
            break;
        }

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

    if (worker->stop_notify_task) {
        xTaskNotifyGive(worker->stop_notify_task);
    }

    vTaskDelete(NULL);
}
