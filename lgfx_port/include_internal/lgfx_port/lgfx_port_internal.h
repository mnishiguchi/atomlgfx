/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/include_internal/lgfx_port/lgfx_port_internal.h

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "globalcontext.h"
#include "term.h"

#include "lgfx_device/lgfx_device.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/protocol.h"
#include "lgfx_runtime/lgfx_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    term ok;
    term error;

    term lgfx;

    term pong;
    term true_;
    term false_;

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

    term caps;
    term last_error;
    term none;

    term batch;
    term batch_status;
    term batch_failure;

    term idle;
    term queued;
    term running;
    term completed;
    term failed;

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

typedef struct lgfx_request_t
{
    uint32_t proto_ver;
    term op;
    uint32_t target;
    uint32_t flags;
    term request_tuple;
    int arity;
} lgfx_request_t;

typedef struct lgfx_port_t
{
    GlobalContext *global;
    Context *ctx;

    lgfx_atoms_t atoms;
    lgfx_last_error_t last_error;

    uint32_t width;
    uint32_t height;

    // Per-port persisted open-time config snapshot.
    // This survives close/init cycles for the same port handle.
    lgfx_open_config_overrides_t open_config_overrides;

    // Explicit batch runtime state is per-port.
    lgfx_runtime_t runtime;

    bool initialized;
} lgfx_port_t;

void lgfx_atoms_init(GlobalContext *global, lgfx_atoms_t *atoms);

// Internal helpers used by getCaps / protocol reply assembly.
uint32_t lgfx_port_feature_bits(const lgfx_port_t *port);
uint8_t lgfx_port_max_sprites(const lgfx_port_t *port);

// Op registry / dispatch helpers.
bool lgfx_op_try_from_atom(const lgfx_port_t *port, term op_atom, lgfx_op_t *out_op);
term lgfx_op_atom_from_op(const lgfx_port_t *port, lgfx_op_t op);
const lgfx_op_meta_t *lgfx_op_meta_lookup(const lgfx_port_t *port, term op_atom);
const char *lgfx_op_name_from_atom(const lgfx_port_t *port, term op_atom);
lgfx_handler_fn lgfx_dispatch_lookup(lgfx_port_t *port, term op_atom);
bool lgfx_port_op_is_enabled(const lgfx_port_t *port, term op_atom);

// Request-envelope validation helpers.
term lgfx_require_proto_ver(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_require_target_domain(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_require_arity_range(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *req,
    int min_arity,
    int max_arity);
term lgfx_require_flags_allowed_mask(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *req,
    uint32_t allowed_mask);
term lgfx_require_target_policy(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *req,
    uint8_t policy);
term lgfx_require_state_policy(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *req,
    uint8_t policy);

// open_port/2 option parsing.
bool lgfx_parse_open_config_opts(
    GlobalContext *global,
    term opts,
    lgfx_open_config_overrides_t *out_overrides,
    const char **error_detail);

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
