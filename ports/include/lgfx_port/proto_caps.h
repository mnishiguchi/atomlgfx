// ports/include/lgfx_port/proto_caps.h
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns protocol-facing FeatureBits for getCaps (wire-safe, masked).
uint32_t lgfx_proto_feature_bits(void);

#ifdef __cplusplus
}
#endif
