// lgfx_port/validate.h
#pragma once

#include <stdint.h>

#include "context.h"
#include "term.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lgfx_port_t lgfx_port_t;
typedef struct lgfx_request_t lgfx_request_t;

// Base envelope validation
term lgfx_require_proto_ver(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_require_target_domain(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);

// Arity validation
term lgfx_require_arity_exact(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, int expected_arity);
term lgfx_require_arity_range(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, int min_arity, int max_arity);

// Flags validation
term lgfx_require_flags_zero(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req);
term lgfx_require_flags_allowed_mask(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, uint32_t allowed_mask);

// Target + state validation (policy values are the encoded uint8 values from ops.def/meta)
term lgfx_require_target_policy(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, uint8_t policy);
term lgfx_require_state_policy(Context *ctx, lgfx_port_t *port, const lgfx_request_t *req, uint8_t policy);

#ifdef __cplusplus
}
#endif
