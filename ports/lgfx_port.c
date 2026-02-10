// ports/lgfx_port.c
//
// AtomVM port driver entry point for the LovyanGFX port.
//
// Responsibilities in this file:
// - Per-context port creation and teardown
// - Mailbox message handling on the port thread
// - Request decode -> metadata validation -> dispatch -> reply flow
//
// Non-responsibilities:
// - Device calls (handled by lgfx_worker.c / lgfx_device.*)
// - AtomVM term decoding details (handled by term_decode.*)
// - Reply encoding helpers (handled by term_encode.*)

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "context.h"
#include "globalcontext.h"
#include "memory.h" // memory_ensure_free / MEMORY_GC_OK
#include "port.h" // port_parse_gen_message / port_send_reply
#include "portnifloader.h"

#include "lgfx_port/dispatch_table.h"
#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/op_meta.h"
#include "lgfx_port/term_decode.h"
#include "lgfx_port/term_encode.h"
#include "lgfx_port/validate.h"
#include "lgfx_port/worker.h"

/*
 * Ensure the reply term is valid.
 *
 * If a handler/validator returns an invalid term (typically due to OOM while
 * constructing a reply), try to replace it with {error, no_memory}.
 * If that allocation also fails, return invalid_term() and the caller will
 * skip sending a reply.
 */
static term ensure_valid_reply(Context *ctx, lgfx_port_t *port, term reply)
{
    if (!term_is_invalid_term(reply)) {
        return reply;
    }

    // Best effort: allocate {error, no_memory}.
    if (memory_ensure_free(ctx, /* tuple2 */ 3) != MEMORY_GC_OK) {
        return term_invalid_term();
    }

    return lgfx_reply_error(ctx, port, port->atoms.no_memory);
}

/*
 * Per-port-instance teardown.
 *
 * Important:
 * - REGISTER_PORT_DRIVER init/destroy callbacks are driver-global lifecycle hooks.
 * - lgfx_port_t is per AtomVM Context and is owned via ctx->platform_data.
 * - Therefore, per-port cleanup must happen from the native handler when the
 *   context is marked Killed.
 *
 * Teardown order:
 * 1) Detach ctx->platform_data first (double-teardown guard)
 * 2) Best-effort device close while worker is still running
 * 3) Reset local lifecycle/cache state
 * 4) Stop worker task/queue
 * 5) Free lgfx_port_t
 */
static void lgfx_port_teardown(Context *ctx)
{
    if (ctx == NULL) {
        return;
    }

    lgfx_port_t *port = (lgfx_port_t *) ctx->platform_data;
    if (port == NULL) {
        return;
    }

    // Guard against accidental double teardown.
    ctx->platform_data = NULL;

    /*
     * Best-effort device close before stopping the worker.
     *
     * lgfx_worker_device_close() runs through the worker queue, so the worker must
     * still be alive here. Ignore the return value during teardown: cleanup should
     * continue even if device close fails.
     */
    if (port->initialized) {
        (void) lgfx_worker_device_close(port);
    }

    // Final local state reset (defensive; port is freed immediately after).
    port->initialized = false;
    port->width = 0;
    port->height = 0;
    lgfx_last_error_clear(port);

    // Stop/free worker resources, then free the port state.
    lgfx_worker_stop(port);

    // Break back-reference before free (debug hygiene).
    port->ctx = NULL;

    free(port);
}

