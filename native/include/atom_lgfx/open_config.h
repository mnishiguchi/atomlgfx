/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>

#include "globalcontext.h"
#include "term.h"

#include "device.h"

#ifdef __cplusplus
extern "C" {
#endif

bool lgfx_parse_open_config_opts(
    GlobalContext *global,
    term opts,
    lgfx_open_config_overrides_t *out_overrides,
    const char **error_detail);

#ifdef __cplusplus
}
#endif
