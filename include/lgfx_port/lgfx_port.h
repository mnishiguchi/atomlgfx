// include/lgfx_port/lgfx_port.h
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lgfx_port_t lgfx_port_t;

// -----------------------------------------------------------------------------
// Protocol-level constants (stable wire values)
// -----------------------------------------------------------------------------
//
// Font preset IDs used by setFontPreset/2.
// - ASCII is always available.
// - JP presets may be compiled out depending on LGFX_PORT_ENABLE_JP_FONTS.
//
typedef enum
{
    LGFX_FONT_PRESET_ASCII = 0,
    LGFX_FONT_PRESET_JP_SMALL = 1,
    LGFX_FONT_PRESET_JP_MEDIUM = 2,
    LGFX_FONT_PRESET_JP_LARGE = 3,
} lgfx_font_preset_t;

// Small, AtomVM-free API (optional but useful)
uint32_t lgfx_port_feature_bits(const lgfx_port_t *port);
uint8_t lgfx_port_max_sprites(const lgfx_port_t *port);

#ifdef __cplusplus
}
#endif
