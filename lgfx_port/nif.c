/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Keep the direct AtomVM NIF bridge in its own translation unit so firmware
// can omit the legacy port entrypoint and protocol handlers without affecting
// the NIF API, shared device adapter, or render-batch executor.

#include "lgfx_port/nif.inc"
