/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_device/render_programs.cpp

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lgfx_device/lgfx_device_internal.hpp"
#include "lgfx_port/protocol.h"

namespace
{

static constexpr uint8_t MAX_OBJECT_BUFFER_HANDLES = 8u;
static constexpr uint8_t MAX_RENDER_PROGRAM_HANDLES = 8u;
static constexpr uint8_t MAX_RENDER_PROGRAM_SOURCES = 16u;
static constexpr configSTACK_DEPTH_TYPE RENDER_PROGRAM_TASK_STACK_WORDS = 4096u;
static constexpr UBaseType_t RENDER_PROGRAM_TASK_PRIORITY = (tskIDLE_PRIORITY + 1u);
static constexpr TickType_t RENDER_PROGRAM_STOP_TIMEOUT_TICKS = pdMS_TO_TICKS(5000);
static constexpr TickType_t RENDER_PROGRAM_FRAME_IDLE_TICKS = 1u;
static constexpr uint16_t ANGLE_CDEG_WRAP = 36000u;

struct RetainedSpriteTransform2DRecord
{
    uint8_t source_index;
    uint8_t reserved;
    int16_t x;
    int16_t y;
    int16_t vx;
    int16_t vy;
    uint16_t angle_cdeg;
    uint16_t zoom_x1024;
    int16_t dangle_cdeg;
    int16_t dzoom_x1024;
};

static_assert(
    sizeof(RetainedSpriteTransform2DRecord) == LGFX_DEVICE_RETAINED_SPRITE_TRANSFORM_2D_RECORD_SIZE,
    "retained object record size drift");

struct RetainedObjectBuffer
{
    bool allocated = false;
    uint8_t layout_id = 0u;
    uint16_t capacity = 0u;
    uint16_t count = 0u;
    uint8_t *bytes = nullptr;
};

struct RetainedRenderProgram
{
    bool allocated = false;
    volatile bool running = false;
    volatile bool stop_requested = false;

    uint8_t type_id = 0u;
    uint8_t object_buffer_handle = 0u;
    uint8_t update_policy = 0u;

    bool has_transparent = false;
    uint16_t transparent_value = 0u;
    uint16_t background_color = 0u;
    uint16_t requested_strip_height = 0u;
    uint16_t effective_strip_height = 0u;
    uint16_t zoom_min_x1024 = 0u;
    uint16_t zoom_max_x1024 = 0u;
    uint16_t frame_width = 0u;
    uint16_t frame_height = 0u;

    uint8_t source_count = 0u;
    uint8_t source_handles[MAX_RENDER_PROGRAM_SOURCES] = { 0 };
    lgfx::LGFX_Sprite *source_sprites[MAX_RENDER_PROGRAM_SOURCES] = { nullptr };

