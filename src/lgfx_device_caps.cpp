// src/lgfx_device_caps.cpp
// Capability/feature discovery APIs (LCD-only semantics, callable before init).

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

extern "C" uint32_t lgfx_device_feature_bits(void)
{
    // Keep discovery callable even before lgfx_device_init().
    // Source of truth stays in lgfx_device_state.cpp via internal accessor.
    return lgfx_dev::feature_bits_const();
}

extern "C" uint32_t lgfx_device_max_sprites(void)
{
    return static_cast<uint32_t>(lgfx_dev::max_sprites_const());
}
