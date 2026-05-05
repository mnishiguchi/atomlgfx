/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/render_batch_dispatch.cpp

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"

#include "lgfx_device/lgfx_device_internal.hpp"
#include "lgfx_port/lgfx_port_config.h"

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
#include "esp_timer.h"
#endif
#include "lgfx_port/ops.h"
#include "lgfx_port/protocol.h"
#include "lgfx_port/render_batch_dispatch.h"

#ifndef LGFX_PORT_RENDER_BATCH_PREVALIDATE
#define LGFX_PORT_RENDER_BATCH_PREVALIDATE 0
#endif

#if (LGFX_PORT_RENDER_BATCH_PREVALIDATE != 0) && (LGFX_PORT_RENDER_BATCH_PREVALIDATE != 1)
#error "LGFX_PORT_RENDER_BATCH_PREVALIDATE must be 0 or 1"
#endif

static_assert(sizeof(float) == 4u, "render batch f32 fields require 32-bit float");
static_assert(LGFX_OP_COUNT <= 240, "render batch protocol opcodes must not overlap private render opcodes");
static_assert(LGFX_RENDER_OP_TARGET >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_COLOR_MODE >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_PUSH_SPRITE_TRANSPARENT >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_PUSH_SPRITE_LIST >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_PUSH_SPRITE_REGION_LIST >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_BEGIN_STRIP >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_PRESENT_STRIP >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_FILL_RECT_LIST >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_DRAW_LINE_LIST >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_DRAW_PIXEL_LIST >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_DRAW_RECT_LIST >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_FILL_CIRCLE_LIST >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_DRAW_CIRCLE_LIST >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_FILL_TRIANGLE_LIST >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_DRAW_TRIANGLE_LIST >= 240, "render-private opcodes must stay above protocol opcodes");
static_assert(LGFX_RENDER_OP_EXTENDED >= 240, "render-private opcodes must stay above protocol opcodes");

namespace
{

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
static constexpr const char *LOG_TAG = "lgfx_render_batch";
#endif

static constexpr uint8_t PRZL_OPTION_HAS_TRANSPARENT = 0x01u;
static constexpr uint8_t PRZL_OPTION_APPROX_CULL = 0x02u;
static constexpr uint16_t SPRITE_LIST_FLAG_HAS_TRANSPARENT = 0x0001u;
static constexpr uint16_t SPRITE_LIST_ALLOWED_FLAGS = SPRITE_LIST_FLAG_HAS_TRANSPARENT | LGFX_F_TRANSPARENT_INDEX;
static constexpr uint16_t SPRITE_REGION_LIST_FLAG_HAS_TRANSPARENT = 0x0001u;
static constexpr uint16_t SPRITE_REGION_LIST_ALLOWED_FLAGS = SPRITE_REGION_LIST_FLAG_HAS_TRANSPARENT;
static constexpr size_t SPRITE_LIST_RECORD_SIZE = LGFX_DEVICE_SPRITE_PUSH_RECORD_SIZE;
static constexpr size_t SPRITE_REGION_LIST_RECORD_SIZE = LGFX_DEVICE_SPRITE_REGION_RECORD_SIZE;
static constexpr size_t PRZL_HEADER_SIZE = 12u;
static constexpr size_t PRZL_RECORD_SIZE = LGFX_DEVICE_SPRITE_TRANSFORM_RECORD_SIZE;
static constexpr uint8_t PRZF_OPTION_HAS_TRANSPARENT = 0x01u;
static constexpr uint8_t PRZF_OPTION_APPROX_CULL = 0x02u;
static constexpr size_t PRZF_HEADER_SIZE = 14u;
static constexpr uint16_t TEXT_ALLOWED_FLAGS =
    LGFX_F_TEXT_HAS_BG | LGFX_F_TEXT_FG_INDEX | LGFX_F_TEXT_BG_INDEX;
static constexpr size_t FILL_RECT_LIST_RECORD_SIZE = 10u;
static constexpr size_t DRAW_LINE_LIST_RECORD_SIZE = 10u;
static constexpr size_t DRAW_PIXEL_LIST_RECORD_SIZE = 6u;
static constexpr size_t DRAW_RECT_LIST_RECORD_SIZE = 10u;
static constexpr size_t FILL_CIRCLE_LIST_RECORD_SIZE = 8u;
static constexpr size_t DRAW_CIRCLE_LIST_RECORD_SIZE = 8u;
static constexpr size_t ELLIPSE_LIST_RECORD_SIZE = 10u;
static constexpr size_t FILL_TRIANGLE_LIST_RECORD_SIZE = 14u;
static constexpr size_t DRAW_TRIANGLE_LIST_RECORD_SIZE = 14u;

enum lgfx_render_batch_mode_t
{
    LGFX_RENDER_BATCH_VALIDATE_ONLY,
    LGFX_RENDER_BATCH_EXECUTE
};

struct lgfx_render_batch_state_t
{
    uint8_t target = 0u;
    bool color_is_index = false;
    bool strip_active = false;
};

struct lgfx_render_batch_trace_t
{
    uint32_t command_count = 0u;
    uint32_t target_count = 0u;
    uint32_t color_mode_count = 0u;
    uint32_t scalar_count = 0u;
    uint32_t draw_pixel_list_count = 0u;
    uint32_t draw_pixel_list_instance_count = 0u;
    uint32_t draw_rect_list_count = 0u;
    uint32_t draw_rect_list_instance_count = 0u;
    uint32_t fill_rect_list_count = 0u;
    uint32_t fill_rect_list_instance_count = 0u;
    uint32_t draw_circle_list_count = 0u;
    uint32_t draw_circle_list_instance_count = 0u;
    uint32_t fill_circle_list_count = 0u;
    uint32_t fill_circle_list_instance_count = 0u;
    uint32_t draw_ellipse_list_count = 0u;
    uint32_t draw_ellipse_list_instance_count = 0u;
    uint32_t fill_ellipse_list_count = 0u;
    uint32_t fill_ellipse_list_instance_count = 0u;
    uint32_t draw_line_list_count = 0u;
    uint32_t draw_line_list_instance_count = 0u;
    uint32_t draw_triangle_list_count = 0u;
    uint32_t draw_triangle_list_instance_count = 0u;
    uint32_t fill_triangle_list_count = 0u;
    uint32_t fill_triangle_list_instance_count = 0u;
    uint32_t clip_command_count = 0u;
    uint32_t text_command_count = 0u;
    uint32_t image_command_count = 0u;
    uint32_t sprite_push_count = 0u;
    uint32_t sprite_push_list_count = 0u;
    uint32_t sprite_push_list_instance_count = 0u;
    uint32_t sprite_region_list_count = 0u;
    uint32_t sprite_region_list_instance_count = 0u;
    uint32_t przl_command_count = 0u;
    uint32_t przl_instance_count = 0u;
    uint32_t przl_executed_count = 0u;
    uint32_t przl_culled_count = 0u;
    uint32_t przf_command_count = 0u;
    uint32_t przf_instance_count = 0u;
    uint32_t przf_executed_count = 0u;
    uint32_t przf_culled_count = 0u;
    uint32_t strip_begin_count = 0u;
    uint32_t strip_present_count = 0u;
    uint32_t display_count = 0u;
    int64_t frame_clear_us = 0;
    int64_t frame_draw_us = 0;
    int64_t frame_present_us = 0;
    int64_t display_us = 0;
};

static inline esp_err_t lgfx_render_batch_malformed(bool *out_malformed_command)
{
    if (out_malformed_command) {
        *out_malformed_command = true;
    }

    return ESP_ERR_INVALID_ARG;
}

static inline void lgfx_render_batch_init_result(
    uint32_t *out_failed_index,
    uint8_t *out_failed_opcode,
    bool *out_malformed_command)
{
    if (out_failed_index) {
        *out_failed_index = 0u;
    }
    if (out_failed_opcode) {
        *out_failed_opcode = 0u;
    }
    if (out_malformed_command) {
        *out_malformed_command = false;
    }
}

static inline uint16_t lgfx_render_batch_read_le_u16(const uint8_t *cursor)
{
    return (uint16_t) cursor[0] | ((uint16_t) cursor[1] << 8);
}

static inline int16_t lgfx_render_batch_read_le_i16(const uint8_t *cursor)
{
    return (int16_t) lgfx_render_batch_read_le_u16(cursor);
}

static bool lgfx_render_batch_take_u8(const uint8_t **cursor, const uint8_t *end, uint8_t *out)
{
    if (*cursor > end || (size_t) (end - *cursor) < 1u) {
        return false;
    }

    *out = **cursor;
    *cursor += 1;
    return true;
}

static bool lgfx_render_batch_take_u16(const uint8_t **cursor, const uint8_t *end, uint16_t *out)
{
    if (*cursor > end || (size_t) (end - *cursor) < 2u) {
        return false;
    }

    *out = lgfx_render_batch_read_le_u16(*cursor);
    *cursor += 2;
    return true;
}

static bool lgfx_render_batch_take_u32(const uint8_t **cursor, const uint8_t *end, uint32_t *out)
{
    if (*cursor > end || (size_t) (end - *cursor) < 4u) {
        return false;
    }

    const uint8_t *bytes = *cursor;
    *out = ((uint32_t) bytes[0])
        | (((uint32_t) bytes[1]) << 8)
        | (((uint32_t) bytes[2]) << 16)
        | (((uint32_t) bytes[3]) << 24);
    *cursor += 4;
    return true;
}

static bool lgfx_render_batch_take_i16(const uint8_t **cursor, const uint8_t *end, int16_t *out)
{
    uint16_t value = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &value)) {
        return false;
    }

    *out = (int16_t) value;
    return true;
}


