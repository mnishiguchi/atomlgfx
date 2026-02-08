#ifndef LGFX_DEVICE_H
#define LGFX_DEVICE_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t lgfx_device_init(uint8_t rotation);
esp_err_t lgfx_device_fill_screen(uint16_t rgb565);

esp_err_t lgfx_device_draw_text(
    int16_t x,
    int16_t y,
    uint16_t rgb565,
    uint8_t text_size,
    const uint8_t *text_bytes,
    uint16_t text_len);

#ifdef __cplusplus
}
#endif

#endif
