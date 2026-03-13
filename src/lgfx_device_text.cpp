// src/lgfx_device_text.cpp

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <string.h>

#if defined(LGFX_PORT_ENABLE_JP_FONTS) && (LGFX_PORT_ENABLE_JP_FONTS == 1)
extern const lgfx::U8g2font ui_font_ja_16_min;
#endif

static esp_err_t set_jp_font_scaled(uint8_t target, uint8_t text_size)
{
#if !defined(LGFX_PORT_ENABLE_JP_FONTS) || (LGFX_PORT_ENABLE_JP_FONTS != 1)
    (void) target;
    (void) text_size;
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (text_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
        gfx->setFont(&ui_font_ja_16_min);
        gfx->setTextSize(text_size);
    });
#endif
}

extern "C" esp_err_t lgfx_device_set_text_size(uint8_t target, uint8_t size)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setTextSize(size); });
}

extern "C" esp_err_t lgfx_device_set_text_size_xy(uint8_t target, uint8_t sx, uint8_t sy)
{
    if (sx == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (sy == 0) {
        sy = sx;
    }

    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setTextSize(sx, sy); });
}

extern "C" esp_err_t lgfx_device_set_text_datum(uint8_t target, uint8_t datum)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setTextDatum((textdatum_t) datum); });
}

extern "C" esp_err_t lgfx_device_set_text_wrap(uint8_t target, bool wrap_x, bool wrap_y)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setTextWrap(wrap_x, wrap_y); });
}

extern "C" esp_err_t lgfx_device_set_text_font(uint8_t target, uint8_t font)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setTextFont(font); });
}

extern "C" esp_err_t lgfx_device_set_font_preset(uint8_t target, uint8_t preset)
{
    switch (preset) {
        case LGFX_FONT_PRESET_ASCII:
            return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
                gfx->setTextFont(1);
                gfx->setTextSize(1);
            });

        case LGFX_FONT_PRESET_JP_SMALL:
            return set_jp_font_scaled(target, 1);

        case LGFX_FONT_PRESET_JP_MEDIUM:
            return set_jp_font_scaled(target, 2);

        case LGFX_FONT_PRESET_JP_LARGE:
            return set_jp_font_scaled(target, 3);

        default:
            return ESP_ERR_INVALID_ARG;
    }
}

extern "C" esp_err_t lgfx_device_set_text_color(uint8_t target, uint16_t fg_rgb565, bool has_bg, uint16_t bg_rgb565)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
        if (has_bg) {
            gfx->setTextColor(fg_rgb565, bg_rgb565);
        } else {
            gfx->setTextColor(fg_rgb565);
        }
    });
}

extern "C" esp_err_t lgfx_device_draw_string(uint8_t target, int16_t x, int16_t y, const uint8_t *text, uint16_t text_len)
{
    if (!text || text_len == 0 || text_len > 255) {
        return ESP_ERR_INVALID_ARG;
    }

    char buf[256];
    memcpy(buf, text, (size_t) text_len);
    buf[text_len] = '\0';

    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawString(buf, x, y); });
}
