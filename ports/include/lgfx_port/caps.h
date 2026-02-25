// ports/include/lgfx_port/caps.h
#pragma once

#include <stdint.h>

// Capability bits returned in:
//   {ok, {caps, ProtoVer, MaxBinaryBytes, MaxSprites, FeatureBits}}
//
// Must match docs/LGFX_PORT_PROTOCOL.md.

// Protocol-level constants returned by getCaps
#ifndef LGFX_PORT_PROTO_VER
#define LGFX_PORT_PROTO_VER 1u
#endif

// Upper bound the BE side should assume for a single binary payload (bytes).
// If you change this, update docs/LGFX_PORT_PROTOCOL.md.
#ifndef LGFX_PORT_MAX_BINARY_BYTES
#define LGFX_PORT_MAX_BINARY_BYTES (256u * 1024u)
#endif

// Upper bound for concurrently addressable sprites (if sprite feature is enabled).
#ifndef LGFX_PORT_MAX_SPRITES
#define LGFX_PORT_MAX_SPRITES 8u
#endif

// Default sprite color depth used by createSprite when Depth is omitted.
// (Dual-arity createSprite supports:
//   - {.., W, H}
//   - {.., W, H, Depth})
#ifndef LGFX_PORT_SPRITE_DEFAULT_DEPTH
#define LGFX_PORT_SPRITE_DEFAULT_DEPTH 16u
#endif

#if (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 1u)  && \
    (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 2u)  && \
    (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 4u)  && \
    (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 8u)  && \
    (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 16u) && \
    (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 24u)
#error "LGFX_PORT_SPRITE_DEFAULT_DEPTH must be one of: 1,2,4,8,16,24"
#endif

// Build-time protocol advertisement gate for sprite capability.
// Default is enabled now that sprite support exists.
// Set to 0 to force-disable advertisement for testing/compat.
#ifndef LGFX_PORT_SUPPORTS_SPRITE
#define LGFX_PORT_SUPPORTS_SPRITE 1
#endif

#if (LGFX_PORT_SUPPORTS_SPRITE != 0) && (LGFX_PORT_SUPPORTS_SPRITE != 1)
#error "LGFX_PORT_SUPPORTS_SPRITE must be 0 or 1"
#endif

// Build-time protocol advertisement gate for last_error capability.
// Default is enabled.
// Set to 0 to force-disable advertisement/handler behavior for testing/compat.
#ifndef LGFX_PORT_SUPPORTS_LAST_ERROR
#define LGFX_PORT_SUPPORTS_LAST_ERROR 1
#endif

#if (LGFX_PORT_SUPPORTS_LAST_ERROR != 0) && \
    (LGFX_PORT_SUPPORTS_LAST_ERROR != 1)
#error "LGFX_PORT_SUPPORTS_LAST_ERROR must be 0 or 1"
#endif

// Build-time protocol advertisement gate for pushImage capability.
// Default is enabled because pushImage is implemented.
#ifndef LGFX_PORT_SUPPORTS_PUSHIMAGE
#define LGFX_PORT_SUPPORTS_PUSHIMAGE 1
#endif

#if (LGFX_PORT_SUPPORTS_PUSHIMAGE != 0) && \
    (LGFX_PORT_SUPPORTS_PUSHIMAGE != 1)
#error "LGFX_PORT_SUPPORTS_PUSHIMAGE must be 0 or 1"
#endif

// Build-time protocol advertisement gate for JPG file decoding capability.
// Default is disabled until file-based JPG path is implemented/exposed.
#ifndef LGFX_PORT_SUPPORTS_JPG_FILE
#define LGFX_PORT_SUPPORTS_JPG_FILE 0
#endif

#if (LGFX_PORT_SUPPORTS_JPG_FILE != 0) && \
    (LGFX_PORT_SUPPORTS_JPG_FILE != 1)
#error "LGFX_PORT_SUPPORTS_JPG_FILE must be 0 or 1"
#endif

// Build-time protocol advertisement gate for PNG file decoding capability.
// Default is disabled until file-based PNG path is implemented/exposed.
#ifndef LGFX_PORT_SUPPORTS_PNG_FILE
#define LGFX_PORT_SUPPORTS_PNG_FILE 0
#endif

#if (LGFX_PORT_SUPPORTS_PNG_FILE != 0) && \
    (LGFX_PORT_SUPPORTS_PNG_FILE != 1)
#error "LGFX_PORT_SUPPORTS_PNG_FILE must be 0 or 1"
#endif

// Build-time protocol advertisement gate for batchVoid capability.
// Default is disabled until batch op is implemented/exposed.
#ifndef LGFX_PORT_SUPPORTS_BATCH_VOID
#define LGFX_PORT_SUPPORTS_BATCH_VOID 0
#endif

#if (LGFX_PORT_SUPPORTS_BATCH_VOID != 0) && \
    (LGFX_PORT_SUPPORTS_BATCH_VOID != 1)
#error "LGFX_PORT_SUPPORTS_BATCH_VOID must be 0 or 1"
#endif

// Protocol target space reserves 255 as invalid target sentinel.
#if LGFX_PORT_MAX_SPRITES > 254u
#error "LGFX_PORT_MAX_SPRITES must be <= 254 (255 is reserved invalid target)"
#endif

// FeatureBits (bitset)
#define LGFX_CAP_SPRITE     (1u << 0)
#define LGFX_CAP_PUSHIMAGE  (1u << 1)
#define LGFX_CAP_JPG_FILE   (1u << 2)
#define LGFX_CAP_PNG_FILE   (1u << 3)
#define LGFX_CAP_LAST_ERROR (1u << 4)
#define LGFX_CAP_BATCH_VOID (1u << 5)

// Safe-yield behavior (set at most one)
#define LGFX_CAP_SAFE_YIELD_FORGIVING (1u << 8)
#define LGFX_CAP_SAFE_YIELD_STRICT    (1u << 9)

// All protocol-facing FeatureBits that may appear on the wire.
// Keep in sync with docs/LGFX_PORT_PROTOCOL.md.
#define LGFX_CAP_KNOWN_MASK ( \
    LGFX_CAP_SPRITE | \
    LGFX_CAP_PUSHIMAGE | \
    LGFX_CAP_JPG_FILE | \
    LGFX_CAP_PNG_FILE | \
    LGFX_CAP_LAST_ERROR | \
    LGFX_CAP_BATCH_VOID | \
    LGFX_CAP_SAFE_YIELD_FORGIVING | \
    LGFX_CAP_SAFE_YIELD_STRICT)

// Default safe-yield capability bit (override at build time if needed)
#ifndef LGFX_PORT_SAFE_YIELD_CAP
#define LGFX_PORT_SAFE_YIELD_CAP 0u
#endif

// Validate: must be 0 or exactly one safe-yield bit.
#if (LGFX_PORT_SAFE_YIELD_CAP != 0u) && \
    (LGFX_PORT_SAFE_YIELD_CAP != LGFX_CAP_SAFE_YIELD_FORGIVING) && \
    (LGFX_PORT_SAFE_YIELD_CAP != LGFX_CAP_SAFE_YIELD_STRICT)
#error "LGFX_PORT_SAFE_YIELD_CAP must be 0, LGFX_CAP_SAFE_YIELD_FORGIVING, or LGFX_CAP_SAFE_YIELD_STRICT"
#endif
