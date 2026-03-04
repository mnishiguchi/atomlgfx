// lgfx_port/worker_internal.h
#pragma once

#include "esp_err.h"
#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/worker_jobs.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t lgfx_worker_call(lgfx_port_t *port, lgfx_job_t *job);

#ifdef __cplusplus
}
#endif
