#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Canonical error atoms (protocol-level)
#define LGFX_ERR_BAD_PROTO "bad_proto"
#define LGFX_ERR_BAD_OP "bad_op"
#define LGFX_ERR_BAD_FLAGS "bad_flags"
#define LGFX_ERR_BAD_ARGS "bad_args"
#define LGFX_ERR_BAD_TARGET "bad_target"
#define LGFX_ERR_NOT_INITIALIZED "not_initialized"
#define LGFX_ERR_NOT_WRITING "not_writing"
#define LGFX_ERR_NO_MEMORY "no_memory"
#define LGFX_ERR_INTERNAL "internal"
#define LGFX_ERR_UNSUPPORTED "unsupported"

// Optional detail tuple reason tags
#define LGFX_ERR_BATCH_FAILED "batch_failed"

#ifdef __cplusplus
}
#endif
