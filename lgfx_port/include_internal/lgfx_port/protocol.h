// lgfx_port/include_internal/lgfx_port/protocol.h
/*
 * Protocol constants and wire-level limits.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Build-time configuration generated from lgfx_port/cmake/lgfx_port_config.h.in.
#include "lgfx_port/lgfx_port_config.h"

#include "lgfx_port/lgfx_port.h" // LGFX_FONT_PRESET_*, LGFX_TEXT_SCALE_* and other wire constants

// -----------------------------------------------------------------------------
// Optional debug string constants
// -----------------------------------------------------------------------------

#ifndef LGFX_PORT_PROTOCOL_DEBUG_STRINGS
#define LGFX_PORT_PROTOCOL_DEBUG_STRINGS 0
#endif

#if (LGFX_PORT_PROTOCOL_DEBUG_STRINGS != 0) && (LGFX_PORT_PROTOCOL_DEBUG_STRINGS != 1)
#error "LGFX_PORT_PROTOCOL_DEBUG_STRINGS must be 0 or 1"
#endif

#if LGFX_PORT_PROTOCOL_DEBUG_STRINGS
// Error atom strings.
#define LGFX_ERR_BAD_PROTO "bad_proto"
#define LGFX_ERR_BAD_OP "bad_op"
#define LGFX_ERR_BAD_FLAGS "bad_flags"
#define LGFX_ERR_BAD_ARGS "bad_args"
#define LGFX_ERR_BAD_TARGET "bad_target"
#define LGFX_ERR_NOT_INITIALIZED "not_initialized"
#define LGFX_ERR_NOT_WRITING "not_writing"
#define LGFX_ERR_NO_MEMORY "no_memory"
#define LGFX_ERR_INTERNAL "internal"
#define LGFX_ERR_UNSUPPORTED "unsupported"

// Detail reason tags.
#define LGFX_ERR_BATCH_FAILED "batch_failed"
#endif

// -----------------------------------------------------------------------------
// Protocol constants
// -----------------------------------------------------------------------------

#ifndef LGFX_PORT_PROTO_VER
#define LGFX_PORT_PROTO_VER 1u
#endif

#ifndef LGFX_PORT_MAX_BINARY_BYTES
#define LGFX_PORT_MAX_BINARY_BYTES (256u * 1024u)
#endif

#ifndef LGFX_PORT_MAX_SPRITES
#error "LGFX_PORT_MAX_SPRITES must be defined by lgfx_port_config.h"
#endif

#if (LGFX_PORT_MAX_SPRITES > 254u)
#error "LGFX_PORT_MAX_SPRITES must be <= 254"
#endif

#ifndef LGFX_PORT_SPRITE_DEFAULT_DEPTH
#define LGFX_PORT_SPRITE_DEFAULT_DEPTH 16u
#endif

#if (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 1u) && (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 2u) && (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 4u) \
    && (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 8u) && (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 16u) && (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 24u)
#error "LGFX_PORT_SPRITE_DEFAULT_DEPTH must be one of: 1,2,4,8,16,24"
#endif

// -----------------------------------------------------------------------------
// Feature toggles sanity (0/1)
// -----------------------------------------------------------------------------

#ifndef LGFX_PORT_ENABLE_TOUCH
#error "LGFX_PORT_ENABLE_TOUCH must be defined by lgfx_port_config.h"
#endif

#if (LGFX_PORT_ENABLE_TOUCH != 0) && (LGFX_PORT_ENABLE_TOUCH != 1)
#error "LGFX_PORT_ENABLE_TOUCH must be 0 or 1"
#endif

#ifndef LGFX_PORT_ENABLE_JP_FONTS
#error "LGFX_PORT_ENABLE_JP_FONTS must be defined by lgfx_port_config.h"
#endif

#if (LGFX_PORT_ENABLE_JP_FONTS != 0) && (LGFX_PORT_ENABLE_JP_FONTS != 1)
#error "LGFX_PORT_ENABLE_JP_FONTS must be 0 or 1"
#endif

// -----------------------------------------------------------------------------
// Derived advertisement gates (0/1)
// -----------------------------------------------------------------------------

#ifndef LGFX_PORT_SUPPORTS_TOUCH
#error "LGFX_PORT_SUPPORTS_TOUCH must be defined by lgfx_port_config.h"
#endif

#if (LGFX_PORT_SUPPORTS_TOUCH != 0) && (LGFX_PORT_SUPPORTS_TOUCH != 1)
#error "LGFX_PORT_SUPPORTS_TOUCH must be 0 or 1"
#endif

// -----------------------------------------------------------------------------
// Cross-check: advertised support must not exceed compiled support
// -----------------------------------------------------------------------------

#if (LGFX_PORT_SUPPORTS_TOUCH == 1) && (LGFX_PORT_ENABLE_TOUCH != 1)
#error "LGFX_PORT_SUPPORTS_TOUCH=1 requires LGFX_PORT_ENABLE_TOUCH=1"
#endif

// -----------------------------------------------------------------------------
// Validation domains
// -----------------------------------------------------------------------------

static inline bool lgfx_validate_color_depth(uint32_t d)
{
    switch (d) {
        case 1:
        case 2:
        case 4:
        case 8:
        case 16:
        case 24:
            return true;
        default:
            return false;
    }
}

static inline bool lgfx_is_palette_color_depth(uint32_t d)
{
    switch (d) {
        case 1:
        case 2:
        case 4:
        case 8:
            return true;
        default:
            return false;
    }
}

static inline bool lgfx_palette_capacity_for_depth(uint32_t depth, uint16_t *out_capacity)
{
    if (!out_capacity) {
        return false;
    }

    switch (depth) {
        case 1:
            *out_capacity = 2u;
            return true;
        case 2:
            *out_capacity = 4u;
            return true;
        case 4:
            *out_capacity = 16u;
            return true;
        case 8:
            *out_capacity = 256u;
            return true;
        default:
            return false;
    }
}

static inline bool lgfx_validate_palette_index_for_depth(uint32_t depth, uint32_t index)
{
    uint16_t capacity = 0;
    return lgfx_palette_capacity_for_depth(depth, &capacity) && index < capacity;
}

static inline bool lgfx_validate_rgb888(uint32_t rgb888)
{
    return rgb888 <= 0xFFFFFFu;
}

// setTextSize wire values use positive x256 fixed-point integers.
// Examples:
// - 256 => 1.0x
// - 384 => 1.5x
// - 512 => 2.0x
static inline bool lgfx_validate_text_scale_x256(uint32_t scale_x256)
{
    return scale_x256 >= 1u && scale_x256 <= 65535u;
}

// -----------------------------------------------------------------------------
// FeatureBits (wire)
// -----------------------------------------------------------------------------

#define LGFX_CAP_SPRITE (1u << 0)
#define LGFX_CAP_PUSHIMAGE (1u << 1)
#define LGFX_CAP_LAST_ERROR (1u << 2)
#define LGFX_CAP_TOUCH (1u << 3)
#define LGFX_CAP_PALETTE (1u << 4)

#define LGFX_CAP_KNOWN_MASK \
    (LGFX_CAP_SPRITE | LGFX_CAP_PUSHIMAGE | LGFX_CAP_LAST_ERROR | LGFX_CAP_TOUCH | LGFX_CAP_PALETTE)

// -----------------------------------------------------------------------------
// Build / advertised capability mask
// -----------------------------------------------------------------------------

// Touch remains derived; the other current capability bits are always built in.
#define LGFX_BUILD_CAP_MASK                                                 \
    ((uint32_t) (LGFX_CAP_SPRITE | LGFX_CAP_PUSHIMAGE | LGFX_CAP_LAST_ERROR \
        | LGFX_CAP_PALETTE                                                  \
        | (LGFX_PORT_SUPPORTS_TOUCH ? LGFX_CAP_TOUCH : 0u)))

// -----------------------------------------------------------------------------
// Op-specific request flags
// -----------------------------------------------------------------------------

#define LGFX_F_TEXT_HAS_BG (1u << 0) // setTextColor: background color is provided
#define LGFX_F_COLOR_INDEX (1u << 1) // primitives: color argument is palette index, not rgb565
#define LGFX_F_TEXT_FG_INDEX (1u << 2) // setTextColor: fg is palette index, not rgb565
#define LGFX_F_TEXT_BG_INDEX (1u << 3) // setTextColor: bg is palette index, not rgb565
#define LGFX_F_TRANSPARENT_INDEX (1u << 4) // sprite push transparent value is palette index, not rgb565

#ifndef F_TEXT_HAS_BG
#define F_TEXT_HAS_BG LGFX_F_TEXT_HAS_BG
#endif
