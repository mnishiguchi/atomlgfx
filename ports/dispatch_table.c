// ports/dispatch_table.c
/*
 * Canonical op list: ports/include/lgfx_port/ops.def
 * Protocol contract: docs/LGFX_PORT_PROTOCOL.md
 */
#include "lgfx_port/dispatch_table.h"
#include "lgfx_port/op_handlers.h"

/*
 * Dispatch op -> handler.
 *
 * The caller (shared dispatch path in lgfx_port.c) is expected to:
 * - reject unknown ops (NULL return) as bad_op
 * - enforce shared per-op validation from ops.def metadata
 *   (arity / flags mask / target policy / init-state policy) before dispatch
 * Handlers then decode payload arguments and execute op-specific behavior.
 */
lgfx_handler_fn lgfx_dispatch_lookup(lgfx_port_t *port, term op)
{
    // Generated from ports/include/lgfx_port/ops.def
#define X(op_name, handler_fn, _atom_str, ...) \
    if (op == port->atoms.op_name) {           \
        return (handler_fn);                   \
    }
#include "lgfx_port/ops.def"
#undef X

    return NULL;
}
