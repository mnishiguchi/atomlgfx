/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/atoms.c

#include "defaultatoms.h" // ATOM_STR
#include "globalcontext.h"

#include "lgfx_port/lgfx_port_internal.h"

#define LGFX_ATOM(global, len_bytes, atom_text) \
    globalcontext_make_atom((global), ATOM_STR(len_bytes, atom_text))

void lgfx_atoms_init(GlobalContext *global, lgfx_atoms_t *atoms)
{
    atoms->ok = globalcontext_make_atom(global, ATOM_STR("\x02", "ok"));
    atoms->error = globalcontext_make_atom(global, ATOM_STR("\x05", "error"));

    atoms->lgfx = globalcontext_make_atom(global, ATOM_STR("\x04", "lgfx"));

    atoms->pong = globalcontext_make_atom(global, ATOM_STR("\x04", "pong"));
    atoms->true_ = globalcontext_make_atom(global, ATOM_STR("\x04", "true"));
    atoms->false_ = globalcontext_make_atom(global, ATOM_STR("\x05", "false"));

    atoms->bad_proto = globalcontext_make_atom(global, ATOM_STR("\x09", "bad_proto"));
    atoms->bad_op = globalcontext_make_atom(global, ATOM_STR("\x06", "bad_op"));
    atoms->bad_flags = globalcontext_make_atom(global, ATOM_STR("\x09", "bad_flags"));
    atoms->bad_args = globalcontext_make_atom(global, ATOM_STR("\x08", "bad_args"));
    atoms->bad_target = globalcontext_make_atom(global, ATOM_STR("\x0A", "bad_target"));
    atoms->not_writing = globalcontext_make_atom(global, ATOM_STR("\x0B", "not_writing"));
    atoms->no_memory = globalcontext_make_atom(global, ATOM_STR("\x09", "no_memory"));
    atoms->internal = globalcontext_make_atom(global, ATOM_STR("\x08", "internal"));
    atoms->unsupported = globalcontext_make_atom(global, ATOM_STR("\x0B", "unsupported"));
    atoms->not_initialized = globalcontext_make_atom(global, ATOM_STR("\x0F", "not_initialized"));

    atoms->caps = globalcontext_make_atom(global, ATOM_STR("\x04", "caps"));
    atoms->last_error = globalcontext_make_atom(global, ATOM_STR("\x0A", "last_error"));
    atoms->none = globalcontext_make_atom(global, ATOM_STR("\x04", "none"));

    atoms->batch = globalcontext_make_atom(global, ATOM_STR("\x05", "batch"));
    atoms->batch_status = globalcontext_make_atom(global, ATOM_STR("\x0C", "batch_status"));
    atoms->batch_failure = globalcontext_make_atom(global, ATOM_STR("\x0D", "batch_failure"));

    atoms->idle = globalcontext_make_atom(global, ATOM_STR("\x04", "idle"));
    atoms->queued = globalcontext_make_atom(global, ATOM_STR("\x06", "queued"));
    atoms->running = globalcontext_make_atom(global, ATOM_STR("\x07", "running"));
    atoms->completed = globalcontext_make_atom(global, ATOM_STR("\x09", "completed"));
    atoms->failed = globalcontext_make_atom(global, ATOM_STR("\x06", "failed"));

    // Generated from lgfx_port/include_internal/lgfx_port/ops.def.
#define X(op, handler, atom_str, ...) atoms->op = globalcontext_make_atom(global, (atom_str));
#include "lgfx_port/ops.def"
#undef X
}
