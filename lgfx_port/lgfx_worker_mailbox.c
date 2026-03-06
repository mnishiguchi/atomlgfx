// lgfx_port/lgfx_worker_mailbox.c
// Port-thread mailbox drain.
// Mailbox ownership rules: see docs/WORKER_MODEL.md.

#include <stdbool.h>

#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_port/worker.h"

// AtomVM headers are used only by the port-thread mailbox drain path.
#include "context.h"
#include "mailbox.h"
#include "term.h"

// Port-thread callback implemented in ports/lgfx_port.c.
extern void lgfx_port_handle_mailbox_message(Context *ctx, lgfx_port_t *port, term msg);

/*
 * Drain the AtomVM mailbox on the port thread and forward each normal-message
 * term to the port handler.
 *
 * Mailbox message ownership and disposal stay on the port thread.
 * The worker task never touches mailbox objects or AtomVM terms.
 */
void lgfx_worker_drain_mailbox(lgfx_port_t *port)
{
    if (!port || !port->ctx) {
        return;
    }

    Context *ctx = port->ctx;
    Mailbox *mailbox = &ctx->mailbox;
    Heap *heap = &ctx->heap;

    while (true) {
        MailboxMessage *message = mailbox_take_message(mailbox);
        if (!message) {
            break;
        }

        if (message->type == NormalMessage) {
            Message *normal_message = (Message *) message;
            term message_term = normal_message->message;

            lgfx_port_handle_mailbox_message(ctx, port, message_term);
        }

        mailbox_message_dispose(message, heap);
    }
}
