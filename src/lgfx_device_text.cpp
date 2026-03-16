// src/lgfx_device_text.cpp

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <new>
#include <stddef.h>
#include <string.h>

#if defined(LGFX_PORT_ENABLE_JP_FONTS) && (LGFX_PORT_ENABLE_JP_FONTS == 1)
extern const lgfx::U8g2font ui_font_ja_16_min;
#endif

namespace
{

static inline bool lgfx_text_scale_x256_to_float(uint16_t scale_x256, float *out_scale)
{
    if (!out_scale || scale_x256 == 0) {
        return false;
    }

    *out_scale = static_cast<float>(scale_x256) / 256.0f;
    return true;
}

static esp_err_t set_jp_font_scaled(uint8_t target, uint16_t text_scale_x256)
{
#if !defined(LGFX_PORT_ENABLE_JP_FONTS) || (LGFX_PORT_ENABLE_JP_FONTS != 1)
    (void) target;
    (void) text_scale_x256;
    return ESP_ERR_NOT_SUPPORTED;
#else
    float text_scale = 0.0f;
    if (!lgfx_text_scale_x256_to_float(text_scale_x256, &text_scale)) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
        gfx->setFont(&ui_font_ja_16_min);
        gfx->setTextSize(text_scale);
    });
#endif
}

} // namespace

extern "C" esp_err_t lgfx_device_set_text_size(uint8_t target, uint16_t scale_x256)
{
    float scale = 0.0f;
    if (!lgfx_text_scale_x256_to_float(scale_x256, &scale)) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setTextSize(scale); });
}

extern "C" esp_err_t lgfx_device_set_text_size_xy(uint8_t target, uint16_t scale_x_x256, uint16_t scale_y_x256)
{
    float scale_x = 0.0f;
    float scale_y = 0.0f;
    if (!lgfx_text_scale_x256_to_float(scale_x_x256, &scale_x)
        || !lgfx_text_scale_x256_to_float(scale_y_x256, &scale_y)) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setTextSize(scale_x, scale_y); });
}

extern "C" esp_err_t lgfx_device_set_text_datum(uint8_t target, uint8_t datum)
{
    // Numeric passthrough. Protocol/domain validation is limited to u8.
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setTextDatum((textdatum_t) datum); });
}

extern "C" esp_err_t lgfx_device_set_text_wrap(uint8_t target, bool wrap_x, bool wrap_y)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setTextWrap(wrap_x, wrap_y); });
}

extern "C" esp_err_t lgfx_device_set_text_font(uint8_t target, uint8_t font)
{
    // Numeric passthrough. Protocol/domain validation is limited to u8.
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setTextFont(font); });
}

extern "C" esp_err_t lgfx_device_set_text_font_preset(uint8_t target, uint8_t preset)
{
    switch (preset) {
        case LGFX_FONT_PRESET_ASCII:
            return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
                gfx->setTextFont(1);
                gfx->setTextSize(1.0f);
            });

        case LGFX_FONT_PRESET_JP_SMALL:
            return set_jp_font_scaled(target, LGFX_TEXT_SCALE_JP_SMALL_X256);

        case LGFX_FONT_PRESET_JP_MEDIUM:
            return set_jp_font_scaled(target, LGFX_TEXT_SCALE_JP_MEDIUM_X256);

        case LGFX_FONT_PRESET_JP_LARGE:
            return set_jp_font_scaled(target, LGFX_TEXT_SCALE_JP_LARGE_X256);

        default:
            return ESP_ERR_INVALID_ARG;
    }
}

extern "C" esp_err_t lgfx_device_set_text_color(
    uint8_t target,
    bool fg_is_index,
    uint32_t fg_value,
    bool has_bg,
    bool bg_is_index,
    uint32_t bg_value)
{
    esp_err_t fg_validation_err = ESP_OK;
    esp_err_t bg_validation_err = ESP_OK;

    esp_err_t err = lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
        fg_validation_err = lgfx_dev::validate_target_scalar_color(target, gfx, fg_is_index, fg_value);
        if (fg_validation_err != ESP_OK) {
            return;
        }

        if (has_bg) {
            bg_validation_err = lgfx_dev::validate_target_scalar_color(target, gfx, bg_is_index, bg_value);
            if (bg_validation_err != ESP_OK) {
                return;
            }

            gfx->setTextColor(
                static_cast<uint32_t>(fg_value),
                static_cast<uint32_t>(bg_value));
        } else {
            gfx->setTextColor(static_cast<uint32_t>(fg_value));
        }
    });

    if (err != ESP_OK) {
        return err;
    }

    if (fg_validation_err != ESP_OK) {
        return fg_validation_err;
    }

    if (bg_validation_err != ESP_OK) {
        return bg_validation_err;
    }

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_draw_string(
    uint8_t target,
    int16_t x,
    int16_t y,
    const uint8_t *text,
    size_t text_len)
{
    if (!text || text_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (text_len > (SIZE_MAX - 1u)) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *buf = new (std::nothrow) char[text_len + 1u];
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }

    memcpy(buf, text, text_len);
    buf[text_len] = '\0';

    esp_err_t err = lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawString(buf, x, y); });

    delete[] buf;
    return err;
}
