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
// Text font preset IDs used by setTextFontPreset/2.
//
// These values are part of the protocol contract and should remain stable across
// handler / worker / device layers.
//
// Behavior is defined by the implementation behind the protocol:
// - ASCII is always available.
// - Optional JP presets may return "not supported" when JP fonts are compiled
//   out in the current build.
//
typedef enum
{
    LGFX_FONT_PRESET_ASCII = 0,
    LGFX_FONT_PRESET_JP_SMALL = 1,
    LGFX_FONT_PRESET_JP_MEDIUM = 2,
    LGFX_FONT_PRESET_JP_LARGE = 3,
} lgfx_font_preset_t;

// Text scale values used by setTextSize on the wire.
//
// Wire encoding is x256 fixed-point:
// - 256 => 1.0x
// - 384 => 1.5x
// - 512 => 2.0x
//
// These constants are protocol-visible and should remain stable.
#define LGFX_TEXT_SCALE_ONE_X256 ((uint16_t) 256u)
#define LGFX_TEXT_SCALE_JP_SMALL_X256 LGFX_TEXT_SCALE_ONE_X256
#define LGFX_TEXT_SCALE_JP_MEDIUM_X256 ((uint16_t) 512u)
#define LGFX_TEXT_SCALE_JP_LARGE_X256 ((uint16_t) 768u)

#ifdef __cplusplus
}
#endif