/*
 * Process one mailbox message term on the port thread.
 *
 * Ownership:
 * - The mailbox drainer owns MailboxMessage* and disposes it.
 * - This function only consumes the message term and must not retain it.
 * - This function must not dispose mailbox messages.
 */
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
         * Decode failed before dispatch.
         *
         * If decode_error is invalid, we likely failed to allocate an error reply,
         * so treat it as no_memory. Otherwise default to bad_proto, but prefer the
         * explicit reason if decode_error is already a structured {error, Reason}.
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

    // 1) Metadata lookup from the generated op registry (unknown op => bad_op).
    const lgfx_op_meta_t *meta = lgfx_op_meta_lookup(port, req.op);
    if (meta == NULL) {
        lgfx_last_error_set(port, req.op, port->atoms.bad_op, req.flags, req.target, 0);
        reply = lgfx_reply_error(ctx, port, port->atoms.bad_op);
        goto send_reply;
    }

    // 2) Shared validation driven by ops.def metadata.
    term pre = term_invalid_term();

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

    pre = lgfx_require_target_policy(ctx, port, &req, (lgfx_op_target_policy_t) meta->target_policy);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    pre = lgfx_require_state_policy(ctx, port, &req, (lgfx_op_state_policy_t) meta->state_policy);
    if (!term_is_invalid_term(pre)) {
        reply = pre;
        goto send_reply;
    }

    /*
     * 3) Dispatch lookup.
     *
     * This should always exist if ops.def metadata and the dispatch table stay in
     * sync. A miss here indicates an internal wiring mismatch.
     */
    lgfx_handler_fn handler = lgfx_dispatch_lookup(port, req.op);
    if (handler == NULL) {
        lgfx_last_error_set(port, req.op, port->atoms.internal, req.flags, req.target, 0);
        reply = lgfx_reply_error(ctx, port, port->atoms.internal);
        goto send_reply;
    }

    // 4) Execute handler (handlers may still perform op-specific validation/body).
    reply = handler(ctx, port, &req);

send_reply:
    /*
     * Normalize invalid replies to {error, no_memory} when possible.
     *
     * Handlers/validators may have already recorded a richer last_error (for example
     * an esp_err value). Keep that context where possible, but force reason=no_memory
     * if reply construction failed.
     */
    if (term_is_invalid_term(reply)) {
        if (port->last_error.last_op != req.op) {
            lgfx_last_error_set(port, req.op, port->atoms.no_memory, req.flags, req.target, 0);
        } else if (port->last_error.reason != port->atoms.no_memory) {
            lgfx_last_error_set(port, req.op, port->atoms.no_memory, req.flags, req.target, port->last_error.esp_err);
        }
        reply = ensure_valid_reply(ctx, port, reply);
    }

    if (!term_is_invalid_term(reply)) {
        port_send_reply(ctx, gen.pid, gen.ref, reply);
    }
}

static NativeHandlerResult lgfx_port_native_handler(Context *ctx)
{
    lgfx_port_t *port = (lgfx_port_t *) ctx->platform_data;
    if (port == NULL) {
        return NativeContinue;
    }

    /*
     * Per-context teardown path.
     *
     * AtomVM marks the Context as Killed before final destruction. Clean up the
     * worker task/queue and per-port state here, because REGISTER_PORT_DRIVER
     * destroy is driver-global and not called once per port instance.
     */
    if (ctx->flags & Killed) {
        lgfx_port_teardown(ctx);
        return NativeContinue;
    }

    // Normal path: port thread drains mailbox and processes messages.
    lgfx_worker_drain_mailbox(port);

    return NativeContinue;
}

static void lgfx_port_init(GlobalContext *global)
{
    (void) global;
}

static void lgfx_port_destroy(GlobalContext *global)
{
    /*
     * Driver-global teardown only.
     *
     * There is no per-port lgfx_port_t to free here. Per-context teardown is
     * handled in lgfx_port_native_handler() when the Context is marked Killed.
     */
    (void) global;
}

static Context *lgfx_port_create_port(GlobalContext *global, term opts)
{
    (void) opts;

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

    if (!lgfx_worker_start(port)) {
        free(port);
        context_destroy(ctx);
        return NULL;
    }

    ctx->platform_data = port;
    ctx->native_handler = lgfx_port_native_handler;

    return ctx;
}

REGISTER_PORT_DRIVER(lgfx_port, lgfx_port_init, lgfx_port_destroy, lgfx_port_create_port);
