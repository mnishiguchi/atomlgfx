/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/lgfx_port.c

// AtomVM port driver entry point for the LovyanGFX port.
//
// Responsibilities in this file:
// - Per-context port creation and teardown
// - Mailbox message handling on the port thread
// - Request decode -> metadata validation -> dispatch -> reply flow
//
// Non-responsibilities:
// - Atom initialization (handled by atoms.c)
// - Op metadata registry + dispatch lookup (handled by op_registry.c)
// - Request validation helpers (handled by request_validation.c)
// - open_port/2 option parsing (handled by open_config.c)
// - Device implementation details (handled by lgfx_device/*)
// - AtomVM term decoding details (handled by proto_term.c)
// - Reply encoding helpers (handled by proto_term.c)

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "context.h"
#include "globalcontext.h"
#include "mailbox.h"
#include "memory.h" // memory_ensure_free / MEMORY_GC_OK
#include "port.h" // port_parse_gen_message / port_send_reply
#include "portnifloader.h"

#include "esp_log.h"

#include "lgfx_device/lgfx_device.h"
#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/proto_term.h"

#ifndef LGFX_PORT_DEBUG
#define LGFX_PORT_DEBUG 0
#endif
#if (LGFX_PORT_DEBUG != 0) && (LGFX_PORT_DEBUG != 1)
#error "LGFX_PORT_DEBUG must be 0 or 1"
#endif

static const char *const TAG = "lgfx_port";

// -----------------------------------------------------------------------------
// Port lifecycle + mailbox -> decode -> validate -> dispatch
// -----------------------------------------------------------------------------

static term ensure_valid_reply(Context *ctx, lgfx_port_t *port, term reply)
{
    if (!term_is_invalid_term(reply)) {
        return reply;
    }

    if (memory_ensure_free(ctx, 3) != MEMORY_GC_OK) {
        return term_invalid_term();
    }

    return lgfx_reply_error(ctx, port, port->atoms.no_memory);
}

static void lgfx_port_teardown(Context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    lgfx_port_t *port = (lgfx_port_t *) ctx->platform_data;
    if (port == NULL) {
        return;
    }

    ctx->platform_data = NULL;

    /*
     * port->initialized is a port-local lifecycle flag.
     *
     * It means this port completed init() successfully during its current
     * ownership window. It does not describe global singleton availability or
     * current ownership outside that window.
     *
     * Global publication, ownership, and ready-state live in
     * lgfx_device/state.cpp.
     */
    if (port->initialized) {
        (void) lgfx_device_close_for_owner((const void *) port);
    }

    port->initialized = false;
    port->width = 0;
    port->height = 0;
    lgfx_last_error_clear(port);

    lgfx_runtime_deinit(&port->runtime);

    port->ctx = NULL;

    free(port);
}

void lgfx_port_handle_mailbox_message(Context *ctx, lgfx_port_t *port, term msg)
{
    GenMessage gen;
    enum GenMessageParseResult parse_res = port_parse_gen_message(msg, &gen);
    if (parse_res != GenCallMessage) {
        return;
    }

    lgfx_request_t req;
    term decode_error = term_invalid_term();

    if (!lgfx_term_decode_request(ctx, port, gen.req, &req, &decode_error)) {
        /*
         * Decode failed before full request metadata existed.
         *
         * If decode_error is invalid, treat it as no_memory. Otherwise prefer an
         * explicit {error, Reason} reply when one was already built.
         */
        term reason = term_is_invalid_term(decode_error) ? port->atoms.no_memory : port->atoms.bad_proto;

        term decoded_reason = term_invalid_term();
        if (!term_is_invalid_term(decode_error)
            && lgfx_is_error_reply(ctx, port, decode_error, &decoded_reason)
            && term_is_atom(decoded_reason)) {
            reason = decoded_reason;
        }

        lgfx_last_error_set(port, port->atoms.none, reason, 0, 0, 0);

        term safe = ensure_valid_reply(ctx, port, decode_error);
        if (!term_is_invalid_term(safe)) {
            port_send_reply(ctx, gen.pid, gen.ref, safe);
        }

        return;
    }

    term reply = term_invalid_term();
    term pre = term_invalid_term();

    pre = lgfx_require_proto_ver(ctx, port, &req);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    pre = lgfx_require_target_domain(ctx, port, &req);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    const lgfx_op_meta_t *meta = lgfx_op_meta_lookup(port, req.op);
    if (meta == NULL) {
        reply = reply_error(ctx, port, &req, port->atoms.bad_op, 0);
        goto send_reply;
    }

    if (!lgfx_port_op_is_enabled(port, req.op)) {
        reply = reply_error(ctx, port, &req, port->atoms.unsupported, 0);
        goto send_reply;
    }

    pre = lgfx_require_arity_range(ctx, port, &req, meta->min_arity, meta->max_arity);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    pre = lgfx_require_flags_allowed_mask(ctx, port, &req, meta->allowed_flags_mask);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    pre = lgfx_require_target_policy(ctx, port, &req, meta->target_policy);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    pre = lgfx_require_state_policy(ctx, port, &req, meta->state_policy);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    lgfx_handler_fn handler = lgfx_dispatch_lookup(port, req.op);
    if (handler == NULL) {
        reply = reply_error(ctx, port, &req, port->atoms.internal, 0);
        goto send_reply;
    }

    reply = handler(ctx, port, &req);

send_reply:
    if (term_is_invalid_term(reply)) {
        int32_t esp_err = 0;
        if (port->last_error.last_op == req.op) {
            esp_err = port->last_error.esp_err;
        }

        if (port->last_error.last_op != req.op || port->last_error.reason != port->atoms.no_memory) {
            reply = reply_error(ctx, port, &req, port->atoms.no_memory, esp_err);
        }

        reply = ensure_valid_reply(ctx, port, reply);
    }

    if (!term_is_invalid_term(reply)) {
        port_send_reply(ctx, gen.pid, gen.ref, reply);
    }
}

