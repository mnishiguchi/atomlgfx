// ports/proto_caps.c
//
// Protocol-facing capability advertisement used by getCaps.
//
// caps.h is the single source of truth for protocol constants, build-time
// feature gates, and validation. This file only assembles the wire-facing
// FeatureBits/MaxSprites values from those definitions.
//
// Keep aligned with:
// - ports/include/lgfx_port/caps.h
// - ports/include/lgfx_port/ops.def
// - docs/LGFX_PORT_PROTOCOL.md

#include "lgfx_port/proto_caps.h"

#include "lgfx_port/caps.h"

uint32_t lgfx_proto_feature_bits(void)
{
    uint32_t bits = 0;

#if LGFX_PORT_SUPPORTS_SPRITE
    bits |= LGFX_CAP_SPRITE;
#endif

#if LGFX_PORT_SUPPORTS_PUSHIMAGE
    bits |= LGFX_CAP_PUSHIMAGE;
#endif

#if LGFX_PORT_SUPPORTS_JPG_FILE
    bits |= LGFX_CAP_JPG_FILE;
#endif

#if LGFX_PORT_SUPPORTS_PNG_FILE
    bits |= LGFX_CAP_PNG_FILE;
#endif

#if LGFX_PORT_SUPPORTS_LAST_ERROR
    bits |= LGFX_CAP_LAST_ERROR;
#endif

#if LGFX_PORT_SUPPORTS_BATCH_VOID
    bits |= LGFX_CAP_BATCH_VOID;
#endif

    // 0 or exactly one safe-yield bit is validated in caps.h.
    bits |= (uint32_t) LGFX_PORT_SAFE_YIELD_CAP;

    // Never expose unknown bits on the wire.
    return bits & (uint32_t) LGFX_CAP_KNOWN_MASK;
}

uint8_t lgfx_proto_max_sprites(void)
{
#if LGFX_PORT_SUPPORTS_SPRITE
    return (uint8_t) LGFX_PORT_MAX_SPRITES;
#else
    // Contract: MaxSprites must be 0 when CAP_SPRITE is not advertised.
    return 0;
#endif
}