static bool lgfx_render_batch_take_f32(const uint8_t **cursor, const uint8_t *end, float *out)
{
    if (*cursor > end || (size_t) (end - *cursor) < sizeof(float)) {
        return false;
    }

    const uint8_t *bytes = *cursor;
    uint32_t le_value = ((uint32_t) bytes[0])
        | (((uint32_t) bytes[1]) << 8)
        | (((uint32_t) bytes[2]) << 16)
        | (((uint32_t) bytes[3]) << 24);

    memcpy(out, &le_value, sizeof(le_value));
    *cursor += sizeof(float);
    return true;
}

static bool lgfx_render_batch_take_bytes(
    const uint8_t **cursor,
    const uint8_t *end,
    size_t len,
    const uint8_t **out)
{
    if (*cursor > end || (size_t) (end - *cursor) < len) {
        return false;
    }

    *out = *cursor;
    *cursor += len;
    return true;
}

static bool lgfx_render_batch_contains_nul(const uint8_t *bytes, size_t len)
{
    if (!bytes) {
        return false;
    }

    for (size_t i = 0; i < len; i++) {
        if (bytes[i] == 0u) {
            return true;
        }
    }

    return false;
}

static esp_err_t lgfx_render_batch_display_locked()
{
    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = lgfx_dev::presentation_present_locked();
    if (err != ESP_OK) {
        return err;
    }

    lcd->display();
    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_draw_pixel_list(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t flags = 0u;
    uint16_t count = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)
        || !lgfx_render_batch_take_u16(cursor, end, &count)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (flags != 0u || count == 0u || count > (SIZE_MAX / DRAW_PIXEL_LIST_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * DRAW_PIXEL_LIST_RECORD_SIZE;
    const uint8_t *records = nullptr;
    if (!lgfx_render_batch_take_bytes(cursor, end, records_len, &records)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        return ESP_OK;
    }

    if (trace) {
        trace->draw_pixel_list_count++;
        trace->draw_pixel_list_instance_count += count;
        trace->scalar_count += count;
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * DRAW_PIXEL_LIST_RECORD_SIZE);
        const int16_t x = lgfx_render_batch_read_le_i16(record);
        const int16_t y = lgfx_render_batch_read_le_i16(record + 2);
        const uint16_t color = lgfx_render_batch_read_le_u16(record + 4);

        const esp_err_t err = lgfx_dev::draw_pixel_locked(
            state->target,
            x,
            y,
            state->color_is_index,
            (uint32_t) color);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_draw_rect_list(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t flags = 0u;
    uint16_t count = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)
        || !lgfx_render_batch_take_u16(cursor, end, &count)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (flags != 0u || count == 0u || count > (SIZE_MAX / DRAW_RECT_LIST_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * DRAW_RECT_LIST_RECORD_SIZE;
    const uint8_t *records = nullptr;
    if (!lgfx_render_batch_take_bytes(cursor, end, records_len, &records)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * DRAW_RECT_LIST_RECORD_SIZE);
        const uint16_t w = lgfx_render_batch_read_le_u16(record + 4);
        const uint16_t h = lgfx_render_batch_read_le_u16(record + 6);

        if (w == 0u || h == 0u) {
            return lgfx_render_batch_malformed(out_malformed_command);
        }
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        return ESP_OK;
    }

    if (trace) {
        trace->draw_rect_list_count++;
        trace->draw_rect_list_instance_count += count;
        trace->scalar_count += count;
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * DRAW_RECT_LIST_RECORD_SIZE);
        const int16_t x = lgfx_render_batch_read_le_i16(record);
        const int16_t y = lgfx_render_batch_read_le_i16(record + 2);
        const uint16_t w = lgfx_render_batch_read_le_u16(record + 4);
        const uint16_t h = lgfx_render_batch_read_le_u16(record + 6);
        const uint16_t color = lgfx_render_batch_read_le_u16(record + 8);

        const esp_err_t err = lgfx_dev::draw_rect_locked(
            state->target,
            x,
            y,
            w,
            h,
            state->color_is_index,
            (uint32_t) color);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_fill_rect_list(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t flags = 0u;
    uint16_t count = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)
        || !lgfx_render_batch_take_u16(cursor, end, &count)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (flags != 0u || count == 0u || count > (SIZE_MAX / FILL_RECT_LIST_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * FILL_RECT_LIST_RECORD_SIZE;
    const uint8_t *records = nullptr;
    if (!lgfx_render_batch_take_bytes(cursor, end, records_len, &records)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * FILL_RECT_LIST_RECORD_SIZE);
        const uint16_t w = lgfx_render_batch_read_le_u16(record + 4);
        const uint16_t h = lgfx_render_batch_read_le_u16(record + 6);

        if (w == 0u || h == 0u) {
            return lgfx_render_batch_malformed(out_malformed_command);
        }
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        return ESP_OK;
    }

    if (trace) {
        trace->fill_rect_list_count++;
        trace->fill_rect_list_instance_count += count;
        trace->scalar_count += count;
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * FILL_RECT_LIST_RECORD_SIZE);
        const int16_t x = lgfx_render_batch_read_le_i16(record);
        const int16_t y = lgfx_render_batch_read_le_i16(record + 2);
        const uint16_t w = lgfx_render_batch_read_le_u16(record + 4);
        const uint16_t h = lgfx_render_batch_read_le_u16(record + 6);
        const uint16_t color = lgfx_render_batch_read_le_u16(record + 8);

        const esp_err_t err = lgfx_dev::fill_rect_locked(
            state->target,
            x,
            y,
            w,
            h,
            state->color_is_index,
            (uint32_t) color);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_draw_circle_list(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t flags = 0u;
    uint16_t count = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)
        || !lgfx_render_batch_take_u16(cursor, end, &count)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (flags != 0u || count == 0u || count > (SIZE_MAX / DRAW_CIRCLE_LIST_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * DRAW_CIRCLE_LIST_RECORD_SIZE;
    const uint8_t *records = nullptr;
    if (!lgfx_render_batch_take_bytes(cursor, end, records_len, &records)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * DRAW_CIRCLE_LIST_RECORD_SIZE);
        const uint16_t radius = lgfx_render_batch_read_le_u16(record + 4);

        if (radius == 0u) {
            return lgfx_render_batch_malformed(out_malformed_command);
        }
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        return ESP_OK;
    }

    if (trace) {
        trace->draw_circle_list_count++;
        trace->draw_circle_list_instance_count += count;
        trace->scalar_count += count;
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * DRAW_CIRCLE_LIST_RECORD_SIZE);
        const int16_t x = lgfx_render_batch_read_le_i16(record);
        const int16_t y = lgfx_render_batch_read_le_i16(record + 2);
        const uint16_t radius = lgfx_render_batch_read_le_u16(record + 4);
        const uint16_t color = lgfx_render_batch_read_le_u16(record + 6);

        const esp_err_t err = lgfx_dev::draw_circle_locked(
            state->target,
            x,
            y,
            radius,
            state->color_is_index,
            (uint32_t) color);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_fill_circle_list(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t flags = 0u;
    uint16_t count = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)
        || !lgfx_render_batch_take_u16(cursor, end, &count)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (flags != 0u || count == 0u || count > (SIZE_MAX / FILL_CIRCLE_LIST_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * FILL_CIRCLE_LIST_RECORD_SIZE;
    const uint8_t *records = nullptr;
    if (!lgfx_render_batch_take_bytes(cursor, end, records_len, &records)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * FILL_CIRCLE_LIST_RECORD_SIZE);
        const uint16_t radius = lgfx_render_batch_read_le_u16(record + 4);

        if (radius == 0u) {
            return lgfx_render_batch_malformed(out_malformed_command);
        }
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        return ESP_OK;
    }

    if (trace) {
        trace->fill_circle_list_count++;
        trace->fill_circle_list_instance_count += count;
        trace->scalar_count += count;
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * FILL_CIRCLE_LIST_RECORD_SIZE);
        const int16_t x = lgfx_render_batch_read_le_i16(record);
        const int16_t y = lgfx_render_batch_read_le_i16(record + 2);
        const uint16_t radius = lgfx_render_batch_read_le_u16(record + 4);
        const uint16_t color = lgfx_render_batch_read_le_u16(record + 6);

        const esp_err_t err = lgfx_dev::fill_circle_locked(
            state->target,
            x,
            y,
            radius,
            state->color_is_index,
            (uint32_t) color);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_ellipse_list(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t kind = 0u;
    uint8_t reserved = 0u;
    uint16_t flags = 0u;
    uint16_t count = 0u;
    if (!lgfx_render_batch_take_u8(cursor, end, &kind)
        || !lgfx_render_batch_take_u8(cursor, end, &reserved)
        || !lgfx_render_batch_take_u16(cursor, end, &flags)
        || !lgfx_render_batch_take_u16(cursor, end, &count)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if ((kind != LGFX_RENDER_ELLIPSE_LIST_KIND_DRAW && kind != LGFX_RENDER_ELLIPSE_LIST_KIND_FILL)
        || reserved != 0u
        || flags != 0u
        || count == 0u
        || count > (SIZE_MAX / ELLIPSE_LIST_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * ELLIPSE_LIST_RECORD_SIZE;
    const uint8_t *records = nullptr;
    if (!lgfx_render_batch_take_bytes(cursor, end, records_len, &records)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * ELLIPSE_LIST_RECORD_SIZE);
        const uint16_t rx = lgfx_render_batch_read_le_u16(record + 4);
        const uint16_t ry = lgfx_render_batch_read_le_u16(record + 6);

        if (rx == 0u || ry == 0u) {
            return lgfx_render_batch_malformed(out_malformed_command);
        }
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        return ESP_OK;
    }

    if (trace) {
        if (kind == LGFX_RENDER_ELLIPSE_LIST_KIND_DRAW) {
            trace->draw_ellipse_list_count++;
            trace->draw_ellipse_list_instance_count += count;
        } else {
            trace->fill_ellipse_list_count++;
            trace->fill_ellipse_list_instance_count += count;
        }
        trace->scalar_count += count;
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * ELLIPSE_LIST_RECORD_SIZE);
        const int16_t x = lgfx_render_batch_read_le_i16(record);
        const int16_t y = lgfx_render_batch_read_le_i16(record + 2);
        const uint16_t rx = lgfx_render_batch_read_le_u16(record + 4);
        const uint16_t ry = lgfx_render_batch_read_le_u16(record + 6);
        const uint16_t color = lgfx_render_batch_read_le_u16(record + 8);

        const esp_err_t err = kind == LGFX_RENDER_ELLIPSE_LIST_KIND_DRAW
            ? lgfx_dev::draw_ellipse_locked(state->target, x, y, rx, ry, state->color_is_index, (uint32_t) color)
            : lgfx_dev::fill_ellipse_locked(state->target, x, y, rx, ry, state->color_is_index, (uint32_t) color);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_draw_line_list(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t flags = 0u;
    uint16_t count = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)
        || !lgfx_render_batch_take_u16(cursor, end, &count)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (flags != 0u || count == 0u || count > (SIZE_MAX / DRAW_LINE_LIST_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * DRAW_LINE_LIST_RECORD_SIZE;
    const uint8_t *records = nullptr;
    if (!lgfx_render_batch_take_bytes(cursor, end, records_len, &records)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        return ESP_OK;
    }

    if (trace) {
        trace->draw_line_list_count++;
        trace->draw_line_list_instance_count += count;
        trace->scalar_count += count;
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * DRAW_LINE_LIST_RECORD_SIZE);
        const int16_t x0 = lgfx_render_batch_read_le_i16(record);
        const int16_t y0 = lgfx_render_batch_read_le_i16(record + 2);
        const int16_t x1 = lgfx_render_batch_read_le_i16(record + 4);
        const int16_t y1 = lgfx_render_batch_read_le_i16(record + 6);
        const uint16_t color = lgfx_render_batch_read_le_u16(record + 8);

        const esp_err_t err = lgfx_dev::draw_line_locked(
            state->target,
            x0,
            y0,
            x1,
            y1,
            state->color_is_index,
            (uint32_t) color);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_draw_triangle_list(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t flags = 0u;
    uint16_t count = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)
        || !lgfx_render_batch_take_u16(cursor, end, &count)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (flags != 0u || count == 0u || count > (SIZE_MAX / DRAW_TRIANGLE_LIST_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * DRAW_TRIANGLE_LIST_RECORD_SIZE;
    const uint8_t *records = nullptr;
    if (!lgfx_render_batch_take_bytes(cursor, end, records_len, &records)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        return ESP_OK;
    }

    if (trace) {
        trace->draw_triangle_list_count++;
        trace->draw_triangle_list_instance_count += count;
        trace->scalar_count += count;
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * DRAW_TRIANGLE_LIST_RECORD_SIZE);
        const int16_t x0 = lgfx_render_batch_read_le_i16(record);
        const int16_t y0 = lgfx_render_batch_read_le_i16(record + 2);
        const int16_t x1 = lgfx_render_batch_read_le_i16(record + 4);
        const int16_t y1 = lgfx_render_batch_read_le_i16(record + 6);
        const int16_t x2 = lgfx_render_batch_read_le_i16(record + 8);
        const int16_t y2 = lgfx_render_batch_read_le_i16(record + 10);
        const uint16_t color = lgfx_render_batch_read_le_u16(record + 12);

        const esp_err_t err = lgfx_dev::draw_triangle_locked(
            state->target,
            x0,
            y0,
            x1,
            y1,
            x2,
            y2,
            state->color_is_index,
            (uint32_t) color);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_fill_triangle_list(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t flags = 0u;
    uint16_t count = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)
        || !lgfx_render_batch_take_u16(cursor, end, &count)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (flags != 0u || count == 0u || count > (SIZE_MAX / FILL_TRIANGLE_LIST_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * FILL_TRIANGLE_LIST_RECORD_SIZE;
    const uint8_t *records = nullptr;
    if (!lgfx_render_batch_take_bytes(cursor, end, records_len, &records)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        return ESP_OK;
    }

    if (trace) {
        trace->fill_triangle_list_count++;
        trace->fill_triangle_list_instance_count += count;
        trace->scalar_count += count;
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * FILL_TRIANGLE_LIST_RECORD_SIZE);
        const int16_t x0 = lgfx_render_batch_read_le_i16(record);
        const int16_t y0 = lgfx_render_batch_read_le_i16(record + 2);
        const int16_t x1 = lgfx_render_batch_read_le_i16(record + 4);
        const int16_t y1 = lgfx_render_batch_read_le_i16(record + 6);
        const int16_t x2 = lgfx_render_batch_read_le_i16(record + 8);
        const int16_t y2 = lgfx_render_batch_read_le_i16(record + 10);
        const uint16_t color = lgfx_render_batch_read_le_u16(record + 12);

        const esp_err_t err = lgfx_dev::fill_triangle_locked(
            state->target,
            x0,
            y0,
            x1,
            y1,
            x2,
            y2,
            state->color_is_index,
            (uint32_t) color);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_sprite_list(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t flags = 0u;
    uint16_t transparent_value = 0u;
    uint16_t count = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)
        || !lgfx_render_batch_take_u16(cursor, end, &transparent_value)
        || !lgfx_render_batch_take_u16(cursor, end, &count)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if ((flags & ~SPRITE_LIST_ALLOWED_FLAGS) != 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const bool has_transparent = (flags & SPRITE_LIST_FLAG_HAS_TRANSPARENT) != 0u;
    const bool transparent_is_index = (flags & LGFX_F_TRANSPARENT_INDEX) != 0u;
    if (!has_transparent && transparent_value != 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }
    if (transparent_is_index && !has_transparent) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }
    if (transparent_is_index && transparent_value > UINT8_MAX) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (count == 0u || count > (SIZE_MAX / SPRITE_LIST_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * SPRITE_LIST_RECORD_SIZE;
    const uint8_t *records = nullptr;
    if (!lgfx_render_batch_take_bytes(cursor, end, records_len, &records)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        for (uint16_t i = 0; i < count; i++) {
            const uint8_t *record = records + ((size_t) i * SPRITE_LIST_RECORD_SIZE);
            const uint8_t src_handle = record[0];

            if (!lgfx_device_is_sprite_target(src_handle) || record[1] != 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }
        }
        return ESP_OK;
    }

    if (trace) {
        trace->sprite_push_list_count++;
        trace->sprite_push_list_instance_count += count;
    }

    lgfx_dev::PushSpriteListStats stats{};
    const esp_err_t err = lgfx_dev::push_sprite_list_locked(
        state->target,
        records,
        count,
        has_transparent,
        transparent_is_index,
        (uint32_t) transparent_value,
        &stats);
    if (err != ESP_OK) {
        return err;
    }

    if (trace) {
        trace->sprite_push_count += stats.instance_count;
    }

    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_sprite_region_list(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t flags = 0u;
    uint16_t transparent_value = 0u;
    uint16_t count = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)
        || !lgfx_render_batch_take_u16(cursor, end, &transparent_value)
        || !lgfx_render_batch_take_u16(cursor, end, &count)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if ((flags & ~SPRITE_REGION_LIST_ALLOWED_FLAGS) != 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const bool has_transparent = (flags & SPRITE_REGION_LIST_FLAG_HAS_TRANSPARENT) != 0u;
    if (!has_transparent && transparent_value != 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (count == 0u || count > (SIZE_MAX / SPRITE_REGION_LIST_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * SPRITE_REGION_LIST_RECORD_SIZE;
    const uint8_t *records = nullptr;
    if (!lgfx_render_batch_take_bytes(cursor, end, records_len, &records)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        for (uint16_t i = 0; i < count; i++) {
            const uint8_t *record = records + ((size_t) i * SPRITE_REGION_LIST_RECORD_SIZE);
            const uint8_t src_handle = record[0];
            const uint16_t src_w = lgfx_render_batch_read_le_u16(record + 6);
            const uint16_t src_h = lgfx_render_batch_read_le_u16(record + 8);

            if (!lgfx_device_is_sprite_target(src_handle) || record[1] != 0u || src_w == 0u || src_h == 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }
        }
        return ESP_OK;
    }

    if (trace) {
        trace->sprite_region_list_count++;
        trace->sprite_region_list_instance_count += count;
    }

    lgfx_dev::PushSpriteRegionListStats stats{};
    const esp_err_t err = lgfx_dev::push_sprite_region_list_locked(
        state->target,
        records,
        count,
        has_transparent,
        (uint32_t) transparent_value,
        &stats);
    if (err != ESP_OK) {
        return err;
    }

    if (trace) {
        trace->sprite_push_count += stats.instance_count;
    }

    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_przl(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (!state) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t flags = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if ((flags & ~((uint16_t) LGFX_F_TRANSPARENT_INDEX)) != 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (*cursor > end || (size_t) (end - *cursor) < PRZL_HEADER_SIZE) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const uint8_t *header = *cursor;
    if (header[0] != 'P' || header[1] != 'R' || header[2] != 'Z' || header[3] != 'L') {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const uint8_t version = header[4];
    const uint8_t options = header[5];
    const uint16_t transparent_value = lgfx_render_batch_read_le_u16(header + 6);
    const int16_t y_offset = lgfx_render_batch_read_le_i16(header + 8);
    const uint16_t count = lgfx_render_batch_read_le_u16(header + 10);

    if (version != 1u || (options & ~(PRZL_OPTION_HAS_TRANSPARENT | PRZL_OPTION_APPROX_CULL)) != 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const bool has_transparent = (options & PRZL_OPTION_HAS_TRANSPARENT) != 0u;
    const bool approx_cull = (options & PRZL_OPTION_APPROX_CULL) != 0u;
    const bool transparent_is_index = (flags & LGFX_F_TRANSPARENT_INDEX) != 0u;
    if (!has_transparent && transparent_value != 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }
    if (transparent_is_index && !has_transparent) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }
    if (transparent_is_index && transparent_value > UINT8_MAX) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (count == 0u || count > (SIZE_MAX / PRZL_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * PRZL_RECORD_SIZE;
    const uint8_t *records = header + PRZL_HEADER_SIZE;
    if (records > end || (size_t) (end - records) < records_len) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
        for (uint16_t i = 0; i < count; i++) {
            const uint8_t *record = records + ((size_t) i * PRZL_RECORD_SIZE);
            const uint8_t src_handle = record[0];
            const int16_t y = lgfx_render_batch_read_le_i16(record + 4);
            const uint16_t angle_cdeg = lgfx_render_batch_read_le_u16(record + 6);
            const uint16_t zoom_x1024 = lgfx_render_batch_read_le_u16(record + 8);
            const uint16_t zoom_y1024 = lgfx_render_batch_read_le_u16(record + 10);

            if (!lgfx_device_is_sprite_target(src_handle)
                || record[1] != 0u
                || angle_cdeg >= 36000u
                || zoom_x1024 == 0u
                || zoom_y1024 == 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            const int32_t shifted_y32 = (int32_t) y - (int32_t) y_offset;
            if (shifted_y32 < INT16_MIN || shifted_y32 > INT16_MAX) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }
        }
    } else {
        if (trace) {
            trace->przl_command_count++;
            trace->przl_instance_count += count;
        }

        lgfx_dev::PushRotateZoomListStats stats{};
        const esp_err_t err = lgfx_dev::push_rotate_zoom_list_locked(
            state->target,
            records,
            count,
            y_offset,
            has_transparent,
            transparent_is_index,
            (uint32_t) transparent_value,
            approx_cull,
            &stats);
        if (err != ESP_OK) {
            return err;
        }

        if (trace) {
            trace->przl_executed_count += stats.executed_count;
            trace->przl_culled_count += stats.culled_count;
        }
    }

    *cursor = records + records_len;
    return ESP_OK;
}


static esp_err_t lgfx_render_batch_parse_or_dispatch_przf(
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    uint16_t flags = 0u;
    if (!lgfx_render_batch_take_u16(cursor, end, &flags)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if ((flags & ~((uint16_t) LGFX_F_TRANSPARENT_INDEX)) != 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    if (*cursor > end || (size_t) (end - *cursor) < PRZF_HEADER_SIZE) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const uint8_t *header = *cursor;
    if (header[0] != 'P' || header[1] != 'R' || header[2] != 'Z' || header[3] != 'F') {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const uint8_t version = header[4];
    const uint8_t options = header[5];
    const uint16_t transparent_value = lgfx_render_batch_read_le_u16(header + 6);
    const uint16_t frame_height = lgfx_render_batch_read_le_u16(header + 8);
    const uint16_t background_color = lgfx_render_batch_read_le_u16(header + 10);
    const uint16_t count = lgfx_render_batch_read_le_u16(header + 12);

    if (version != 1u || (options & ~(PRZF_OPTION_HAS_TRANSPARENT | PRZF_OPTION_APPROX_CULL)) != 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const bool has_transparent = (options & PRZF_OPTION_HAS_TRANSPARENT) != 0u;
    const bool approx_cull = (options & PRZF_OPTION_APPROX_CULL) != 0u;
    const bool transparent_is_index = (flags & LGFX_F_TRANSPARENT_INDEX) != 0u;
    if (!has_transparent && transparent_value != 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }
    if (transparent_is_index && !has_transparent) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }
    if (transparent_is_index && transparent_value > UINT8_MAX) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }
    if (frame_height == 0u || count == 0u || count > (SIZE_MAX / PRZL_RECORD_SIZE)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const size_t records_len = (size_t) count * PRZL_RECORD_SIZE;
    const uint8_t *records = header + PRZF_HEADER_SIZE;
    if (records > end || (size_t) (end - records) < records_len) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *record = records + ((size_t) i * PRZL_RECORD_SIZE);
        const uint8_t src_handle = record[0];
        const uint16_t angle_cdeg = lgfx_render_batch_read_le_u16(record + 6);
        const uint16_t zoom_x1024 = lgfx_render_batch_read_le_u16(record + 8);
        const uint16_t zoom_y1024 = lgfx_render_batch_read_le_u16(record + 10);

        if (!lgfx_device_is_sprite_target(src_handle)
            || record[1] != 0u
            || angle_cdeg >= 36000u
            || zoom_x1024 == 0u
            || zoom_y1024 == 0u) {
            return lgfx_render_batch_malformed(out_malformed_command);
        }
    }

    if (mode == LGFX_RENDER_BATCH_EXECUTE) {
        lgfx_dev::PushRotateZoomFrameStats stats{};
        const esp_err_t err = lgfx_dev::push_rotate_zoom_frame_strips_locked(
            frame_height,
            (uint32_t) background_color,
            records,
            count,
            has_transparent,
            transparent_is_index,
            (uint32_t) transparent_value,
            approx_cull,
            &stats);
        if (err != ESP_OK) {
            return err;
        }

        if (trace) {
            trace->przf_command_count++;
            trace->przf_instance_count += stats.instance_count;
            trace->przf_executed_count += stats.executed_count;
            trace->przf_culled_count += stats.culled_count;
            trace->strip_begin_count += stats.strip_count;
            trace->strip_present_count += stats.strip_count;
            trace->frame_clear_us += stats.clear_us;
            trace->frame_draw_us += stats.draw_us;
            trace->frame_present_us += stats.present_us;
        }
    }

    *cursor = records + records_len;
    return ESP_OK;
}

static esp_err_t lgfx_render_batch_parse_or_dispatch_extended(
    lgfx_render_batch_state_t *state,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    uint8_t subop = 0u;
    if (!lgfx_render_batch_take_u8(cursor, end, &subop)) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    switch (subop) {
        case LGFX_RENDER_EXT_OP_ELLIPSE_LIST:
            return lgfx_render_batch_parse_or_dispatch_ellipse_list(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_RENDER_EXT_OP_PUSH_ROTATE_ZOOM_FRAME_STRIPS:
            if (state && state->strip_active) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }
            return lgfx_render_batch_parse_or_dispatch_przf(
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}


static esp_err_t lgfx_render_batch_parse_or_dispatch_one(
    lgfx_render_batch_state_t *state,
    uint8_t op,
    const uint8_t **cursor,
    const uint8_t *end,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    switch (op) {
        case LGFX_RENDER_OP_TARGET: {
            uint8_t target = 0u;
            if (!lgfx_render_batch_take_u8(cursor, end, &target)
                || !lgfx_dev::protocol_valid_target(target)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            state->target = target;
            if (mode == LGFX_RENDER_BATCH_EXECUTE && trace) {
                trace->target_count++;
            }
            return ESP_OK;
        }

        case LGFX_RENDER_OP_COLOR_MODE: {
            uint8_t color_mode = 0u;
            if (!lgfx_render_batch_take_u8(cursor, end, &color_mode)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            switch (color_mode) {
                case LGFX_RENDER_COLOR_MODE_RGB565:
                    state->color_is_index = false;
                    if (mode == LGFX_RENDER_BATCH_EXECUTE && trace) {
                        trace->color_mode_count++;
                    }
                    return ESP_OK;
                case LGFX_RENDER_COLOR_MODE_PALETTE_INDEX:
                    state->color_is_index = true;
                    if (mode == LGFX_RENDER_BATCH_EXECUTE && trace) {
                        trace->color_mode_count++;
                    }
                    return ESP_OK;
                default:
                    return lgfx_render_batch_malformed(out_malformed_command);
            }
        }

        case LGFX_RENDER_OP_DRAW_PIXEL_LIST:
            return lgfx_render_batch_parse_or_dispatch_draw_pixel_list(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_RENDER_OP_DRAW_RECT_LIST:
            return lgfx_render_batch_parse_or_dispatch_draw_rect_list(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_RENDER_OP_FILL_RECT_LIST:
            return lgfx_render_batch_parse_or_dispatch_fill_rect_list(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_RENDER_OP_DRAW_CIRCLE_LIST:
            return lgfx_render_batch_parse_or_dispatch_draw_circle_list(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_RENDER_OP_FILL_CIRCLE_LIST:
            return lgfx_render_batch_parse_or_dispatch_fill_circle_list(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_RENDER_OP_EXTENDED:
            return lgfx_render_batch_parse_or_dispatch_extended(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_RENDER_OP_DRAW_LINE_LIST:
            return lgfx_render_batch_parse_or_dispatch_draw_line_list(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_RENDER_OP_DRAW_TRIANGLE_LIST:
            return lgfx_render_batch_parse_or_dispatch_draw_triangle_list(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_RENDER_OP_FILL_TRIANGLE_LIST:
            return lgfx_render_batch_parse_or_dispatch_fill_triangle_list(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_OP_display: {
            if (state->strip_active) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->display_count++;
#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
                const int64_t display_started_at_us = esp_timer_get_time();
                const esp_err_t display_err = lgfx_render_batch_display_locked();
                trace->display_us += esp_timer_get_time() - display_started_at_us;
                return display_err;
#endif
            }

            return lgfx_render_batch_display_locked();
        }

        case LGFX_OP_fillScreen:
        case LGFX_OP_clear: {
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_u16(cursor, end, &color)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            if ((lgfx_op_t) op == LGFX_OP_fillScreen) {
                return lgfx_dev::fill_screen_locked(state->target, state->color_is_index, (uint32_t) color);
            }

            return lgfx_dev::clear_locked(state->target, state->color_is_index, (uint32_t) color);
        }

        case LGFX_OP_drawPixel: {
            int16_t x = 0;
            int16_t y = 0;
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &color)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            return lgfx_dev::draw_pixel_locked(state->target, x, y, state->color_is_index, (uint32_t) color);
        }

        case LGFX_OP_drawFastVLine: {
            int16_t x = 0;
            int16_t y = 0;
            uint16_t h = 0u;
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &h)
                || !lgfx_render_batch_take_u16(cursor, end, &color)
                || h == 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            return lgfx_dev::draw_fast_vline_locked(state->target, x, y, h, state->color_is_index, (uint32_t) color);
        }

        case LGFX_OP_drawFastHLine: {
            int16_t x = 0;
            int16_t y = 0;
            uint16_t w = 0u;
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &w)
                || !lgfx_render_batch_take_u16(cursor, end, &color)
                || w == 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            return lgfx_dev::draw_fast_hline_locked(state->target, x, y, w, state->color_is_index, (uint32_t) color);
        }

        case LGFX_OP_drawLine: {
            int16_t x0 = 0;
            int16_t y0 = 0;
            int16_t x1 = 0;
            int16_t y1 = 0;
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_i16(cursor, end, &x0)
                || !lgfx_render_batch_take_i16(cursor, end, &y0)
                || !lgfx_render_batch_take_i16(cursor, end, &x1)
                || !lgfx_render_batch_take_i16(cursor, end, &y1)
                || !lgfx_render_batch_take_u16(cursor, end, &color)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            return lgfx_dev::draw_line_locked(state->target, x0, y0, x1, y1, state->color_is_index, (uint32_t) color);
        }

        case LGFX_OP_drawRect:
        case LGFX_OP_fillRect: {
            int16_t x = 0;
            int16_t y = 0;
            uint16_t w = 0u;
            uint16_t h = 0u;
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &w)
                || !lgfx_render_batch_take_u16(cursor, end, &h)
                || !lgfx_render_batch_take_u16(cursor, end, &color)
                || w == 0u
                || h == 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            if ((lgfx_op_t) op == LGFX_OP_drawRect) {
                return lgfx_dev::draw_rect_locked(state->target, x, y, w, h, state->color_is_index, (uint32_t) color);
            }

            return lgfx_dev::fill_rect_locked(state->target, x, y, w, h, state->color_is_index, (uint32_t) color);
        }

        case LGFX_OP_drawRoundRect:
        case LGFX_OP_fillRoundRect: {
            int16_t x = 0;
            int16_t y = 0;
            uint16_t w = 0u;
            uint16_t h = 0u;
            uint16_t r = 0u;
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &w)
                || !lgfx_render_batch_take_u16(cursor, end, &h)
                || !lgfx_render_batch_take_u16(cursor, end, &r)
                || !lgfx_render_batch_take_u16(cursor, end, &color)
                || w == 0u
                || h == 0u
                || r == 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            if ((lgfx_op_t) op == LGFX_OP_drawRoundRect) {
                return lgfx_dev::draw_round_rect_locked(
                    state->target,
                    x,
                    y,
                    w,
                    h,
                    r,
                    state->color_is_index,
                    (uint32_t) color);
            }

            return lgfx_dev::fill_round_rect_locked(
                state->target,
                x,
                y,
                w,
                h,
                r,
                state->color_is_index,
                (uint32_t) color);
        }

        case LGFX_OP_drawCircle:
        case LGFX_OP_fillCircle: {
            int16_t x = 0;
            int16_t y = 0;
            uint16_t r = 0u;
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &r)
                || !lgfx_render_batch_take_u16(cursor, end, &color)
                || r == 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            if ((lgfx_op_t) op == LGFX_OP_drawCircle) {
                return lgfx_dev::draw_circle_locked(state->target, x, y, r, state->color_is_index, (uint32_t) color);
            }

            return lgfx_dev::fill_circle_locked(state->target, x, y, r, state->color_is_index, (uint32_t) color);
        }

        case LGFX_OP_drawEllipse:
        case LGFX_OP_fillEllipse: {
            int16_t x = 0;
            int16_t y = 0;
            uint16_t rx = 0u;
            uint16_t ry = 0u;
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &rx)
                || !lgfx_render_batch_take_u16(cursor, end, &ry)
                || !lgfx_render_batch_take_u16(cursor, end, &color)
                || rx == 0u
                || ry == 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            if ((lgfx_op_t) op == LGFX_OP_drawEllipse) {
                return lgfx_dev::draw_ellipse_locked(state->target, x, y, rx, ry, state->color_is_index, (uint32_t) color);
            }

            return lgfx_dev::fill_ellipse_locked(state->target, x, y, rx, ry, state->color_is_index, (uint32_t) color);
        }

        case LGFX_OP_drawArc:
        case LGFX_OP_fillArc: {
            int16_t x = 0;
            int16_t y = 0;
            uint16_t r0 = 0u;
            uint16_t r1 = 0u;
            float angle0 = 0.0f;
            float angle1 = 0.0f;
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &r0)
                || !lgfx_render_batch_take_u16(cursor, end, &r1)
                || !lgfx_render_batch_take_f32(cursor, end, &angle0)
                || !lgfx_render_batch_take_f32(cursor, end, &angle1)
                || !lgfx_render_batch_take_u16(cursor, end, &color)
                || r0 == 0u
                || r1 == 0u
                || !isfinite(angle0)
                || !isfinite(angle1)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            if ((lgfx_op_t) op == LGFX_OP_drawArc) {
                return lgfx_dev::draw_arc_locked(
                    state->target,
                    x,
                    y,
                    r0,
                    r1,
                    angle0,
                    angle1,
                    state->color_is_index,
                    (uint32_t) color);
            }

            return lgfx_dev::fill_arc_locked(
                state->target,
                x,
                y,
                r0,
                r1,
                angle0,
                angle1,
                state->color_is_index,
                (uint32_t) color);
        }

        case LGFX_OP_drawBezier: {
            uint8_t point_count = 0u;
            uint8_t reserved = 0u;
            int16_t x0 = 0;
            int16_t y0 = 0;
            int16_t x1 = 0;
            int16_t y1 = 0;
            int16_t x2 = 0;
            int16_t y2 = 0;
            int16_t x3 = 0;
            int16_t y3 = 0;
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_u8(cursor, end, &point_count)
                || !lgfx_render_batch_take_u8(cursor, end, &reserved)
                || reserved != 0u
                || (point_count != 3u && point_count != 4u)
                || !lgfx_render_batch_take_i16(cursor, end, &x0)
                || !lgfx_render_batch_take_i16(cursor, end, &y0)
                || !lgfx_render_batch_take_i16(cursor, end, &x1)
                || !lgfx_render_batch_take_i16(cursor, end, &y1)
                || !lgfx_render_batch_take_i16(cursor, end, &x2)
                || !lgfx_render_batch_take_i16(cursor, end, &y2)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (point_count == 4u) {
                if (!lgfx_render_batch_take_i16(cursor, end, &x3)
                    || !lgfx_render_batch_take_i16(cursor, end, &y3)) {
                    return lgfx_render_batch_malformed(out_malformed_command);
                }
            }

            if (!lgfx_render_batch_take_u16(cursor, end, &color)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            if (point_count == 3u) {
                return lgfx_dev::draw_bezier3_locked(
                    state->target,
                    x0,
                    y0,
                    x1,
                    y1,
                    x2,
                    y2,
                    state->color_is_index,
                    (uint32_t) color);
            }

            return lgfx_dev::draw_bezier4_locked(
                state->target,
                x0,
                y0,
                x1,
                y1,
                x2,
                y2,
                x3,
                y3,
                state->color_is_index,
                (uint32_t) color);
        }

        case LGFX_OP_drawTriangle:
        case LGFX_OP_fillTriangle: {
            int16_t x0 = 0;
            int16_t y0 = 0;
            int16_t x1 = 0;
            int16_t y1 = 0;
            int16_t x2 = 0;
            int16_t y2 = 0;
            uint16_t color = 0u;
            if (!lgfx_render_batch_take_i16(cursor, end, &x0)
                || !lgfx_render_batch_take_i16(cursor, end, &y0)
                || !lgfx_render_batch_take_i16(cursor, end, &x1)
                || !lgfx_render_batch_take_i16(cursor, end, &y1)
                || !lgfx_render_batch_take_i16(cursor, end, &x2)
                || !lgfx_render_batch_take_i16(cursor, end, &y2)
                || !lgfx_render_batch_take_u16(cursor, end, &color)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->scalar_count++;
            }

            if ((lgfx_op_t) op == LGFX_OP_drawTriangle) {
                return lgfx_dev::draw_triangle_locked(
                    state->target,
                    x0,
                    y0,
                    x1,
                    y1,
                    x2,
                    y2,
                    state->color_is_index,
                    (uint32_t) color);
            }

            return lgfx_dev::fill_triangle_locked(
                state->target,
                x0,
                y0,
                x1,
                y1,
                x2,
                y2,
                state->color_is_index,
                (uint32_t) color);
        }

        case LGFX_OP_setClipRect: {
            int16_t x = 0;
            int16_t y = 0;
            uint16_t w = 0u;
            uint16_t h = 0u;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &w)
                || !lgfx_render_batch_take_u16(cursor, end, &h)
                || w == 0u
                || h == 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->clip_command_count++;
            }

            return lgfx_dev::set_clip_rect_locked(state->target, x, y, w, h);
        }

        case LGFX_OP_clearClipRect: {
            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->clip_command_count++;
            }

            return lgfx_dev::clear_clip_rect_locked(state->target);
        }

        case LGFX_OP_setTextFontPreset: {
            uint8_t preset = 0u;
            if (!lgfx_render_batch_take_u8(cursor, end, &preset)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            switch (preset) {
                case LGFX_FONT_PRESET_ASCII:
                case LGFX_FONT_PRESET_JP:
                    break;
                default:
                    return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->text_command_count++;
            }

            return lgfx_dev::set_text_font_preset_locked(
                state->target,
                (lgfx_font_preset_t) preset);
        }

        case LGFX_OP_setTextSize: {
            uint16_t scale_x1024 = 0u;
            uint16_t scale_y1024 = 0u;
            if (!lgfx_render_batch_take_u16(cursor, end, &scale_x1024)
                || !lgfx_render_batch_take_u16(cursor, end, &scale_y1024)
                || scale_x1024 == 0u
                || scale_y1024 == 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->text_command_count++;
            }

            return lgfx_dev::set_text_size_locked(
                state->target,
                (float) scale_x1024 / 1024.0f,
                (float) scale_y1024 / 1024.0f);
        }

        case LGFX_OP_setTextDatum: {
            uint8_t datum = 0u;
            if (!lgfx_render_batch_take_u8(cursor, end, &datum)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->text_command_count++;
            }

            return lgfx_dev::set_text_datum_locked(state->target, datum);
        }

        case LGFX_OP_setTextWrap: {
            uint8_t wrap_x = 0u;
            uint8_t wrap_y = 0u;
            if (!lgfx_render_batch_take_u8(cursor, end, &wrap_x)
                || !lgfx_render_batch_take_u8(cursor, end, &wrap_y)
                || wrap_x > 1u
                || wrap_y > 1u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->text_command_count++;
            }

            return lgfx_dev::set_text_wrap_locked(state->target, wrap_x != 0u, wrap_y != 0u);
        }

        case LGFX_OP_setTextColor: {
            uint16_t flags = 0u;
            uint16_t fg_value = 0u;
            if (!lgfx_render_batch_take_u16(cursor, end, &flags)
                || !lgfx_render_batch_take_u16(cursor, end, &fg_value)
                || (flags & ~TEXT_ALLOWED_FLAGS) != 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            const bool has_bg = (flags & LGFX_F_TEXT_HAS_BG) != 0u;
            const bool fg_is_index = (flags & LGFX_F_TEXT_FG_INDEX) != 0u;
            const bool bg_is_index = (flags & LGFX_F_TEXT_BG_INDEX) != 0u;

            if (!has_bg && bg_is_index) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            uint16_t bg_value = 0u;
            if (has_bg && !lgfx_render_batch_take_u16(cursor, end, &bg_value)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if ((fg_is_index && fg_value > UINT8_MAX)
                || (has_bg && bg_is_index && bg_value > UINT8_MAX)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->text_command_count++;
            }

            return lgfx_dev::set_text_color_locked(
                state->target,
                fg_is_index,
                (uint32_t) fg_value,
                has_bg,
                bg_is_index,
                (uint32_t) bg_value);
        }

        case LGFX_OP_setCursor: {
            int16_t x = 0;
            int16_t y = 0;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->text_command_count++;
            }

            return lgfx_dev::set_cursor_locked(state->target, x, y);
        }

        case LGFX_OP_drawString: {
            int16_t x = 0;
            int16_t y = 0;
            uint16_t text_len = 0u;
            const uint8_t *text = nullptr;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &text_len)
                || text_len == 0u
                || !lgfx_render_batch_take_bytes(cursor, end, (size_t) text_len, &text)
                || lgfx_render_batch_contains_nul(text, (size_t) text_len)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->text_command_count++;
            }

            return lgfx_dev::draw_string_locked(
                state->target,
                x,
                y,
                text,
                (size_t) text_len);
        }

        case LGFX_OP_print:
        case LGFX_OP_println: {
            uint16_t text_len = 0u;
            const uint8_t *text = nullptr;
            if (!lgfx_render_batch_take_u16(cursor, end, &text_len)
                || !lgfx_render_batch_take_bytes(cursor, end, (size_t) text_len, &text)
                || lgfx_render_batch_contains_nul(text, (size_t) text_len)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->text_command_count++;
            }

            if ((lgfx_op_t) op == LGFX_OP_print) {
                return lgfx_dev::print_locked(state->target, text, (size_t) text_len);
            }

            return lgfx_dev::println_locked(state->target, text, (size_t) text_len);
        }

        case LGFX_OP_drawJpg: {
            uint8_t variant = 0u;
            uint8_t reserved = 0u;
            int16_t x = 0;
            int16_t y = 0;
            uint16_t max_w = 0u;
            uint16_t max_h = 0u;
            int16_t off_x = 0;
            int16_t off_y = 0;
            float scale_x = 1.0f;
            float scale_y = 1.0f;
            uint32_t jpeg_len_u32 = 0u;
            const uint8_t *jpeg = nullptr;

            if (!lgfx_render_batch_take_u8(cursor, end, &variant)
                || !lgfx_render_batch_take_u8(cursor, end, &reserved)
                || reserved != 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (variant == 0u) {
                if (!lgfx_render_batch_take_i16(cursor, end, &x)
                    || !lgfx_render_batch_take_i16(cursor, end, &y)
                    || !lgfx_render_batch_take_u32(cursor, end, &jpeg_len_u32)
                    || !lgfx_render_batch_take_bytes(cursor, end, (size_t) jpeg_len_u32, &jpeg)) {
                    return lgfx_render_batch_malformed(out_malformed_command);
                }
            } else if (variant == 1u) {
                if (!lgfx_render_batch_take_i16(cursor, end, &x)
                    || !lgfx_render_batch_take_i16(cursor, end, &y)
                    || !lgfx_render_batch_take_u16(cursor, end, &max_w)
                    || !lgfx_render_batch_take_u16(cursor, end, &max_h)
                    || !lgfx_render_batch_take_i16(cursor, end, &off_x)
                    || !lgfx_render_batch_take_i16(cursor, end, &off_y)
                    || !lgfx_render_batch_take_f32(cursor, end, &scale_x)
                    || !lgfx_render_batch_take_f32(cursor, end, &scale_y)
                    || !lgfx_render_batch_take_u32(cursor, end, &jpeg_len_u32)
                    || !lgfx_render_batch_take_bytes(cursor, end, (size_t) jpeg_len_u32, &jpeg)) {
                    return lgfx_render_batch_malformed(out_malformed_command);
                }
            } else {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (jpeg_len_u32 == 0u
                || !isfinite(scale_x)
                || !isfinite(scale_y)
                || scale_x <= 0.0f
                || scale_y <= 0.0f) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->image_command_count++;
            }

            return lgfx_dev::draw_jpg_locked(
                state->target,
                x,
                y,
                max_w,
                max_h,
                off_x,
                off_y,
                scale_x,
                scale_y,
                jpeg,
                (size_t) jpeg_len_u32);
        }

        case LGFX_OP_pushImage: {
            int16_t x = 0;
            int16_t y = 0;
            uint16_t w = 0u;
            uint16_t h = 0u;
            uint16_t stride_pixels = 0u;
            uint32_t pixels_len_u32 = 0u;
            const uint8_t *pixels = nullptr;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &w)
                || !lgfx_render_batch_take_u16(cursor, end, &h)
                || !lgfx_render_batch_take_u16(cursor, end, &stride_pixels)
                || !lgfx_render_batch_take_u32(cursor, end, &pixels_len_u32)
                || !lgfx_render_batch_take_bytes(cursor, end, (size_t) pixels_len_u32, &pixels)
                || w == 0u
                || h == 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            uint16_t normalized_stride = stride_pixels;
            if (normalized_stride == 0u) {
                normalized_stride = w;
            }

            const size_t pixels_len = (size_t) pixels_len_u32;
            const uint64_t needed64 = (uint64_t) normalized_stride * 2u * (uint64_t) h;
            if (normalized_stride < w
                || (pixels_len & 1u) != 0u
                || needed64 > (uint64_t) SIZE_MAX
                || pixels_len < (size_t) needed64) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->image_command_count++;
            }

            return lgfx_dev::push_image_rgb565_strided_locked(
                state->target,
                x,
                y,
                w,
                h,
                stride_pixels,
                pixels,
                pixels_len);
        }

        case LGFX_OP_setPaletteColor: {
            uint8_t palette_index = 0u;
            uint8_t reserved = 0u;
            uint32_t rgb888 = 0u;
            if (!lgfx_render_batch_take_u8(cursor, end, &palette_index)
                || !lgfx_render_batch_take_u8(cursor, end, &reserved)
                || !lgfx_render_batch_take_u32(cursor, end, &rgb888)
                || reserved != 0u
                || rgb888 > 0xFFFFFFu
                || !lgfx_device_is_sprite_target(state->target)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            return lgfx_dev::set_palette_color_locked(state->target, palette_index, rgb888);
        }

        case LGFX_OP_setPivot: {
            int16_t x = 0;
            int16_t y = 0;
            if (!lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            return lgfx_dev::set_pivot_locked(state->target, x, y);
        }

        case LGFX_OP_pushSprite: {
            uint8_t src_handle = 0u;
            int16_t x = 0;
            int16_t y = 0;
            if (!lgfx_render_batch_take_u8(cursor, end, &src_handle)
                || !lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_device_is_sprite_target(src_handle)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->sprite_push_count++;
            }

            return lgfx_dev::push_sprite_locked(
                src_handle,
                state->target,
                x,
                y,
                false,
                false,
                0u);
        }

        case LGFX_RENDER_OP_PUSH_SPRITE_TRANSPARENT: {
            uint16_t flags = 0u;
            uint8_t src_handle = 0u;
            int16_t x = 0;
            int16_t y = 0;
            uint16_t transparent_value = 0u;
            if (!lgfx_render_batch_take_u16(cursor, end, &flags)
                || !lgfx_render_batch_take_u8(cursor, end, &src_handle)
                || !lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_u16(cursor, end, &transparent_value)
                || !lgfx_device_is_sprite_target(src_handle)
                || (flags & ~((uint16_t) LGFX_F_TRANSPARENT_INDEX)) != 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            const bool transparent_is_index = (flags & LGFX_F_TRANSPARENT_INDEX) != 0u;
            if (transparent_is_index && transparent_value > UINT8_MAX) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->sprite_push_count++;
            }

            return lgfx_dev::push_sprite_locked(
                src_handle,
                state->target,
                x,
                y,
                true,
                transparent_is_index,
                (uint32_t) transparent_value);
        }

        case LGFX_RENDER_OP_PUSH_SPRITE_LIST:
            return lgfx_render_batch_parse_or_dispatch_sprite_list(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_RENDER_OP_PUSH_SPRITE_REGION_LIST:
            return lgfx_render_batch_parse_or_dispatch_sprite_region_list(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        case LGFX_OP_pushRotateZoom: {
            uint8_t options = 0u;
            uint8_t reserved = 0u;
            uint16_t flags = 0u;
            uint8_t src_handle = 0u;
            uint8_t src_reserved = 0u;
            int16_t x = 0;
            int16_t y = 0;
            float angle = 0.0f;
            float zoom_x = 1.0f;
            float zoom_y = 1.0f;
            uint16_t transparent_value = 0u;
            if (!lgfx_render_batch_take_u8(cursor, end, &options)
                || !lgfx_render_batch_take_u8(cursor, end, &reserved)
                || !lgfx_render_batch_take_u16(cursor, end, &flags)
                || !lgfx_render_batch_take_u8(cursor, end, &src_handle)
                || !lgfx_render_batch_take_u8(cursor, end, &src_reserved)
                || !lgfx_render_batch_take_i16(cursor, end, &x)
                || !lgfx_render_batch_take_i16(cursor, end, &y)
                || !lgfx_render_batch_take_f32(cursor, end, &angle)
                || !lgfx_render_batch_take_f32(cursor, end, &zoom_x)
                || !lgfx_render_batch_take_f32(cursor, end, &zoom_y)
                || !lgfx_render_batch_take_u16(cursor, end, &transparent_value)
                || reserved != 0u
                || src_reserved != 0u
                || !lgfx_device_is_sprite_target(src_handle)
                || (flags & ~((uint16_t) LGFX_F_TRANSPARENT_INDEX)) != 0u
                || (options & ~((uint8_t) 0x01u)) != 0u
                || !isfinite(angle)
                || !isfinite(zoom_x)
                || !isfinite(zoom_y)
                || zoom_x <= 0.0f
                || zoom_y <= 0.0f) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            const bool has_transparent = (options & 0x01u) != 0u;
            const bool transparent_is_index = (flags & LGFX_F_TRANSPARENT_INDEX) != 0u;
            if (!has_transparent && transparent_value != 0u) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }
            if (transparent_is_index && !has_transparent) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }
            if (transparent_is_index && transparent_value > UINT8_MAX) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                return ESP_OK;
            }

            if (trace) {
                trace->sprite_push_count++;
            }

            return lgfx_dev::push_rotate_zoom_locked(
                src_handle,
                state->target,
                x,
                y,
                angle,
                zoom_x,
                zoom_y,
                has_transparent,
                transparent_is_index,
                (uint32_t) transparent_value,
                false);
        }

        case LGFX_RENDER_OP_BEGIN_STRIP: {
            uint16_t y0 = 0u;
            if (!lgfx_render_batch_take_u16(cursor, end, &y0)) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (state->strip_active) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                state->strip_active = true;
                return ESP_OK;
            }

            if (trace) {
                trace->strip_begin_count++;
            }

            const esp_err_t err = lgfx_dev::presentation_begin_strip_locked(y0);
            if (err != ESP_OK) {
                return err;
            }

            state->strip_active = true;
            return ESP_OK;
        }

        case LGFX_RENDER_OP_PRESENT_STRIP: {
            if (!state->strip_active) {
                return lgfx_render_batch_malformed(out_malformed_command);
            }

            if (mode == LGFX_RENDER_BATCH_VALIDATE_ONLY) {
                state->strip_active = false;
                return ESP_OK;
            }

            if (trace) {
                trace->strip_present_count++;
            }

            const esp_err_t err = lgfx_dev::presentation_present_strip_locked();
            if (err != ESP_OK) {
                return err;
            }

            state->strip_active = false;
            return ESP_OK;
        }

        case LGFX_OP_pushRotateZoomList:
            return lgfx_render_batch_parse_or_dispatch_przl(
                state,
                cursor,
                end,
                out_malformed_command,
                mode,
                trace);

        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
}

static esp_err_t lgfx_render_batch_run_stream(
    const uint8_t *bytes,
    size_t len,
    uint32_t *out_failed_index,
    uint8_t *out_failed_opcode,
    bool *out_malformed_command,
    lgfx_render_batch_mode_t mode,
    lgfx_render_batch_trace_t *trace)
{
    if (bytes == nullptr || len == 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    const uint8_t *cursor = bytes;
    const uint8_t *const end = bytes + len;
    uint32_t command_index = 0u;
    lgfx_render_batch_state_t state{};

    while (cursor < end) {
        const uint8_t op = *cursor++;
        if (mode == LGFX_RENDER_BATCH_EXECUTE && trace) {
            trace->command_count++;
        }

        const esp_err_t err = lgfx_render_batch_parse_or_dispatch_one(
            &state,
            op,
            &cursor,
            end,
            out_malformed_command,
            mode,
            trace);
        if (err != ESP_OK) {
            if (out_failed_index) {
                *out_failed_index = command_index;
            }
            if (out_failed_opcode) {
                *out_failed_opcode = op;
            }
            return err;
        }

        command_index++;
    }

    if (state.strip_active) {
        if (out_failed_index) {
            *out_failed_index = command_index;
        }
        if (out_failed_opcode) {
            *out_failed_opcode = 0u;
        }
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    return ESP_OK;
}

} // namespace

extern "C" esp_err_t lgfx_render_batch_dispatch_validate(
    const uint8_t *bytes,
    size_t len,
    uint32_t *out_failed_index,
    uint8_t *out_failed_opcode,
    bool *out_malformed_command)
{
    lgfx_render_batch_init_result(out_failed_index, out_failed_opcode, out_malformed_command);
    return lgfx_render_batch_run_stream(
        bytes,
        len,
        out_failed_index,
        out_failed_opcode,
        out_malformed_command,
        LGFX_RENDER_BATCH_VALIDATE_ONLY,
        nullptr);
}

extern "C" esp_err_t lgfx_render_batch_dispatch_run(
    const uint8_t *bytes,
    size_t len,
    uint32_t *out_failed_index,
    uint8_t *out_failed_opcode,
    bool *out_malformed_command)
{
    lgfx_render_batch_init_result(out_failed_index, out_failed_opcode, out_malformed_command);

    if (bytes == nullptr || len == 0u) {
        return lgfx_render_batch_malformed(out_malformed_command);
    }

    esp_err_t err = ESP_OK;

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
    int64_t prevalidate_us = 0;
#endif

#if LGFX_PORT_RENDER_BATCH_PREVALIDATE
#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
    const int64_t prevalidate_started_at_us = esp_timer_get_time();
#endif

    err = lgfx_render_batch_dispatch_validate(
        bytes,
        len,
        out_failed_index,
        out_failed_opcode,
        out_malformed_command);

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
    prevalidate_us = esp_timer_get_time() - prevalidate_started_at_us;
#endif

    if (err != ESP_OK) {
        return err;
    }
#endif

    lgfx_dev::ScopedLcdLock lock;
    err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
    const int64_t start_write_started_at_us = esp_timer_get_time();
#endif

    err = lgfx_dev::start_write_locked();
    if (err != ESP_OK) {
        return err;
    }

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
    const int64_t execute_started_at_us = esp_timer_get_time();
#endif

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
    lgfx_render_batch_trace_t trace{};
    lgfx_render_batch_trace_t *trace_ptr = &trace;
#else
    lgfx_render_batch_trace_t *trace_ptr = nullptr;
#endif

    err = lgfx_render_batch_run_stream(
        bytes,
        len,
        out_failed_index,
        out_failed_opcode,
        out_malformed_command,
        LGFX_RENDER_BATCH_EXECUTE,
        trace_ptr);

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
    const int64_t end_write_started_at_us = esp_timer_get_time();
#endif

    const esp_err_t end_err = lgfx_dev::end_write_locked();

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
    const int64_t finished_at_us = esp_timer_get_time();
    const int64_t start_write_us = execute_started_at_us - start_write_started_at_us;
    const int64_t execute_us = end_write_started_at_us - execute_started_at_us;
    const int64_t end_write_us = finished_at_us - end_write_started_at_us;
    const int64_t total_us = prevalidate_us + start_write_us + execute_us + end_write_us;

    ESP_LOGI(
        LOG_TAG,
        "stats batch_bytes=%u prevalidate_us=%lld start_write_us=%lld execute_us=%lld end_write_us=%lld total_us=%lld commands=%u target=%u color_mode=%u scalar=%u draw_pixel_lists=%u draw_pixel_list_instances=%u draw_rect_lists=%u draw_rect_list_instances=%u fill_rect_lists=%u fill_rect_list_instances=%u draw_circle_lists=%u draw_circle_list_instances=%u fill_circle_lists=%u fill_circle_list_instances=%u draw_ellipse_lists=%u draw_ellipse_list_instances=%u fill_ellipse_lists=%u fill_ellipse_list_instances=%u draw_line_lists=%u draw_line_list_instances=%u draw_triangle_lists=%u draw_triangle_list_instances=%u fill_triangle_lists=%u fill_triangle_list_instances=%u clip=%u text=%u image=%u sprite_push=%u sprite_push_lists=%u sprite_push_list_instances=%u sprite_region_lists=%u sprite_region_list_instances=%u przl_commands=%u przl_instances=%u przl_executed=%u przl_culled=%u przf_commands=%u przf_instances=%u przf_executed=%u przf_culled=%u strip_begin=%u strip_present=%u display=%u frame_clear_us=%lld frame_draw_us=%lld frame_present_us=%lld display_us=%lld err=%d end_err=%d",
        (unsigned) len,
        (long long) prevalidate_us,
        (long long) start_write_us,
        (long long) execute_us,
        (long long) end_write_us,
        (long long) total_us,
        (unsigned) trace.command_count,
        (unsigned) trace.target_count,
        (unsigned) trace.color_mode_count,
        (unsigned) trace.scalar_count,
        (unsigned) trace.draw_pixel_list_count,
        (unsigned) trace.draw_pixel_list_instance_count,
        (unsigned) trace.draw_rect_list_count,
        (unsigned) trace.draw_rect_list_instance_count,
        (unsigned) trace.fill_rect_list_count,
        (unsigned) trace.fill_rect_list_instance_count,
        (unsigned) trace.draw_circle_list_count,
        (unsigned) trace.draw_circle_list_instance_count,
        (unsigned) trace.fill_circle_list_count,
        (unsigned) trace.fill_circle_list_instance_count,
        (unsigned) trace.draw_ellipse_list_count,
        (unsigned) trace.draw_ellipse_list_instance_count,
        (unsigned) trace.fill_ellipse_list_count,
        (unsigned) trace.fill_ellipse_list_instance_count,
        (unsigned) trace.draw_line_list_count,
        (unsigned) trace.draw_line_list_instance_count,
        (unsigned) trace.draw_triangle_list_count,
        (unsigned) trace.draw_triangle_list_instance_count,
        (unsigned) trace.fill_triangle_list_count,
        (unsigned) trace.fill_triangle_list_instance_count,
        (unsigned) trace.clip_command_count,
        (unsigned) trace.text_command_count,
        (unsigned) trace.image_command_count,
        (unsigned) trace.sprite_push_count,
        (unsigned) trace.sprite_push_list_count,
        (unsigned) trace.sprite_push_list_instance_count,
        (unsigned) trace.sprite_region_list_count,
        (unsigned) trace.sprite_region_list_instance_count,
        (unsigned) trace.przl_command_count,
        (unsigned) trace.przl_instance_count,
        (unsigned) trace.przl_executed_count,
        (unsigned) trace.przl_culled_count,
        (unsigned) trace.przf_command_count,
        (unsigned) trace.przf_instance_count,
        (unsigned) trace.przf_executed_count,
        (unsigned) trace.przf_culled_count,
        (unsigned) trace.strip_begin_count,
        (unsigned) trace.strip_present_count,
        (unsigned) trace.display_count,
        (long long) trace.frame_clear_us,
        (long long) trace.frame_draw_us,
        (long long) trace.frame_present_us,
        (long long) trace.display_us,
        (int) err,
        (int) end_err);
#endif

    if (err != ESP_OK) {
        (void) lgfx_dev::presentation_cancel_strip_locked();
        return err;
    }

    if (end_err != ESP_OK) {
        (void) lgfx_dev::presentation_cancel_strip_locked();
        return end_err;
    }

    return ESP_OK;
}
