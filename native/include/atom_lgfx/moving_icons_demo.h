/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Icon images are 32x32 RGB565 byte strings owned by the Elixir caller. */
esp_err_t lgfx_moving_icons_start(const uint8_t *info, size_t info_len,
                                  const uint8_t *alert, size_t alert_len,
                                  const uint8_t *close_icon, size_t close_len,
                                  uint8_t object_count);
esp_err_t lgfx_moving_icons_stop(void);
esp_err_t lgfx_moving_icons_stats(uint32_t *frames, uint32_t *elapsed_ms,
                                  uint32_t *last_frame_us);

#ifdef __cplusplus
}
#endif