    TaskHandle_t task_handle = nullptr;
    lgfx_retained_render_program_stats_t stats = {};
};

static RetainedObjectBuffer g_object_buffers[MAX_OBJECT_BUFFER_HANDLES + 1u] = {};
static RetainedRenderProgram g_render_programs[MAX_RENDER_PROGRAM_HANDLES + 1u] = {};
static uint8_t g_active_render_program_handle = 0u;

static inline size_t layout_record_size(uint8_t layout_id)
{
    switch (layout_id) {
        case LGFX_OBJECT_BUFFER_LAYOUT_SPRITE_TRANSFORM_2D:
            return LGFX_DEVICE_RETAINED_SPRITE_TRANSFORM_2D_RECORD_SIZE;
        default:
            return 0u;
    }
}

static inline RetainedObjectBuffer *lookup_object_buffer(uint8_t handle)
{
    if (handle == 0u || handle > MAX_OBJECT_BUFFER_HANDLES) {
        return nullptr;
    }

    return &g_object_buffers[handle];
}

static inline const RetainedObjectBuffer *lookup_object_buffer_const(uint8_t handle)
{
    if (handle == 0u || handle > MAX_OBJECT_BUFFER_HANDLES) {
        return nullptr;
    }

    return &g_object_buffers[handle];
}

static inline RetainedRenderProgram *lookup_render_program(uint8_t handle)
{
    if (handle == 0u || handle > MAX_RENDER_PROGRAM_HANDLES) {
        return nullptr;
    }

    return &g_render_programs[handle];
}

static inline uint8_t handle_from_render_program(const RetainedRenderProgram *program)
{
    if (!program) {
        return 0u;
    }

    for (uint8_t handle = 1u; handle <= MAX_RENDER_PROGRAM_HANDLES; handle++) {
        if (&g_render_programs[handle] == program) {
            return handle;
        }
    }

    return 0u;
}

static inline bool safe_capacity_bytes(uint16_t capacity, size_t record_size, size_t *out_total_bytes)
{
    if (!out_total_bytes || record_size == 0u) {
        return false;
    }

    const size_t total_bytes = static_cast<size_t>(capacity) * record_size;
    if (capacity != 0u && (total_bytes / record_size) != static_cast<size_t>(capacity)) {
        return false;
    }

    if (total_bytes > static_cast<size_t>(LGFX_PORT_MAX_BINARY_BYTES)) {
        return false;
    }

    *out_total_bytes = total_bytes;
    return true;
}

static inline bool valid_render_program_type(uint8_t type_id)
{
    return type_id == LGFX_RENDER_PROGRAM_TYPE_STRIPED_SPRITE_TRANSFORM;
}

static inline bool valid_render_program_update_policy(uint8_t update_policy)
{
    return update_policy == LGFX_RENDER_PROGRAM_UPDATE_NONE
        || update_policy == LGFX_RENDER_PROGRAM_UPDATE_BOUNCE;
}

static inline bool valid_render_program_mode(uint8_t mode)
{
    return mode == LGFX_RENDER_PROGRAM_MODE_EXCLUSIVE;
}

static inline bool validate_retained_record(const RetainedSpriteTransform2DRecord &record)
{
    return record.reserved == 0u
        && record.angle_cdeg < ANGLE_CDEG_WRAP
        && record.zoom_x1024 != 0u;
}

static bool approx_retained_transform_may_touch(
    lgfx::LGFXBase *src,
    lgfx::LGFXBase *dst,
    int16_t x,
    int16_t y,
    uint16_t zoom_x1024)
{
    if (!src || !dst || zoom_x1024 == 0u) {
        return true;
    }

    const float src_w = static_cast<float>(src->width());
    const float src_h = static_cast<float>(src->height());
    const float dst_w = static_cast<float>(dst->width());
    const float dst_h = static_cast<float>(dst->height());

    if (src_w <= 0.0f || src_h <= 0.0f || dst_w <= 0.0f || dst_h <= 0.0f) {
        return true;
    }

    const float zoom = static_cast<float>(zoom_x1024) / 1024.0f;
    const float half_w = ((src_w * zoom) + (src_h * zoom)) * 0.5f + 1.0f;
    const float half_h = half_w;
    const float center_x = static_cast<float>(x);
    const float center_y = static_cast<float>(y);

    return (center_x + half_w) >= 0.0f
        && (center_y + half_h) >= 0.0f
        && (center_x - half_w) < dst_w
        && (center_y - half_h) < dst_h;
}

static inline uint16_t wrap_angle_cdeg(int32_t angle_cdeg)
{
    int32_t wrapped = angle_cdeg % static_cast<int32_t>(ANGLE_CDEG_WRAP);
    if (wrapped < 0) {
        wrapped += static_cast<int32_t>(ANGLE_CDEG_WRAP);
    }

    return static_cast<uint16_t>(wrapped);
}

static inline int16_t positive_i16(int16_t value)
{
    if (value == INT16_MIN) {
        return INT16_MAX;
    }

    return value < 0 ? static_cast<int16_t>(-value) : value;
}

static inline int16_t negative_i16(int16_t value)
{
    if (value == INT16_MIN) {
        return INT16_MIN;
    }

    return value > 0 ? static_cast<int16_t>(-value) : value;
}

static inline void bounce_axis_i16(int16_t *pos, int16_t *delta, int32_t min_v, int32_t max_v)
{
    if (!pos || !delta) {
        return;
    }

    const int32_t next_pos = static_cast<int32_t>(*pos) + static_cast<int32_t>(*delta);

    if (next_pos < min_v) {
        *pos = static_cast<int16_t>(min_v);
        *delta = positive_i16(*delta);
        return;
    }

    if (next_pos > max_v) {
        *pos = static_cast<int16_t>(max_v);
        *delta = negative_i16(*delta);
        return;
    }

    *pos = static_cast<int16_t>(next_pos);
}

static inline void bounce_zoom_i16(
    uint16_t *zoom_x1024,
    int16_t *dzoom_x1024,
    uint16_t min_zoom_x1024,
    uint16_t max_zoom_x1024)
{
    if (!zoom_x1024 || !dzoom_x1024) {
        return;
    }

    const int32_t next_zoom = static_cast<int32_t>(*zoom_x1024) + static_cast<int32_t>(*dzoom_x1024);

    if (next_zoom < static_cast<int32_t>(min_zoom_x1024)) {
        *zoom_x1024 = min_zoom_x1024;
        *dzoom_x1024 = positive_i16(*dzoom_x1024);
        return;
    }

    if (next_zoom > static_cast<int32_t>(max_zoom_x1024)) {
        *zoom_x1024 = max_zoom_x1024;
        *dzoom_x1024 = negative_i16(*dzoom_x1024);
        return;
    }

    *zoom_x1024 = static_cast<uint16_t>(next_zoom);
}

static void clear_program_runtime_state(RetainedRenderProgram *program)
{
    if (!program) {
        return;
    }

    for (uint8_t i = 0u; i < MAX_RENDER_PROGRAM_SOURCES; i++) {
        program->source_sprites[i] = nullptr;
    }

    program->task_handle = nullptr;
    program->running = false;
    program->stop_requested = false;
    program->stats.running = false;

    const uint8_t handle = handle_from_render_program(program);
    if (handle != 0u && g_active_render_program_handle == handle) {
        g_active_render_program_handle = 0u;
    }
}

static esp_err_t resolve_program_source_sprites_locked(RetainedRenderProgram *program)
{
    if (!program || !program->allocated) {
        return ESP_ERR_NOT_FOUND;
    }

    if (!valid_render_program_type(program->type_id) || program->source_count == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0u; i < program->source_count; i++) {
        const uint8_t src_handle = program->source_handles[i];
        if (!lgfx_device_is_sprite_target(src_handle)) {
            return ESP_ERR_INVALID_ARG;
        }

        auto *src = lgfx_dev::resolve_sprite_locked(src_handle);
        if (!src) {
            return ESP_ERR_NOT_FOUND;
        }

        program->source_sprites[i] = src;
    }

    return ESP_OK;
}

static esp_err_t render_program_frame_locked(
    RetainedRenderProgram *program,
    RetainedObjectBuffer *buffer,
    uint16_t *out_drawn_count,
    uint16_t *out_culled_count,
    int32_t *out_draw_us,
    int32_t *out_present_us)
{
    if (!program || !buffer || !buffer->allocated) {
        return ESP_ERR_INVALID_STATE;
    }

    if (buffer->layout_id != LGFX_OBJECT_BUFFER_LAYOUT_SPRITE_TRANSFORM_2D) {
        return ESP_ERR_INVALID_ARG;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint16_t frame_height = program->frame_height;
    const uint16_t strip_height = program->effective_strip_height;
    if (frame_height == 0u || strip_height == 0u) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t drawn_count = 0u;
    uint16_t culled_count = 0u;
    int64_t draw_total_us = 0;
    int64_t present_total_us = 0;

    auto *records = reinterpret_cast<RetainedSpriteTransform2DRecord *>(buffer->bytes);

    for (uint16_t y0 = 0u; y0 < frame_height; y0 = static_cast<uint16_t>(y0 + strip_height)) {
        esp_err_t err = lgfx_dev::presentation_begin_strip_locked(y0);
        if (err != ESP_OK) {
            return err;
        }

        auto *dst = lgfx_dev::resolve_render_surface_locked(0u);
        if (!dst) {
            (void) lgfx_dev::presentation_cancel_strip_locked();
            return ESP_ERR_INVALID_STATE;
        }

        dst->clear(program->background_color);

        const int64_t draw_started_at_us = esp_timer_get_time();

        for (uint16_t i = 0u; i < buffer->count; i++) {
            RetainedSpriteTransform2DRecord &record = records[i];

            if (!validate_retained_record(record) || record.source_index >= program->source_count) {
                culled_count++;
                continue;
            }

            auto *src = program->source_sprites[record.source_index];
            if (!src) {
                culled_count++;
                continue;
            }

            const int32_t shifted_y32 = static_cast<int32_t>(record.y) - static_cast<int32_t>(y0);
            if (shifted_y32 < INT16_MIN || shifted_y32 > INT16_MAX) {
                culled_count++;
                continue;
            }

            const int16_t shifted_y = static_cast<int16_t>(shifted_y32);
            if (!approx_retained_transform_may_touch(src, dst, record.x, shifted_y, record.zoom_x1024)) {
                culled_count++;
                continue;
            }

            const float angle = static_cast<float>(record.angle_cdeg) / 100.0f;
            const float zoom = static_cast<float>(record.zoom_x1024) / 1024.0f;

            if (program->has_transparent) {
                src->pushRotateZoom(
                    dst,
                    static_cast<float>(record.x),
                    static_cast<float>(shifted_y),
                    angle,
                    zoom,
                    zoom,
                    static_cast<uint32_t>(program->transparent_value));
            } else {
                src->pushRotateZoom(
                    dst,
                    static_cast<float>(record.x),
                    static_cast<float>(shifted_y),
                    angle,
                    zoom,
                    zoom);
            }

            drawn_count++;
        }

        draw_total_us += (esp_timer_get_time() - draw_started_at_us);

        const int64_t present_started_at_us = esp_timer_get_time();
        err = lgfx_dev::presentation_present_strip_locked();
        if (err != ESP_OK) {
            (void) lgfx_dev::presentation_cancel_strip_locked();
            return err;
        }

        present_total_us += (esp_timer_get_time() - present_started_at_us);
    }

    const int64_t display_started_at_us = esp_timer_get_time();
    lcd->display();
    present_total_us += (esp_timer_get_time() - display_started_at_us);

    if (out_drawn_count) {
        *out_drawn_count = drawn_count;
    }
    if (out_culled_count) {
        *out_culled_count = culled_count;
    }
    if (out_draw_us) {
        *out_draw_us = static_cast<int32_t>(draw_total_us);
    }
    if (out_present_us) {
        *out_present_us = static_cast<int32_t>(present_total_us);
    }

    return ESP_OK;
}

static void apply_bounce_update_locked(RetainedRenderProgram *program, RetainedObjectBuffer *buffer)
{
    if (!program || !buffer || !buffer->allocated) {
        return;
    }

    if (program->update_policy != LGFX_RENDER_PROGRAM_UPDATE_BOUNCE
        || buffer->layout_id != LGFX_OBJECT_BUFFER_LAYOUT_SPRITE_TRANSFORM_2D
        || buffer->count == 0u) {
        return;
    }

    const int32_t max_x = (program->frame_width == 0u)
        ? 0
        : static_cast<int32_t>(program->frame_width) - 1;
    const int32_t max_y = (program->frame_height == 0u)
        ? 0
        : static_cast<int32_t>(program->frame_height) - 1;

    auto *records = reinterpret_cast<RetainedSpriteTransform2DRecord *>(buffer->bytes);

    for (uint16_t i = 0u; i < buffer->count; i++) {
        RetainedSpriteTransform2DRecord &record = records[i];

        if (!validate_retained_record(record)) {
            continue;
        }

        record.angle_cdeg = wrap_angle_cdeg(
            static_cast<int32_t>(record.angle_cdeg) + static_cast<int32_t>(record.dangle_cdeg));

        bounce_axis_i16(&record.x, &record.vx, 0, max_x);
        bounce_axis_i16(&record.y, &record.vy, 0, max_y);
        bounce_zoom_i16(
            &record.zoom_x1024,
            &record.dzoom_x1024,
            program->zoom_min_x1024,
            program->zoom_max_x1024);
    }
}

static void render_program_task(void *arg)
{
    auto *program = static_cast<RetainedRenderProgram *>(arg);
    const uint8_t handle = handle_from_render_program(program);

    if (!program || handle == 0u) {
        vTaskDelete(nullptr);
        return;
    }

    while (!program->stop_requested) {
        const int64_t frame_started_at_us = esp_timer_get_time();

        uint16_t drawn_count = 0u;
        uint16_t culled_count = 0u;
        int32_t draw_us = 0;
        int32_t present_us = 0;
        int32_t update_us = 0;

        esp_err_t frame_err = ESP_OK;

        {
            lgfx_dev::ScopedLcdLock lock;
            frame_err = lgfx_dev::lock_published_ready_ignoring_exclusive(lock);

            if (frame_err == ESP_OK) {
                RetainedObjectBuffer *buffer = lookup_object_buffer(program->object_buffer_handle);
                if (!buffer || !buffer->allocated) {
                    frame_err = ESP_ERR_INVALID_STATE;
                } else {
                    frame_err = render_program_frame_locked(
                        program,
                        buffer,
                        &drawn_count,
                        &culled_count,
                        &draw_us,
                        &present_us);

                    if (frame_err == ESP_OK) {
                        const int64_t update_started_at_us = esp_timer_get_time();
                        apply_bounce_update_locked(program, buffer);
                        update_us = static_cast<int32_t>(esp_timer_get_time() - update_started_at_us);

                        program->stats.running = true;
                        program->stats.frame_count++;
                        program->stats.object_count = buffer->count;
                        program->stats.drawn_count = drawn_count;
                        program->stats.culled_count = culled_count;
                        program->stats.strip_height = program->effective_strip_height;
                        program->stats.last_draw_us = draw_us;
                        program->stats.last_present_us = present_us;
                        program->stats.last_update_us = update_us;
                        program->stats.last_frame_us =
                            static_cast<int32_t>(esp_timer_get_time() - frame_started_at_us);
                    }
                }
            }
        }

        if (frame_err != ESP_OK) {
            break;
        }

        // Block for one tick so the idle task can run and feed the task watchdog.
        vTaskDelay(RENDER_PROGRAM_FRAME_IDLE_TICKS);
    }

    {
        lgfx_dev::ScopedLcdLock lock;
        if (lgfx_dev::lock_published_ready_ignoring_exclusive(lock) == ESP_OK) {
            (void) lgfx_dev::end_write_locked();
        }
    }

    clear_program_runtime_state(program);
    vTaskDelete(nullptr);
}

static esp_err_t stop_render_program_internal(RetainedRenderProgram *program)
{
    if (!program || !program->allocated) {
        return ESP_ERR_NOT_FOUND;
    }

    if (!program->running) {
        clear_program_runtime_state(program);
        return ESP_OK;
    }

    program->stop_requested = true;

    const TickType_t started_at = xTaskGetTickCount();
    while (program->running) {
        if ((xTaskGetTickCount() - started_at) > RENDER_PROGRAM_STOP_TIMEOUT_TICKS) {
            return ESP_ERR_INVALID_STATE;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }

    return ESP_OK;
}

} // namespace

namespace lgfx_dev
{

bool retained_renderer_running()
{
    return g_active_render_program_handle != 0u;
}

bool retained_sprite_in_use_locked(uint8_t handle)
{
    if (!lgfx_device_is_sprite_target(handle)) {
        return false;
    }

    for (uint8_t program_handle = 1u; program_handle <= MAX_RENDER_PROGRAM_HANDLES; program_handle++) {
        const RetainedRenderProgram &program = g_render_programs[program_handle];
        if (!program.allocated) {
            continue;
        }

        for (uint8_t i = 0u; i < program.source_count; i++) {
            if (program.source_handles[i] == handle) {
                return true;
            }
        }
    }

    return false;
}

bool retained_object_buffer_in_use_locked(uint8_t handle)
{
    if (handle == 0u || handle > MAX_OBJECT_BUFFER_HANDLES) {
        return false;
    }

    for (uint8_t program_handle = 1u; program_handle <= MAX_RENDER_PROGRAM_HANDLES; program_handle++) {
        const RetainedRenderProgram &program = g_render_programs[program_handle];
        if (program.allocated && program.object_buffer_handle == handle) {
            return true;
        }
    }

    return false;
}

esp_err_t retained_stop_active_renderer()
{
    if (g_active_render_program_handle == 0u) {
        return ESP_OK;
    }

    RetainedRenderProgram *program = lookup_render_program(g_active_render_program_handle);
    return stop_render_program_internal(program);
}

void retained_destroy_all_locked()
{
    g_active_render_program_handle = 0u;

    for (uint8_t handle = 1u; handle <= MAX_RENDER_PROGRAM_HANDLES; handle++) {
        RetainedRenderProgram &program = g_render_programs[handle];
        clear_program_runtime_state(&program);
        program.allocated = false;
        program.type_id = 0u;
        program.object_buffer_handle = 0u;
        program.update_policy = 0u;
        program.has_transparent = false;
        program.transparent_value = 0u;
        program.background_color = 0u;
        program.requested_strip_height = 0u;
        program.effective_strip_height = 0u;
        program.zoom_min_x1024 = 0u;
        program.zoom_max_x1024 = 0u;
        program.frame_width = 0u;
        program.frame_height = 0u;
        program.source_count = 0u;
        memset(program.source_handles, 0, sizeof(program.source_handles));
        program.stats = {};
    }

    for (uint8_t handle = 1u; handle <= MAX_OBJECT_BUFFER_HANDLES; handle++) {
        RetainedObjectBuffer &buffer = g_object_buffers[handle];
        free(buffer.bytes);
        buffer.bytes = nullptr;
        buffer.allocated = false;
        buffer.layout_id = 0u;
        buffer.capacity = 0u;
        buffer.count = 0u;
    }
}

} // namespace lgfx_dev

extern "C" esp_err_t lgfx_device_retained_object_buffer_create(
    uint8_t layout_id,
    uint16_t capacity,
    uint8_t *out_handle)
{
    if (!out_handle || capacity == 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t record_size = layout_record_size(layout_id);
    size_t total_bytes = 0u;
    if (record_size == 0u || !safe_capacity_bytes(capacity, record_size, &total_bytes)) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_published_ready_ignoring_exclusive(lock);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t handle = 0u;
    for (uint8_t i = 1u; i <= MAX_OBJECT_BUFFER_HANDLES; i++) {
        if (!g_object_buffers[i].allocated) {
            handle = i;
            break;
        }
    }

    if (handle == 0u) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t *bytes = static_cast<uint8_t *>(malloc(total_bytes));
    if (!bytes) {
        return ESP_ERR_NO_MEM;
    }

    memset(bytes, 0, total_bytes);

    RetainedObjectBuffer &buffer = g_object_buffers[handle];
    buffer.allocated = true;
    buffer.layout_id = layout_id;
    buffer.capacity = capacity;
    buffer.count = 0u;
    buffer.bytes = bytes;

    *out_handle = handle;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_retained_object_buffer_write(
    uint8_t handle,
    const uint8_t *records,
    size_t records_len)
{
    RetainedObjectBuffer *buffer = lookup_object_buffer(handle);
    if (!buffer || !buffer->allocated) {
        return ESP_ERR_NOT_FOUND;
    }

    const size_t record_size = layout_record_size(buffer->layout_id);
    if (record_size == 0u) {
        return ESP_ERR_INVALID_STATE;
    }

    if (records_len == 0u) {
        lgfx_dev::ScopedLcdLock lock;
        esp_err_t err = lgfx_dev::lock_published_ready_ignoring_exclusive(lock);
        if (err != ESP_OK) {
            return err;
        }

        buffer->count = 0u;
        return ESP_OK;
    }

    if (!records || (records_len % record_size) != 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t record_count = records_len / record_size;
    if (record_count > buffer->capacity) {
        return ESP_ERR_INVALID_ARG;
    }

    if (buffer->layout_id == LGFX_OBJECT_BUFFER_LAYOUT_SPRITE_TRANSFORM_2D) {
        const auto *typed_records = reinterpret_cast<const RetainedSpriteTransform2DRecord *>(records);
        for (size_t i = 0u; i < record_count; i++) {
            if (!validate_retained_record(typed_records[i])) {
                return ESP_ERR_INVALID_ARG;
            }
        }
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_published_ready_ignoring_exclusive(lock);
    if (err != ESP_OK) {
        return err;
    }

    memcpy(buffer->bytes, records, records_len);
    buffer->count = static_cast<uint16_t>(record_count);
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_retained_object_buffer_delete(uint8_t handle)
{
    RetainedObjectBuffer *buffer = lookup_object_buffer(handle);
    if (!buffer || !buffer->allocated) {
        return ESP_ERR_NOT_FOUND;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_published_ready_ignoring_exclusive(lock);
    if (err != ESP_OK) {
        return err;
    }

    if (lgfx_dev::retained_object_buffer_in_use_locked(handle)) {
        return ESP_ERR_INVALID_STATE;
    }

    free(buffer->bytes);
    buffer->bytes = nullptr;
    buffer->allocated = false;
    buffer->layout_id = 0u;
    buffer->capacity = 0u;
    buffer->count = 0u;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_retained_render_program_create(
    uint8_t type_id,
    uint8_t object_buffer_handle,
    const uint8_t *source_handles,
    size_t source_count,
    uint16_t strip_height,
    uint32_t background_color,
    bool has_transparent,
    uint16_t transparent_value,
    uint8_t update_policy,
    uint16_t zoom_min_x1024,
    uint16_t zoom_max_x1024,
    uint8_t *out_handle)
{
    if (!out_handle
        || !valid_render_program_type(type_id)
        || source_count == 0u
        || source_count > MAX_RENDER_PROGRAM_SOURCES
        || !source_handles
        || strip_height == 0u
        || background_color > 0xFFFFu
        || (!has_transparent && transparent_value != 0u)
        || !valid_render_program_update_policy(update_policy)
        || zoom_min_x1024 == 0u
        || zoom_min_x1024 > zoom_max_x1024) {
        return ESP_ERR_INVALID_ARG;
    }

    RetainedObjectBuffer *buffer = lookup_object_buffer(object_buffer_handle);
    if (!buffer || !buffer->allocated) {
        return ESP_ERR_NOT_FOUND;
    }

    if (buffer->layout_id != LGFX_OBJECT_BUFFER_LAYOUT_SPRITE_TRANSFORM_2D) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_published_ready_ignoring_exclusive(lock);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t handle = 0u;
    for (uint8_t i = 1u; i <= MAX_RENDER_PROGRAM_HANDLES; i++) {
        if (!g_render_programs[i].allocated) {
            handle = i;
            break;
        }
    }

    if (handle == 0u) {
        return ESP_ERR_NO_MEM;
    }

    RetainedRenderProgram &program = g_render_programs[handle];
    program = {};
    program.allocated = true;
    program.type_id = type_id;
    program.object_buffer_handle = object_buffer_handle;
    program.update_policy = update_policy;
    program.has_transparent = has_transparent;
    program.transparent_value = transparent_value;
    program.background_color = static_cast<uint16_t>(background_color);
    program.requested_strip_height = strip_height;
    program.zoom_min_x1024 = zoom_min_x1024;
    program.zoom_max_x1024 = zoom_max_x1024;
    program.source_count = static_cast<uint8_t>(source_count);
    program.stats = {};

    for (size_t i = 0u; i < source_count; i++) {
        const uint8_t src_handle = source_handles[i];
        if (!lgfx_device_is_sprite_target(src_handle)
            || lgfx_dev::resolve_sprite_locked(src_handle) == nullptr) {
            program = {};
            return ESP_ERR_NOT_FOUND;
        }

        program.source_handles[i] = src_handle;
    }

    *out_handle = handle;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_retained_render_program_start(uint8_t handle, uint8_t mode)
{
    if (!valid_render_program_mode(mode)) {
        return ESP_ERR_INVALID_ARG;
    }

    RetainedRenderProgram *program = lookup_render_program(handle);
    if (!program || !program->allocated) {
        return ESP_ERR_NOT_FOUND;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_published_ready_ignoring_exclusive(lock);
    if (err != ESP_OK) {
        return err;
    }

    if (g_active_render_program_handle != 0u && g_active_render_program_handle != handle) {
        return ESP_ERR_INVALID_STATE;
    }

    if (program->running) {
        return ESP_OK;
    }

    RetainedObjectBuffer *buffer = lookup_object_buffer(program->object_buffer_handle);
    if (!buffer || !buffer->allocated) {
        return ESP_ERR_NOT_FOUND;
    }

    err = resolve_program_source_sprites_locked(program);
    if (err != ESP_OK) {
        return err;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    (void) lgfx_dev::presentation_destroy_buffers_locked();
    (void) lgfx_dev::presentation_configure_locked(
        static_cast<uint16_t>(lcd->width()),
        static_cast<uint16_t>(lcd->height()),
        program->requested_strip_height);

    err = lgfx_dev::presentation_ensure_buffers_locked();
    if (err != ESP_OK) {
        return err;
    }

    program->effective_strip_height = lgfx_dev::presentation_strip_height_locked();
    program->frame_width = static_cast<uint16_t>(lcd->width());
    program->frame_height = static_cast<uint16_t>(lcd->height());
    program->stats = {};
    program->stats.running = true;
    program->stats.object_count = buffer->count;
    program->stats.strip_height = program->effective_strip_height;
    program->stop_requested = false;
    program->running = true;
    g_active_render_program_handle = handle;

    err = lgfx_dev::start_write_locked();
    if (err != ESP_OK) {
        clear_program_runtime_state(program);
        return err;
    }

    const BaseType_t created = xTaskCreate(
        render_program_task,
        "lgfx_rr",
        RENDER_PROGRAM_TASK_STACK_WORDS,
        program,
        RENDER_PROGRAM_TASK_PRIORITY,
        &program->task_handle);

    if (created != pdPASS) {
        (void) lgfx_dev::end_write_locked();
        clear_program_runtime_state(program);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_retained_render_program_stop(uint8_t handle)
{
    RetainedRenderProgram *program = lookup_render_program(handle);
    return stop_render_program_internal(program);
}

extern "C" esp_err_t lgfx_device_retained_render_program_stats(
    uint8_t handle,
    lgfx_retained_render_program_stats_t *out_stats)
{
    if (!out_stats) {
        return ESP_ERR_INVALID_ARG;
    }

    RetainedRenderProgram *program = lookup_render_program(handle);
    if (!program || !program->allocated) {
        return ESP_ERR_NOT_FOUND;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_published_ready_ignoring_exclusive(lock);
    if (err != ESP_OK) {
        return err;
    }

    *out_stats = program->stats;
    out_stats->running = program->running;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_retained_render_program_destroy(uint8_t handle)
{
    RetainedRenderProgram *program = lookup_render_program(handle);
    if (!program || !program->allocated) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = stop_render_program_internal(program);
    if (err != ESP_OK) {
        return err;
    }

    lgfx_dev::ScopedLcdLock lock;
    err = lgfx_dev::lock_published_ready_ignoring_exclusive(lock);
    if (err != ESP_OK) {
        return err;
    }

    *program = {};
    return ESP_OK;
}

extern "C" bool lgfx_device_retained_renderer_running(void)
{
    return lgfx_dev::retained_renderer_running();
}
