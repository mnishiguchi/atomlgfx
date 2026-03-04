// lgfx_port/include_internal/lgfx_port/protocol.h
/*
 * Protocol-level constants and wire contract definitions.
 *
 * - Protocol version + limits
 * - FeatureBits (wire)
 * - Build/runtime gate validation domains
 * - Op request flags (req->flags)
 * - Optional protocol-level string constants (doc/debug)
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Build-time configuration (generated from lgfx_port/cmake/lgfx_port_config.h.in)
#include "lgfx_port/lgfx_port_config.h"

#include "lgfx_port/lgfx_port.h" // for LGFX_FONT_PRESET_* and other wire constants

// -----------------------------------------------------------------------------
// Optional: protocol-level string constants (doc/debug)
// -----------------------------------------------------------------------------
//
// Keep these off by default to avoid exporting extra public macros.
// Enable with -DLGFX_PORT_PROTOCOL_DEBUG_STRINGS=1 if you want them.
//
#ifndef LGFX_PORT_PROTOCOL_DEBUG_STRINGS
#define LGFX_PORT_PROTOCOL_DEBUG_STRINGS 0
#endif

#if (LGFX_PORT_PROTOCOL_DEBUG_STRINGS != 0) && (LGFX_PORT_PROTOCOL_DEBUG_STRINGS != 1)
#error "LGFX_PORT_PROTOCOL_DEBUG_STRINGS must be 0 or 1"
#endif

#if LGFX_PORT_PROTOCOL_DEBUG_STRINGS
// Optional: protocol-level error atom string constants (doc/debug)
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

// Optional detail tuple reason tags
#define LGFX_ERR_BATCH_FAILED "batch_failed"
#endif

// -----------------------------------------------------------------------------
// Protocol constants
// -----------------------------------------------------------------------------
//
// Notes:
// - Some constants have safe defaults here.
// - LGFX_PORT_MAX_SPRITES must come from the generated config header so builds
//   cannot silently fall back when the config is missing/stale.
//
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
// Advertisement / enable gates (0/1).
// These must come from the generated config (single source of truth).
// -----------------------------------------------------------------------------
#ifndef LGFX_PORT_SUPPORTS_SPRITE
#error "LGFX_PORT_SUPPORTS_SPRITE must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_SUPPORTS_LAST_ERROR
#error "LGFX_PORT_SUPPORTS_LAST_ERROR must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_SUPPORTS_PUSHIMAGE
#error "LGFX_PORT_SUPPORTS_PUSHIMAGE must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_SUPPORTS_TOUCH
#error "LGFX_PORT_SUPPORTS_TOUCH must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_SUPPORTS_JPG_FILE
#error "LGFX_PORT_SUPPORTS_JPG_FILE must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_SUPPORTS_PNG_FILE
#error "LGFX_PORT_SUPPORTS_PNG_FILE must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_SUPPORTS_BATCH_VOID
#error "LGFX_PORT_SUPPORTS_BATCH_VOID must be defined by lgfx_port_config.h"
#endif

#if (LGFX_PORT_SUPPORTS_SPRITE != 0) && (LGFX_PORT_SUPPORTS_SPRITE != 1)
#error "LGFX_PORT_SUPPORTS_SPRITE must be 0 or 1"
#endif
#if (LGFX_PORT_SUPPORTS_LAST_ERROR != 0) && (LGFX_PORT_SUPPORTS_LAST_ERROR != 1)
#error "LGFX_PORT_SUPPORTS_LAST_ERROR must be 0 or 1"
#endif
#if (LGFX_PORT_SUPPORTS_PUSHIMAGE != 0) && (LGFX_PORT_SUPPORTS_PUSHIMAGE != 1)
#error "LGFX_PORT_SUPPORTS_PUSHIMAGE must be 0 or 1"
#endif
#if (LGFX_PORT_SUPPORTS_TOUCH != 0) && (LGFX_PORT_SUPPORTS_TOUCH != 1)
#error "LGFX_PORT_SUPPORTS_TOUCH must be 0 or 1"
#endif
#if (LGFX_PORT_SUPPORTS_JPG_FILE != 0) && (LGFX_PORT_SUPPORTS_JPG_FILE != 1)
#error "LGFX_PORT_SUPPORTS_JPG_FILE must be 0 or 1"
#endif
#if (LGFX_PORT_SUPPORTS_PNG_FILE != 0) && (LGFX_PORT_SUPPORTS_PNG_FILE != 1)
#error "LGFX_PORT_SUPPORTS_PNG_FILE must be 0 or 1"
#endif
#if (LGFX_PORT_SUPPORTS_BATCH_VOID != 0) && (LGFX_PORT_SUPPORTS_BATCH_VOID != 1)
#error "LGFX_PORT_SUPPORTS_BATCH_VOID must be 0 or 1"
#endif

// -----------------------------------------------------------------------------
// Cross-check: protocol support must not exceed compiled implementation
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

// -----------------------------------------------------------------------------
// FeatureBits (wire)
// -----------------------------------------------------------------------------
#define LGFX_CAP_SPRITE (1u << 0)
#define LGFX_CAP_PUSHIMAGE (1u << 1)
#define LGFX_CAP_JPG_FILE (1u << 2)
#define LGFX_CAP_PNG_FILE (1u << 3)
#define LGFX_CAP_LAST_ERROR (1u << 4)
#define LGFX_CAP_BATCH_VOID (1u << 5)
#define LGFX_CAP_TOUCH (1u << 6)

#define LGFX_CAP_SAFE_YIELD_FORGIVING (1u << 8)
#define LGFX_CAP_SAFE_YIELD_STRICT (1u << 9)

#define LGFX_CAP_KNOWN_MASK                                                                                                                    \
    (LGFX_CAP_SPRITE | LGFX_CAP_PUSHIMAGE | LGFX_CAP_TOUCH | LGFX_CAP_JPG_FILE | LGFX_CAP_PNG_FILE | LGFX_CAP_LAST_ERROR | LGFX_CAP_BATCH_VOID \
        | LGFX_CAP_SAFE_YIELD_FORGIVING | LGFX_CAP_SAFE_YIELD_STRICT)

// Safe yield capability (0 or one of the two safe-yield bits). Must come from config.
#ifndef LGFX_PORT_SAFE_YIELD_CAP
#error "LGFX_PORT_SAFE_YIELD_CAP must be defined by lgfx_port_config.h"
#endif

#if (LGFX_PORT_SAFE_YIELD_CAP != 0u) && (LGFX_PORT_SAFE_YIELD_CAP != LGFX_CAP_SAFE_YIELD_FORGIVING) && (LGFX_PORT_SAFE_YIELD_CAP != LGFX_CAP_SAFE_YIELD_STRICT)
#error "LGFX_PORT_SAFE_YIELD_CAP must be 0, LGFX_CAP_SAFE_YIELD_FORGIVING, or LGFX_CAP_SAFE_YIELD_STRICT"
#endif

// -----------------------------------------------------------------------------
// Build capability mask (compiled surface area for FeatureBits gating)
//
// Used by the port implementation to check whether an ops.def feature_cap_bit is
// valid/enabled under the current build configuration.
//
// Notes:
// - Safe-yield is selected by LGFX_PORT_SAFE_YIELD_CAP (0 or exactly one bit).
// - This mask is intentionally independent of "transaction surface exists" logic;
//   FeatureBits still decides whether to advertise the safe-yield bit at runtime.
// -----------------------------------------------------------------------------
#define LGFX_BUILD_CAP_MASK                                          \
    ((uint32_t) ((LGFX_PORT_SUPPORTS_SPRITE ? LGFX_CAP_SPRITE : 0u)  \
        | (LGFX_PORT_SUPPORTS_PUSHIMAGE ? LGFX_CAP_PUSHIMAGE : 0u)   \
        | (LGFX_PORT_SUPPORTS_TOUCH ? LGFX_CAP_TOUCH : 0u)           \
        | (LGFX_PORT_SUPPORTS_LAST_ERROR ? LGFX_CAP_LAST_ERROR : 0u) \
        | (LGFX_PORT_SUPPORTS_JPG_FILE ? LGFX_CAP_JPG_FILE : 0u)     \
        | (LGFX_PORT_SUPPORTS_PNG_FILE ? LGFX_CAP_PNG_FILE : 0u)     \
        | (LGFX_PORT_SUPPORTS_BATCH_VOID ? LGFX_CAP_BATCH_VOID : 0u) \
        | (uint32_t) LGFX_PORT_SAFE_YIELD_CAP))

// -----------------------------------------------------------------------------
// Op-specific request flags (req->flags)
//
// NOTE:
// - These are per-op. Unrecognized bits must be rejected as bad_flags.
// -----------------------------------------------------------------------------
#define LGFX_F_TEXT_HAS_BG (1u << 0) // setTextColor: background color is provided

// Back-compat alias (if any callers still use the short name)
#ifndef F_TEXT_HAS_BG
#define F_TEXT_HAS_BG LGFX_F_TEXT_HAS_BG
#endif
