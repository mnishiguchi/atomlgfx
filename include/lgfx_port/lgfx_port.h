// lgfx_port/include/lgfx_port/lgfx_port.h
/*
 * Canonical op list: lgfx_port/include/lgfx_port/ops.def
 * Protocol contract: docs/LGFX_PORT_PROTOCOL.md
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Build-time configuration (generated from include/lgfx_port/lgfx_port_config.h.in)
#include "lgfx_port/lgfx_port_config.h"

#include "context.h"
#include "globalcontext.h"
#include "term.h"

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

// -----------------------------------------------------------------------------
// Protocol constants / FeatureBits / validation domains
// -----------------------------------------------------------------------------

#ifndef LGFX_PORT_PROTO_VER
#define LGFX_PORT_PROTO_VER 1u
#endif

#ifndef LGFX_PORT_MAX_BINARY_BYTES
#define LGFX_PORT_MAX_BINARY_BYTES (256u * 1024u)
#endif

#ifndef LGFX_PORT_MAX_SPRITES
#define LGFX_PORT_MAX_SPRITES 8u
#endif

#ifndef LGFX_PORT_SPRITE_DEFAULT_DEPTH
#define LGFX_PORT_SPRITE_DEFAULT_DEPTH 16u
#endif

#if (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 1u) && (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 2u) && (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 4u) \
    && (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 8u) && (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 16u) && (LGFX_PORT_SPRITE_DEFAULT_DEPTH != 24u)
#error "LGFX_PORT_SPRITE_DEFAULT_DEPTH must be one of: 1,2,4,8,16,24"
#endif

// Advertisement / enable gates (0/1).
// These gates must stay consistent with:
// - ops.def feature_cap_bit usage
// - getCaps FeatureBits derivation
// - runtime behavior (disabled ops => {error, unsupported})
#ifndef LGFX_PORT_SUPPORTS_SPRITE
#define LGFX_PORT_SUPPORTS_SPRITE 1
#endif

#ifndef LGFX_PORT_SUPPORTS_LAST_ERROR
#define LGFX_PORT_SUPPORTS_LAST_ERROR 1
#endif

#ifndef LGFX_PORT_SUPPORTS_PUSHIMAGE
#define LGFX_PORT_SUPPORTS_PUSHIMAGE 1
#endif

#ifndef LGFX_PORT_SUPPORTS_TOUCH
#define LGFX_PORT_SUPPORTS_TOUCH 0
#endif

#ifndef LGFX_PORT_SUPPORTS_JPG_FILE
#define LGFX_PORT_SUPPORTS_JPG_FILE 0
#endif

#ifndef LGFX_PORT_SUPPORTS_PNG_FILE
#define LGFX_PORT_SUPPORTS_PNG_FILE 0
#endif

#ifndef LGFX_PORT_SUPPORTS_BATCH_VOID
#define LGFX_PORT_SUPPORTS_BATCH_VOID 0
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

// FeatureBits (wire)
#define LGFX_CAP_SPRITE (1u << 0)
#define LGFX_CAP_PUSHIMAGE (1u << 1)
#define LGFX_CAP_JPG_FILE (1u << 2)
#define LGFX_CAP_PNG_FILE (1u << 3)
#define LGFX_CAP_LAST_ERROR (1u << 4)
#define LGFX_CAP_BATCH_VOID (1u << 5)
#define LGFX_CAP_TOUCH (1u << 6)

#define LGFX_CAP_SAFE_YIELD_FORGIVING (1u << 8)
#define LGFX_CAP_SAFE_YIELD_STRICT (1u << 9)

#define LGFX_CAP_KNOWN_MASK                                                                                                                      \
    (LGFX_CAP_SPRITE | LGFX_CAP_PUSHIMAGE | LGFX_CAP_TOUCH | LGFX_CAP_JPG_FILE | LGFX_CAP_PNG_FILE | LGFX_CAP_LAST_ERROR | LGFX_CAP_BATCH_VOID   \
        | LGFX_CAP_SAFE_YIELD_FORGIVING | LGFX_CAP_SAFE_YIELD_STRICT)

#ifndef LGFX_PORT_SAFE_YIELD_CAP
#define LGFX_PORT_SAFE_YIELD_CAP 0u
#endif

#if (LGFX_PORT_SAFE_YIELD_CAP != 0u) && (LGFX_PORT_SAFE_YIELD_CAP != LGFX_CAP_SAFE_YIELD_FORGIVING) && (LGFX_PORT_SAFE_YIELD_CAP != LGFX_CAP_SAFE_YIELD_STRICT)
#error "LGFX_PORT_SAFE_YIELD_CAP must be 0, LGFX_CAP_SAFE_YIELD_FORGIVING, or LGFX_CAP_SAFE_YIELD_STRICT"
#endif

/*
 * Op-specific request flags (req->flags)
 *
 * NOTE:
 * - These are per-op. Unrecognized bits must be rejected as bad_flags.
 */
#define LGFX_F_TEXT_HAS_BG (1u << 0) // setTextColor: background color is provided

#ifndef F_TEXT_HAS_BG
#define F_TEXT_HAS_BG LGFX_F_TEXT_HAS_BG
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    term ok;
    term error;

    term lgfx;

    // common values
    term pong;
    term true_;
    term false_;

    // error reasons
    term bad_proto;
    term bad_op;
    term bad_flags;
    term bad_args;
    term bad_target;
    term not_writing;
    term no_memory;
    term internal;
    term unsupported;
    term not_initialized;

    // structured reply tags
    term caps;
    term last_error;
    term none;

    // ops (generated from lgfx_port/include/lgfx_port/ops.def)
#define X(op, handler, atom_str, ...) term op;
#include "lgfx_port/ops.def"
#undef X

} lgfx_atoms_t;

typedef struct
{
    term last_op; // atom or 'none'
    term reason; // atom or 'none'
    uint32_t flags;
    uint32_t target;
    int32_t esp_err; // 0 if n/a
} lgfx_last_error_t;

typedef struct
{
    GlobalContext *global;
    Context *ctx; // owning AtomVM context (used by handlers/worker)
    void *worker; // lgfx_worker_t* (opaque; owned by lgfx_worker.c)

    lgfx_atoms_t atoms;
    lgfx_last_error_t last_error;

    uint32_t width;
    uint32_t height;

    bool initialized;
} lgfx_port_t;

void lgfx_atoms_init(GlobalContext *global, lgfx_atoms_t *atoms);

// getCaps derivation (metadata-driven; see ops.def + docs/LGFX_PORT_PROTOCOL.md)
uint32_t lgfx_port_feature_bits(const lgfx_port_t *port);
uint8_t lgfx_port_max_sprites(const lgfx_port_t *port);

// Returns true if an op is known and enabled under build/runtime gates.
// Disabled ops must behave as {error, unsupported} and must not be advertised via FeatureBits.
bool lgfx_port_op_is_enabled(const lgfx_port_t *port, term op_atom);

static inline void lgfx_last_error_clear(lgfx_port_t *port)
{
    port->last_error.last_op = port->atoms.none;
    port->last_error.reason = port->atoms.none;
    port->last_error.flags = 0;
    port->last_error.target = 0;
    port->last_error.esp_err = 0;
}

static inline void lgfx_last_error_set(
    lgfx_port_t *port,
    term last_op,
    term reason,
    uint32_t flags,
    uint32_t target,
    int32_t esp_err)
{
    port->last_error.last_op = last_op;
    port->last_error.reason = reason;
    port->last_error.flags = flags;
    port->last_error.target = target;
    port->last_error.esp_err = esp_err;
}

#ifdef __cplusplus
}
#endif