static void lgfx_port_drain_mailbox(lgfx_port_t *port)
{
    if (port == NULL || port->ctx == NULL) {
        return;
    }

    Context *ctx = port->ctx;
    Mailbox *mailbox = &ctx->mailbox;
    Heap *heap = &ctx->heap;

    Message *message = mailbox_first(mailbox);
    while (message != NULL) {
        term message_term = message->message;

        lgfx_port_handle_mailbox_message(ctx, port, message_term);

        MailboxMessage *removed = mailbox_take_message(mailbox);
        if (removed == NULL) {
            break;
        }

        mailbox_message_dispose(removed, heap);

        if (lgfx_runtime_has_pending(&port->runtime)) {
            (void) lgfx_runtime_process_pending(&port->runtime);
        }

        message = mailbox_first(mailbox);
    }
}

static NativeHandlerResult lgfx_port_native_handler(Context *ctx)
{
    lgfx_port_t *port = (lgfx_port_t *) ctx->platform_data;
    if (port == NULL) {
        return NativeContinue;
    }

    if (ctx->flags & Killed) {
        lgfx_port_teardown(ctx);
        return NativeContinue;
    }

    lgfx_port_drain_mailbox(port);

    return NativeContinue;
}

static void lgfx_port_init(GlobalContext *global)
{
    (void) global;
}

static void lgfx_port_destroy(GlobalContext *global)
{
    (void) global;
}

static Context *lgfx_port_create_port(GlobalContext *global, term opts)
{
    Context *ctx = context_new(global);
    if (ctx == NULL) {
        return NULL;
    }

    lgfx_port_t *port = (lgfx_port_t *) calloc(1, sizeof(lgfx_port_t));
    if (port == NULL) {
        context_destroy(ctx);
        return NULL;
    }

    port->global = global;
    port->ctx = ctx;

    lgfx_atoms_init(global, &port->atoms);
    lgfx_last_error_clear(port);

    if (lgfx_runtime_init(&port->runtime) != ESP_OK) {
        free(port);
        context_destroy(ctx);
        return NULL;
    }

    lgfx_open_config_overrides_t open_config_overrides = { 0 };
    const char *open_config_error = NULL;

    if (!lgfx_parse_open_config_opts(global, opts, &open_config_overrides, &open_config_error)) {
        ESP_LOGE(
            TAG,
            "invalid open_port opts for lgfx_port: %s",
            open_config_error ? open_config_error : "unknown error");
        lgfx_runtime_deinit(&port->runtime);
        free(port);
        context_destroy(ctx);
        return NULL;
    }

    /*
     * Persist a per-port config snapshot.
     *
     * This includes the all-default case, so every opened port keeps a
     * deterministic baseline for future init() calls.
     *
     * The snapshot is configuration only. It is separate from singleton
     * publication, ownership, and begin()/ready state. Opening another port does
     * not overwrite a global pending config; the singleton constraint is enforced
     * only when a port tries to claim the live device via init().
     */
    port->open_config_overrides = open_config_overrides;

    ctx->platform_data = port;
    ctx->native_handler = lgfx_port_native_handler;

    return ctx;
}

REGISTER_PORT_DRIVER(lgfx_port, lgfx_port_init, lgfx_port_destroy, lgfx_port_create_port);
