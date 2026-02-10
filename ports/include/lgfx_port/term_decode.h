#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t proto_ver;
    term op; // atom
    uint32_t target; // 0..254
    uint32_t flags; // u32
    term request_tuple; // original request tuple
    int arity; // tuple arity
} lgfx_request_t;

// Decode {lgfx, ProtoVer, Op, Target, Flags, ...}
bool lgfx_term_decode_request(
    Context *ctx,
    lgfx_port_t *port,
    term request,
    lgfx_request_t *out,
    term *out_error_reply);

#ifdef __cplusplus
}
#endif
