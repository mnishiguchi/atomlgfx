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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    term ok;
    term error;

    term lgfx;
    term call;

    term pong;
    term true_;
    term false_;

    term bad_proto;
    term bad_op;
    term bad_flags;
    term bad_args;
    term bad_target;
    term batch_failed;
    term not_writing;
    term no_memory;
    term internal;
    term unsupported;
    term not_initialized;
    term resource_busy;
    term renderer_running;

    term caps;
    term last_error;
    term none;
    term render_program_stats;

} lgfx_atoms_t;

typedef struct
{
    term last_op; // numeric opcode or 'none'
    term reason; // atom or 'none'
    uint32_t flags;
    uint32_t target;
    int32_t esp_err; // 0 if n/a
} lgfx_last_error_t;

#ifndef LGFX_REQ_MAX_INLINE_ARGS
#define LGFX_REQ_MAX_INLINE_ARGS 16
#endif

#if LGFX_REQ_MAX_INLINE_ARGS < 1
#error "LGFX_REQ_MAX_INLINE_ARGS must be >= 1"
#endif

typedef struct lgfx_request_t
{
    uint32_t proto_ver;
    uint32_t opcode;
    lgfx_op_t op;
    uint32_t target;
    uint32_t flags;
    term request_tuple;
    term args_list;
    int arity;
    int arg_count;
    term args[LGFX_REQ_MAX_INLINE_ARGS];
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

    bool initialized;
} lgfx_port_t;

void lgfx_atoms_init(GlobalContext *global, lgfx_atoms_t *atoms);

// Internal helpers used by getCaps / protocol reply assembly.
uint32_t lgfx_port_feature_bits(const lgfx_port_t *port);
uint8_t lgfx_port_max_sprites(const lgfx_port_t *port);

// Op registry / dispatch helpers.
bool lgfx_op_try_from_opcode(uint32_t opcode, lgfx_op_t *out_op);
const lgfx_op_meta_t *lgfx_op_meta_lookup_by_op(lgfx_op_t op);
const char *lgfx_op_name_from_op(lgfx_op_t op);
lgfx_handler_fn lgfx_dispatch_lookup_by_op(lgfx_port_t *port, lgfx_op_t op);
bool lgfx_port_op_is_enabled_by_op(const lgfx_port_t *port, lgfx_op_t op);

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
