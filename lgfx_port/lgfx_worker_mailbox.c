// lgfx_port/lgfx_worker_mailbox.c
//
// Port-thread mailbox drain:
// - Pull messages from the AtomVM mailbox
// - Forward NormalMessage payloads to the port handler
// - Dispose mailbox messages on the port thread
//
// This file is the only worker compilation unit that includes AtomVM headers.
// The worker task itself remains term-free.

#include <stdbool.h>

#include "lgfx_port/lgfx_port.h"
#include "lgfx_port/worker.h"

// AtomVM headers are used only by the port-thread mailbox drain path.
#include "context.h"
#include "mailbox.h"
#include "term.h"

// Port-thread callback implemented in ports/lgfx_port.c.
extern void lgfx_port_handle_mailbox_message(Context *ctx, lgfx_port_t *port, term msg);

/*
 * Drain the AtomVM mailbox on the port thread and forward each message term to
 * the port handler. Mailbox message ownership/disposal stays on the port thread.
 * The worker task never touches msg_term or mailbox objects.
 */
void lgfx_worker_drain_mailbox(lgfx_port_t *port)
{
    if (!port || !port->ctx) {
        return;
    }

    Context *ctx = port->ctx;
    Mailbox *mbox = &ctx->mailbox;
    Heap *heap = &ctx->heap;

    while (true) {
        MailboxMessage *m = mailbox_take_message(mbox);
        if (!m) {
            break;
        }

        if (m->type == NormalMessage) {
            Message *mm = (Message *) m;
            term msg_term = mm->message;

            lgfx_port_handle_mailbox_message(ctx, port, msg_term);
        }

        mailbox_message_dispose(m, heap);
    }
}
