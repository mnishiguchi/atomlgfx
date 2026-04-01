/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_runtime/lgfx_command_dispatch.cpp

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"

#include "lgfx_device/lgfx_device_internal.hpp"
#include "lgfx_port/protocol.h"
#include "lgfx_runtime/lgfx_command_dispatch.h"

namespace
{

static inline bool lgfx_command_color_is_index(const lgfx_batch_command_t *command)
{
    return (command->flags & (uint32_t) LGFX_F_COLOR_INDEX) != 0u;
}

static inline bool lgfx_command_transparent_is_index(const lgfx_batch_command_t *command)
{
    return (command->flags & (uint32_t) LGFX_F_TRANSPARENT_INDEX) != 0u;
}

static inline bool lgfx_command_text_has_bg(const lgfx_batch_command_t *command)
{
    return (command->flags & (uint32_t) LGFX_F_TEXT_HAS_BG) != 0u;
}

static inline bool lgfx_command_text_fg_is_index(const lgfx_batch_command_t *command)
{
    return (command->flags & (uint32_t) LGFX_F_TEXT_FG_INDEX) != 0u;
}

static inline bool lgfx_command_text_bg_is_index(const lgfx_batch_command_t *command)
{
    return (command->flags & (uint32_t) LGFX_F_TEXT_BG_INDEX) != 0u;
}

static inline float lgfx_command_unpack_f32(uint32_t bits)
{
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static esp_err_t lgfx_command_dispatch_one_locked(const lgfx_batch_command_t *command)
{
    if (command == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t target = command->target;

    switch (command->op) {
        case LGFX_OP_fillScreen: {
            const bool color_is_index = lgfx_command_color_is_index(command);
            const uint32_t color_value = command->inline_args.words[0];
            return lgfx_dev::fill_screen_locked(target, color_is_index, color_value);
        }

        case LGFX_OP_fillRect: {
            const int16_t x = (int16_t) (int32_t) command->inline_args.words[0];
            const int16_t y = (int16_t) (int32_t) command->inline_args.words[1];
            const uint16_t w = (uint16_t) command->inline_args.words[2];
            const uint16_t h = (uint16_t) command->inline_args.words[3];
            const bool color_is_index = lgfx_command_color_is_index(command);
            const uint32_t color_value = command->inline_args.words[4];

            return lgfx_dev::fill_rect_locked(target, x, y, w, h, color_is_index, color_value);
        }

        case LGFX_OP_drawLine: {
            const int16_t x0 = (int16_t) (int32_t) command->inline_args.words[0];
            const int16_t y0 = (int16_t) (int32_t) command->inline_args.words[1];
            const int16_t x1 = (int16_t) (int32_t) command->inline_args.words[2];
            const int16_t y1 = (int16_t) (int32_t) command->inline_args.words[3];
            const bool color_is_index = lgfx_command_color_is_index(command);
            const uint32_t color_value = command->inline_args.words[4];

            return lgfx_dev::draw_line_locked(target, x0, y0, x1, y1, color_is_index, color_value);
        }

        case LGFX_OP_setClipRect: {
            const int16_t x = (int16_t) (int32_t) command->inline_args.words[0];
            const int16_t y = (int16_t) (int32_t) command->inline_args.words[1];
            const uint16_t w = (uint16_t) command->inline_args.words[2];
            const uint16_t h = (uint16_t) command->inline_args.words[3];

            return lgfx_dev::set_clip_rect_locked(target, x, y, w, h);
        }

        case LGFX_OP_clearClipRect:
            return lgfx_dev::clear_clip_rect_locked(target);

        case LGFX_OP_setTextSize: {
            const bool use_xy = command->inline_args.words[0] != 0u;
            const float scale_x = lgfx_command_unpack_f32(command->inline_args.words[1]);
            const float scale_y = lgfx_command_unpack_f32(command->inline_args.words[2]);

            return lgfx_dev::set_text_size_locked(
                target,
                scale_x,
                use_xy ? scale_y : scale_x);
        }

        case LGFX_OP_setTextDatum: {
            const uint8_t datum = (uint8_t) command->inline_args.words[0];
            return lgfx_dev::set_text_datum_locked(target, datum);
        }

        case LGFX_OP_setTextWrap: {
            const bool wrap_x = command->inline_args.words[0] != 0u;
            const bool wrap_y = command->inline_args.words[1] != 0u;
            return lgfx_dev::set_text_wrap_locked(target, wrap_x, wrap_y);
        }

        case LGFX_OP_setTextFontPreset: {
            const lgfx_font_preset_t preset = (lgfx_font_preset_t) ((uint8_t) command->inline_args.words[0]);
            return lgfx_dev::set_text_font_preset_locked(target, preset);
        }

        case LGFX_OP_setTextColor: {
            const bool fg_is_index = lgfx_command_text_fg_is_index(command);
            const bool has_bg = lgfx_command_text_has_bg(command);
            const bool bg_is_index = lgfx_command_text_bg_is_index(command);
            const uint32_t fg_value = command->inline_args.words[0];
            const uint32_t bg_value = command->inline_args.words[1];

            return lgfx_dev::set_text_color_locked(
                target,
                fg_is_index,
                fg_value,
                has_bg,
                bg_is_index,
                bg_value);
        }

        case LGFX_OP_setCursor: {
            const int16_t x = (int16_t) (int32_t) command->inline_args.words[0];
            const int16_t y = (int16_t) (int32_t) command->inline_args.words[1];
            return lgfx_dev::set_cursor_locked(target, x, y);
        }

        case LGFX_OP_pushSprite: {
            const uint8_t dst_target = (uint8_t) command->inline_args.words[0];
            const int16_t x = (int16_t) (int32_t) command->inline_args.words[1];
            const int16_t y = (int16_t) (int32_t) command->inline_args.words[2];
            const bool has_transparent = command->inline_args.words[3] != 0u;
            const bool transparent_is_index = lgfx_command_transparent_is_index(command);
            const uint32_t transparent_value = command->inline_args.words[4];

            return lgfx_dev::push_sprite_locked(
                target,
                dst_target,
                x,
                y,
                has_transparent,
                transparent_is_index,
                transparent_value);
        }

        case LGFX_OP_pushRotateZoom: {
            const uint8_t dst_target = (uint8_t) command->inline_args.words[0];
            const int16_t x = (int16_t) (int32_t) command->inline_args.words[1];
            const int16_t y = (int16_t) (int32_t) command->inline_args.words[2];
            const float angle = lgfx_command_unpack_f32(command->inline_args.words[3]);
            const float zoom_x = lgfx_command_unpack_f32(command->inline_args.words[4]);
            const float zoom_y = lgfx_command_unpack_f32(command->inline_args.words[5]);
            const bool has_transparent = command->inline_args.words[6] != 0u;
            const bool transparent_is_index = lgfx_command_transparent_is_index(command);
            const uint32_t transparent_value = command->inline_args.words[7];

            return lgfx_dev::push_rotate_zoom_locked(
                target,
                dst_target,
                x,
                y,
                angle,
                zoom_x,
                zoom_y,
                has_transparent,
                transparent_is_index,
                transparent_value);
        }

        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

} // namespace

extern "C" void lgfx_dispatch_batch_result_clear(lgfx_dispatch_batch_result_t *result)
{
    if (result == nullptr) {
        return;
    }

    result->has_failure = false;
    result->failed_index = 0u;
    result->failed_esp_err = ESP_OK;
}

extern "C" bool lgfx_command_dispatch_is_supported(lgfx_op_t op)
{
    switch (op) {
        case LGFX_OP_fillScreen:
        case LGFX_OP_fillRect:
        case LGFX_OP_drawLine:
        case LGFX_OP_setClipRect:
        case LGFX_OP_clearClipRect:
        case LGFX_OP_setTextSize:
        case LGFX_OP_setTextDatum:
        case LGFX_OP_setTextWrap:
        case LGFX_OP_setTextFontPreset:
        case LGFX_OP_setTextColor:
        case LGFX_OP_setCursor:
        case LGFX_OP_pushSprite:
        case LGFX_OP_pushRotateZoom:
            return true;

        default:
            return false;
    }
}

extern "C" esp_err_t lgfx_command_dispatch_one(const lgfx_batch_command_t *command)
{
    if (command == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lgfx_command_dispatch_is_supported(command->op)) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    return lgfx_command_dispatch_one_locked(command);
}

extern "C" esp_err_t lgfx_command_dispatch_run_batch(
    const lgfx_batch_command_t *commands,
    size_t command_count,
    lgfx_dispatch_batch_result_t *out_result)
{
    lgfx_dispatch_batch_result_clear(out_result);

    if (command_count > 0u && commands == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (command_count == 0u) {
        return ESP_OK;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    err = lgfx_dev::start_write_locked();
    if (err != ESP_OK) {
        return err;
    }

    esp_err_t run_err = ESP_OK;

    for (size_t i = 0; i < command_count; i++) {
        const lgfx_batch_command_t *command = &commands[i];

        if (!lgfx_command_dispatch_is_supported(command->op)) {
            run_err = ESP_ERR_NOT_SUPPORTED;
        } else {
            run_err = lgfx_command_dispatch_one_locked(command);
        }

        if (run_err != ESP_OK) {
            if (out_result != nullptr) {
                out_result->has_failure = true;
                out_result->failed_index = (uint32_t) i;
                out_result->failed_esp_err = run_err;
            }
            break;
        }
    }

    const esp_err_t end_err = lgfx_dev::end_write_locked();

    if (run_err != ESP_OK) {
        return run_err;
    }

    if (end_err != ESP_OK) {
        return end_err;
    }

    return ESP_OK;
}
