// ports/include/lgfx_port/font_preset.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Stable protocol enum for setFontPreset/1 payload.
// Keep in sync with host-side mapping in examples/elixir/lib/sample_app/port.ex.
enum
{
    LGFX_FONT_PRESET_ASCII = 0,
    LGFX_FONT_PRESET_JP_SMALL = 1,
    LGFX_FONT_PRESET_JP_MEDIUM = 2,
    LGFX_FONT_PRESET_JP_LARGE = 3,
};

#ifdef __cplusplus
}
#endif
