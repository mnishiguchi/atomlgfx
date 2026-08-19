/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/handlers.c
//
// Ordinary operation handlers live in one translation unit. The section files
// are included here to keep the dispatch surface flat without making one very
// large source file hard to scan.
//
// Each handler should stay synchronous and wire-oriented:
// - decode only the operation-specific request payload
// - call the matching lgfx_device_* adapter function
// - return exactly one protocol reply
//
// Batch-specific decoding and command execution must stay behind the dedicated
// submitBinaryBatch handler and render_batch_dispatch.cpp.
//
// The direct AtomVM NIF bridge is included here as a second thin transport. It
// bypasses the port protocol but calls the same lgfx_device_* adapter functions
// and the same render-batch executor.

#include "lgfx_port/handlers/control.inc"
#include "lgfx_port/handlers/device.inc"
#include "lgfx_port/handlers/primitives.inc"
#include "lgfx_port/handlers/text.inc"
#include "lgfx_port/handlers/images.inc"
#include "lgfx_port/handlers/clip.inc"
#include "lgfx_port/handlers/sprites.inc"
#include "lgfx_port/handlers/touch.inc"
#include "lgfx_port/nif.inc"
