// SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
//
// SPDX-License-Identifier: Apache-2.0

// lgfx_device/text.cpp

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

template <typename Fn>
static esp_err_t with_nul_terminated_text(
    const uint8_t *text,
    size_t text_len,
    bool allow_empty,
    Fn fn)
{
    if ((!allow_empty && text_len == 0) || (text_len > 0 && !text)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (text_len > (SIZE_MAX - 1u)) {
        return ESP_ERR_INVALID_SIZE;
    }

    char *buf = new (std::nothrow) char[text_len + 1u];
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }

    if (text_len > 0) {
        memcpy(buf, text, text_len);
    }
    buf[text_len] = '\0';

    esp_err_t err = fn(buf);

    delete[] buf;
    return err;
}

static esp_err_t set_jp_font_default(uint8_t target)
{
#if !defined(LGFX_PORT_ENABLE_JP_FONTS) || (LGFX_PORT_ENABLE_JP_FONTS != 1)
    (void) target;
    return ESP_ERR_NOT_SUPPORTED;
#else
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
        gfx->setFont(&ui_font_ja_16_min);
        gfx->setTextSize(1.0f);
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

extern "C" esp_err_t lgfx_device_set_text_font_preset(uint8_t target, uint8_t preset)
{
    switch (preset) {
        case LGFX_FONT_PRESET_ASCII:
            return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
                gfx->setTextFont(1);
                gfx->setTextSize(1.0f);
            });

        case LGFX_FONT_PRESET_JP:
            return set_jp_font_default(target);

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

extern "C" esp_err_t lgfx_device_set_cursor(uint8_t target, int16_t x, int16_t y)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->setCursor(x, y); });
}

extern "C" esp_err_t lgfx_device_get_cursor(uint8_t target, int32_t *out_x, int32_t *out_y)
{
    if (!out_x || !out_y) {
        return ESP_ERR_INVALID_ARG;
    }

    int32_t x = 0;
    int32_t y = 0;

    esp_err_t err = lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
        x = static_cast<int32_t>(gfx->getCursorX());
        y = static_cast<int32_t>(gfx->getCursorY());
    });

    if (err != ESP_OK) {
        return err;
    }

    *out_x = x;
    *out_y = y;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_draw_string(
    uint8_t target,
    int16_t x,
    int16_t y,
    const uint8_t *text,
    size_t text_len)
{
    return with_nul_terminated_text(
        text,
        text_len,
        false,
        [&](const char *buf) {
            return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->drawString(buf, x, y); });
        });
}

extern "C" esp_err_t lgfx_device_print(
    uint8_t target,
    const uint8_t *text,
    size_t text_len)
{
    return with_nul_terminated_text(
        text,
        text_len,
        true,
        [&](const char *buf) {
            return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->print(buf); });
        });
}

extern "C" esp_err_t lgfx_device_println(
    uint8_t target,
    const uint8_t *text,
    size_t text_len)
{
    return with_nul_terminated_text(
        text,
        text_len,
        true,
        [&](const char *buf) {
            return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { gfx->println(buf); });
        });
}
