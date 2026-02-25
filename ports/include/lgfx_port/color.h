// ports/include/lgfx_port/color.h
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "term.h"

#include "lgfx_port/term_conv.h"
#include "lgfx_port/validate.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Color policy (hybrid strategy)
 * ------------------------------
 * - Scalar color arguments in the protocol use RGB888 packed as 0xRRGGBB (no alpha)
 * - Pixel payload binaries (e.g. pushImage) remain RGB565 bytes
 */

/*
 * Convert packed RGB888 (0xRRGGBB) to RGB565.
 */
static inline uint16_t lgfx_color888_to_rgb565(uint32_t color888)
{
    const uint8_t r = (uint8_t) ((color888 >> 16) & 0xFFu);
    const uint8_t g = (uint8_t) ((color888 >> 8) & 0xFFu);
    const uint8_t b = (uint8_t) (color888 & 0xFFu);

    return (uint16_t) (((uint16_t) (r & 0xF8u) << 8)
                     | ((uint16_t) (g & 0xFCu) << 3)
                     | ((uint16_t) (b >> 3)));
}

/*
 * Decode a protocol scalar color term and convert to RGB565.
 *
 * Accepts:
 *   - integer term representing RGB888 packed as 0xRRGGBB
 *
 * Returns false on decode/validation failure.
 */
static inline bool lgfx_term_to_color565(term color_t, uint16_t *out_color565)
{
    uint32_t color888 = 0;

    if (!lgfx_term_to_u32(color_t, &color888)) {
        return false;
    }
    if (!lgfx_validate_color888(color888)) {
        return false;
    }

    *out_color565 = lgfx_color888_to_rgb565(color888);
    return true;
}

#ifdef __cplusplus
}
#endif
