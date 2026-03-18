/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
// - Optional Japanese-capable preset may return "not supported" when Japanese
//   fonts are compiled out in the current build.
//
typedef enum
{
    LGFX_FONT_PRESET_ASCII = 0,
    LGFX_FONT_PRESET_JP = 1,
} lgfx_font_preset_t;

// Text scale encoding used by setTextSize on the wire.
//
// Wire encoding is x256 fixed-point:
// - 256 => 1.0x
//
// This constant is protocol-visible and should remain stable.
#define LGFX_TEXT_SCALE_ONE_X256 ((uint16_t) 256u)

#ifdef __cplusplus
}
#endif
