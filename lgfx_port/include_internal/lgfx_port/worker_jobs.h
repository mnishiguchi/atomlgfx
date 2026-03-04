// include/lgfx_port/worker_jobs.h
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

// This header is intentionally FreeRTOS-free and AtomVM-free.
// It is used internally by worker compilation units.

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    LGFX_JOB__INVALID = 0,
#define X(kind, member, fields, exec_body) LGFX_JOB_##kind,
#include "lgfx_port/worker_jobs.def"
#undef X
    LGFX_JOB__COUNT
} lgfx_job_kind_t;

typedef struct
{
    lgfx_job_kind_t kind;

    // Completion (written by worker, read by caller).
    // Kept FreeRTOS-free here: TaskHandle_t is pointer-like.
    void *notify_task;
    esp_err_t err;

    // Optional heap-owned payload for variable-length byte buffers.
    // Worker frees this after executing the job (before notify).
    uint8_t *owned_payload;

    union
    {
#define X(kind, member, fields, exec_body) \
    struct                                 \
    {                                      \
        fields                             \
    } member;
#include "lgfx_port/worker_jobs.def"
#undef X
    } a;
} lgfx_job_t;

#ifdef __cplusplus
}
#endif
