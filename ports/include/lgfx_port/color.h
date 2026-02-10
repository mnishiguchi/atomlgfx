#pragma once
#include <stdint.h>

static inline uint16_t lgfx_color888_to_rgb565(uint32_t c)
{
    // c is 0x00RRGGBB
    uint8_t r = (uint8_t) ((c >> 16) & 0xFF);
    uint8_t g = (uint8_t) ((c >> 8) & 0xFF);
    uint8_t b = (uint8_t) (c & 0xFF);

    return (uint16_t) (((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3));
}
