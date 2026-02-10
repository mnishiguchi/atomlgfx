// ports/include/lgfx_port/lgfx_port.h
/*
 * Canonical op list: ports/include/lgfx_port/ops.def
 * Protocol contract: docs/LGFX_PORT_PROTOCOL.md
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "globalcontext.h"
#include "term.h"

#ifdef __cplusplus
extern "C" {
#endif

// Protocol constants + caps live here (and the one legacy alias).
#include "lgfx_port/caps.h"

/*
 * Op-specific request flags (req->flags)
 *
 * NOTE:
 * - These are per-op. Unrecognized bits must be rejected as bad_flags.
 */
#define LGFX_F_TEXT_HAS_BG (1u << 0) // setTextColor: background color is provided

// Backward-compat alias (if some handlers use this shorter name)
#ifndef F_TEXT_HAS_BG
#define F_TEXT_HAS_BG LGFX_F_TEXT_HAS_BG
#endif

// Common atoms (interned once)
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
    term not_initialized; // used when ops require init but port isn't initialized

    // structured reply tags
    term caps;
    term last_error;
    term none;

    // ops (generated from ports/include/lgfx_port/ops.def)
#define X(op, handler, atom_str, ...) term op;
#include "lgfx_port/ops.def"
#undef X

} lgfx_atoms_t;

typedef struct
{
    term last_op; // atom or 'none'
    term reason; // atom or 'none' (must be safe to store across calls)
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

    // Optional cached LCD dimensions (if you want width/height without device calls)
    uint32_t width;
    uint32_t height;

    // Lifecycle state (set true after successful init; cleared on close/deinit)
    bool initialized;

    // Future: pointers/handles to src/ device adapter state
    // void *device;
} lgfx_port_t;

void lgfx_atoms_init(GlobalContext *global, lgfx_atoms_t *atoms);

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
