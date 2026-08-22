/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// native/device/sprites.cpp

#include "device.h"
#include "device_internal.hpp"

#include <cmath>
#include <new>

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
#include "esp_timer.h"
#endif

namespace
{

static constexpr size_t SOURCE_SPRITE_CACHE_SIZE = 8u;

struct SourceSpriteCacheEntry
{
    uint8_t handle = 0u;
    lgfx::LGFX_Sprite *sprite = nullptr;
    bool transparent_validated = false;
};

static esp_err_t resolve_source_sprite_cached(
    SourceSpriteCacheEntry *cache,
    uint8_t src_handle,
    lgfx::LGFX_Sprite **out_src,
    bool *out_transparent_validated)
{
    if (!cache || !out_src || !out_transparent_validated) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < SOURCE_SPRITE_CACHE_SIZE; ++i) {
        if (cache[i].sprite && cache[i].handle == src_handle) {
            *out_src = cache[i].sprite;
            *out_transparent_validated = cache[i].transparent_validated;
            return ESP_OK;
        }
    }

    lgfx::LGFX_Sprite *src = lgfx_dev::resolve_sprite_locked(src_handle);
    if (!src) {
        return ESP_ERR_NOT_FOUND;
    }

    for (size_t i = 0; i < SOURCE_SPRITE_CACHE_SIZE; ++i) {
        if (!cache[i].sprite) {
            cache[i].handle = src_handle;
            cache[i].sprite = src;
            cache[i].transparent_validated = false;
            *out_src = src;
            *out_transparent_validated = false;
            return ESP_OK;
        }
    }

    // Cache full. This should be rare for animation examples, but falling back
    // to an uncached resolved pointer is safer than failing a valid frame.
    *out_src = src;
    *out_transparent_validated = false;
    return ESP_OK;
}

static void mark_source_sprite_transparent_validated(
    SourceSpriteCacheEntry *cache,
    uint8_t src_handle)
{
    if (!cache) {
        return;
    }

    for (size_t i = 0; i < SOURCE_SPRITE_CACHE_SIZE; ++i) {
        if (cache[i].sprite && cache[i].handle == src_handle) {
            cache[i].transparent_validated = true;
            return;
        }
    }
}

static inline bool lgfx_rotate_zoom_args_are_valid(float angle, float zoom_x, float zoom_y)
{
    return std::isfinite(angle)
        && std::isfinite(zoom_x)
        && std::isfinite(zoom_y)
        && zoom_x > 0.0f
        && zoom_y > 0.0f;
}

static bool lgfx_approx_transform_may_touch(
    lgfx::LGFXBase *src,
    lgfx::LGFXBase *dst,
    int16_t x,
    int16_t y,
    float zoom_x,
    float zoom_y)
{
    if (!src || !dst) {
        return true;
    }

    const float src_w = static_cast<float>(src->width());
    const float src_h = static_cast<float>(src->height());
    const float dst_w = static_cast<float>(dst->width());
    const float dst_h = static_cast<float>(dst->height());

    if (src_w <= 0.0f || src_h <= 0.0f || dst_w <= 0.0f || dst_h <= 0.0f) {
        return true;
    }

    // Conservative center-pivot approximation used only when explicitly requested.
    // MovingIcons sets the source sprite pivot to its center, which makes this a
    // cheap way to skip objects that cannot affect the current strip sprite.
    const float half_w = ((src_w * zoom_x) + (src_h * zoom_y)) * 0.5f + 1.0f;
    const float half_h = half_w;
    const float center_x = static_cast<float>(x);
    const float center_y = static_cast<float>(y);

    return (center_x + half_w) >= 0.0f
        && (center_y + half_h) >= 0.0f
        && (center_x - half_w) < dst_w
        && (center_y - half_h) < dst_h;
}

static uint16_t read_le_u16(const uint8_t *bytes)
{
    return static_cast<uint16_t>(
        static_cast<uint16_t>(bytes[0])
        | static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8));
}

static int16_t read_le_i16(const uint8_t *bytes)
{
    return static_cast<int16_t>(read_le_u16(bytes));
}

static esp_err_t lgfx_push_sprite_locked_impl(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value)
{
    if (!lgfx_device_is_sprite_target(src_handle)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lgfx_dev::protocol_valid_target(dst_target)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!has_transparent && (transparent_is_index || transparent_value != 0u)) {
        return ESP_ERR_INVALID_ARG;
    }

    auto *src = lgfx_dev::resolve_sprite_locked(src_handle);
    if (!src) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = lgfx_dev::validate_sprite_transparent_scalar(
        src,
        has_transparent,
        transparent_is_index,
        transparent_value);
    if (err != ESP_OK) {
        return err;
    }

    if (lgfx_device_is_lcd_target(dst_target)) {
        auto *render_surface = lgfx_dev::resolve_render_surface_locked(dst_target);
        if (!render_surface) {
            return ESP_ERR_INVALID_STATE;
        }

        if (has_transparent) {
            src->pushSprite(render_surface, x, y, static_cast<uint32_t>(transparent_value));
        } else {
            src->pushSprite(render_surface, x, y);
        }

        return ESP_OK;
    }

    auto *dst_spr = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst_spr) {
        return ESP_ERR_NOT_FOUND;
    }

    if (has_transparent) {
        src->pushSprite(dst_spr, x, y, static_cast<uint32_t>(transparent_value));
    } else {
        src->pushSprite(dst_spr, x, y);
    }

    return ESP_OK;
}

static bool lgfx_sprite_raw16_buffer(
    lgfx::LGFX_Sprite *src,
    const uint16_t **out_pixels,
    uint16_t *out_w,
    uint16_t *out_h)
{
    if (!src || !out_pixels || !out_w || !out_h) {
        return false;
    }

    if (src->getColorDepth() != 16) {
        return false;
    }

    const int32_t width = src->width();
    const int32_t height = src->height();
    if (width <= 0 || height <= 0 || width > UINT16_MAX || height > UINT16_MAX) {
        return false;
    }

    const uint16_t *pixels = static_cast<const uint16_t *>(src->getBuffer());
    if (!pixels) {
        return false;
    }

    *out_pixels = pixels;
    *out_w = static_cast<uint16_t>(width);
    *out_h = static_cast<uint16_t>(height);
    return true;
}

template <typename DstGfx>
static esp_err_t lgfx_push_sprite_resolved_locked_impl(
    lgfx::LGFX_Sprite *src,
    DstGfx *dst,
    int16_t x,
    int16_t y,
    bool has_transparent,
    uint32_t transparent_value)
{
    if (!src || !dst) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint16_t *pixels = nullptr;
    uint16_t width = 0;
    uint16_t height = 0;

    if (lgfx_sprite_raw16_buffer(src, &pixels, &width, &height)) {
        if (has_transparent) {
            if (transparent_value <= UINT16_MAX) {
                dst->pushImage(x, y, width, height, pixels, static_cast<uint32_t>(transparent_value));
                return ESP_OK;
            }
        } else {
            dst->pushImage(x, y, width, height, pixels);
            return ESP_OK;
        }
    }

    if (has_transparent) {
        src->pushSprite(dst, x, y, static_cast<uint32_t>(transparent_value));
    } else {
        src->pushSprite(dst, x, y);
    }

    return ESP_OK;
}

template <typename DstGfx>
static esp_err_t lgfx_push_sprite_list_to_resolved_target_locked(
    DstGfx *dst,
    const uint8_t *instance_records,
    size_t instance_count,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value,
    lgfx_dev::PushSpriteListStats *out_stats)
{
    SourceSpriteCacheEntry source_cache[SOURCE_SPRITE_CACHE_SIZE] = {};

    for (size_t i = 0; i < instance_count; ++i) {
        const uint8_t *record = instance_records + (i * LGFX_DEVICE_SPRITE_PUSH_RECORD_SIZE);
        const uint8_t src_handle = record[0];
        const int16_t x = read_le_i16(record + 2);
        const int16_t y = read_le_i16(record + 4);

        if (out_stats) {
            out_stats->instance_count++;
        }

        if (!lgfx_device_is_sprite_target(src_handle) || record[1] != 0) {
            return ESP_ERR_INVALID_ARG;
        }

        lgfx::LGFX_Sprite *src = nullptr;
        bool transparent_already_validated = false;
        const esp_err_t resolve_err = resolve_source_sprite_cached(
            source_cache,
            src_handle,
            &src,
            &transparent_already_validated);
        if (resolve_err != ESP_OK) {
            return resolve_err;
        }

        if (!transparent_already_validated) {
            const esp_err_t transparent_err = lgfx_dev::validate_sprite_transparent_scalar(
                src,
                has_transparent,
                transparent_is_index,
                transparent_value);
            if (transparent_err != ESP_OK) {
                return transparent_err;
            }
            mark_source_sprite_transparent_validated(source_cache, src_handle);
        }

        const esp_err_t err = lgfx_push_sprite_resolved_locked_impl(
            src,
            dst,
            x,
            y,
            has_transparent,
            transparent_value);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}


static inline bool lgfx_i16_add_u16_checked(int16_t base, uint16_t offset, int16_t *out_value)
{
    if (!out_value) {
        return false;
    }

    const int32_t value = static_cast<int32_t>(base) + static_cast<int32_t>(offset);
    if (value < INT16_MIN || value > INT16_MAX) {
        return false;
    }

    *out_value = static_cast<int16_t>(value);
    return true;
}

template <typename DstGfx>
static esp_err_t lgfx_push_image_region_rows_locked(
    DstGfx *dst,
    int16_t dst_x,
    int16_t dst_y,
    uint16_t src_w,
    uint16_t src_h,
    const uint16_t *src_pixels,
    size_t src_stride_pixels,
    bool has_transparent,
    uint32_t transparent_value)
{
    if (!dst || !src_pixels || src_w == 0 || src_h == 0 || src_stride_pixels < src_w) {
        return ESP_ERR_INVALID_ARG;
    }

    // When the requested region is contiguous, push it as one image instead of
    // issuing one pushImage call per row. This is the common path for whole-sprite
    // blits and for atlas layouts whose variant width matches the atlas stride.
    if (src_stride_pixels == (size_t) src_w) {
        if (has_transparent) {
            dst->pushImage(
                dst_x,
                dst_y,
                src_w,
                src_h,
                src_pixels,
                static_cast<uint32_t>(transparent_value));
        } else {
            dst->pushImage(
                dst_x,
                dst_y,
                src_w,
                src_h,
                src_pixels);
        }

        return ESP_OK;
    }

    for (uint16_t row = 0; row < src_h; ++row) {
        int16_t row_y = 0;
        if (!lgfx_i16_add_u16_checked(dst_y, row, &row_y)) {
            return ESP_ERR_INVALID_ARG;
        }

        const uint16_t *row_pixels = src_pixels + ((size_t) row * src_stride_pixels);
        if (has_transparent) {
            dst->pushImage(
                dst_x,
                row_y,
                src_w,
                1,
                row_pixels,
                static_cast<uint32_t>(transparent_value));
        } else {
            dst->pushImage(
                dst_x,
                row_y,
                src_w,
                1,
                row_pixels);
        }
    }

    return ESP_OK;
}

template <typename DstGfx>
static esp_err_t lgfx_push_sprite_region_list_to_resolved_target_locked(
    DstGfx *dst,
    const uint8_t *instance_records,
    size_t instance_count,
    bool has_transparent,
    uint32_t transparent_value,
    lgfx_dev::PushSpriteRegionListStats *out_stats)
{
    if (has_transparent && transparent_value > UINT16_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t cached_src_handle = 0u;
    lgfx::LGFX_Sprite *cached_src = nullptr;
    const uint16_t *cached_buffer = nullptr;
    int32_t cached_width = 0;
    int32_t cached_height = 0;

    for (size_t i = 0; i < instance_count; ++i) {
        const uint8_t *record = instance_records + (i * LGFX_DEVICE_SPRITE_REGION_RECORD_SIZE);
        const uint8_t src_handle = record[0];
        const uint16_t src_x = read_le_u16(record + 2);
        const uint16_t src_y = read_le_u16(record + 4);
        const uint16_t src_w = read_le_u16(record + 6);
        const uint16_t src_h = read_le_u16(record + 8);
        const int16_t dst_x = read_le_i16(record + 10);
        const int16_t dst_y = read_le_i16(record + 12);

        if (out_stats) {
            out_stats->instance_count++;
        }

        if (!lgfx_device_is_sprite_target(src_handle) || record[1] != 0 || src_w == 0 || src_h == 0) {
            return ESP_ERR_INVALID_ARG;
        }

        if (!cached_src || src_handle != cached_src_handle) {
            cached_src = lgfx_dev::resolve_sprite_locked(src_handle);
            cached_src_handle = src_handle;
            cached_buffer = nullptr;
            if (!cached_src) {
                return ESP_ERR_NOT_FOUND;
            }
            if (cached_src->getColorDepth() != 16) {
                return ESP_ERR_NOT_SUPPORTED;
            }

            cached_width = cached_src->width();
            cached_height = cached_src->height();
            if (cached_width <= 0 || cached_height <= 0) {
                return ESP_ERR_INVALID_STATE;
            }
        }

        const uint32_t src_x2 = (uint32_t) src_x + (uint32_t) src_w;
        const uint32_t src_y2 = (uint32_t) src_y + (uint32_t) src_h;
        if (src_x2 > (uint32_t) cached_width || src_y2 > (uint32_t) cached_height) {
            return ESP_ERR_INVALID_ARG;
        }

        const bool covers_whole_source = src_x == 0
            && src_y == 0
            && (uint32_t) src_w == (uint32_t) cached_width
            && (uint32_t) src_h == (uint32_t) cached_height;
        if (covers_whole_source) {
            const esp_err_t err = lgfx_push_sprite_resolved_locked_impl(
                cached_src,
                dst,
                dst_x,
                dst_y,
                has_transparent,
                transparent_value);
            if (err != ESP_OK) {
                return err;
            }

            continue;
        }

        if (!cached_buffer) {
            cached_buffer = static_cast<const uint16_t *>(cached_src->getBuffer());
            if (!cached_buffer) {
                return ESP_ERR_INVALID_STATE;
            }
        }

        const uint16_t *region_pixels =
            cached_buffer + ((size_t) src_y * (size_t) cached_width) + src_x;

        const size_t stride_pixels = (src_x == 0 && src_w == (uint16_t) cached_width)
            ? (size_t) src_w
            : (size_t) cached_width;

        const esp_err_t err = lgfx_push_image_region_rows_locked(
            dst,
            dst_x,
            dst_y,
            src_w,
            src_h,
            region_pixels,
            stride_pixels,
            has_transparent,
            transparent_value);
        if (err != ESP_OK) {
            return err;
        }
    }

    return ESP_OK;
}

template <typename DstGfx>
static esp_err_t lgfx_push_rotate_zoom_resolved_locked_impl(
    lgfx::LGFX_Sprite *src,
    DstGfx *dst,
    int16_t x,
    int16_t y,
    float angle,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    uint32_t transparent_value,
    bool approx_cull,
    bool *out_was_culled)
{
    if (out_was_culled) {
        *out_was_culled = false;
    }

    if (!src || !dst || !lgfx_rotate_zoom_args_are_valid(angle, zoom_x, zoom_y)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (approx_cull && !lgfx_approx_transform_may_touch(src, dst, x, y, zoom_x, zoom_y)) {
        if (out_was_culled) {
            *out_was_culled = true;
        }
        return ESP_OK;
    }

    if (has_transparent) {
        src->pushRotateZoom(
            dst,
            static_cast<float>(x),
            static_cast<float>(y),
            angle,
            zoom_x,
            zoom_y,
            static_cast<uint32_t>(transparent_value));
    } else {
        src->pushRotateZoom(
            dst,
            static_cast<float>(x),
            static_cast<float>(y),
            angle,
            zoom_x,
            zoom_y);
    }

    return ESP_OK;
}

template <typename DstGfx>
static esp_err_t lgfx_push_rotate_zoom_list_to_resolved_target_locked(
    DstGfx *dst,
    const uint8_t *instance_records,
    size_t instance_count,
    int16_t y_offset,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value,
    bool approx_cull,
    lgfx_dev::PushRotateZoomListStats *out_stats)
{
    SourceSpriteCacheEntry source_cache[SOURCE_SPRITE_CACHE_SIZE] = {};

    for (size_t i = 0; i < instance_count; ++i) {
        const uint8_t *record = instance_records + (i * LGFX_DEVICE_SPRITE_TRANSFORM_RECORD_SIZE);
        const uint8_t src_handle = record[0];
        const int16_t x = read_le_i16(record + 2);
        const int16_t y = read_le_i16(record + 4);
        const uint16_t angle_cdeg = read_le_u16(record + 6);
        const uint16_t zoom_x1024 = read_le_u16(record + 8);
        const uint16_t zoom_y1024 = read_le_u16(record + 10);

        if (out_stats) {
            out_stats->instance_count++;
        }

        if (!lgfx_device_is_sprite_target(src_handle)
            || record[1] != 0
            || angle_cdeg >= 36000u
            || zoom_x1024 == 0
            || zoom_y1024 == 0) {
            return ESP_ERR_INVALID_ARG;
        }

        const int32_t shifted_y32 = static_cast<int32_t>(y) - static_cast<int32_t>(y_offset);
        if (shifted_y32 < INT16_MIN || shifted_y32 > INT16_MAX) {
            return ESP_ERR_INVALID_ARG;
        }

        lgfx::LGFX_Sprite *src = nullptr;
        bool transparent_already_validated = false;
        const esp_err_t resolve_err = resolve_source_sprite_cached(
            source_cache,
            src_handle,
            &src,
            &transparent_already_validated);
        if (resolve_err != ESP_OK) {
            return resolve_err;
        }

        if (!transparent_already_validated) {
            const esp_err_t transparent_err = lgfx_dev::validate_sprite_transparent_scalar(
                src,
                has_transparent,
                transparent_is_index,
                transparent_value);
            if (transparent_err != ESP_OK) {
                return transparent_err;
            }
            mark_source_sprite_transparent_validated(source_cache, src_handle);
        }

        bool was_culled = false;
        const esp_err_t err = lgfx_push_rotate_zoom_resolved_locked_impl(
            src,
            dst,
            x,
            static_cast<int16_t>(shifted_y32),
            static_cast<float>(angle_cdeg) / 100.0f,
            static_cast<float>(zoom_x1024) / 1024.0f,
            static_cast<float>(zoom_y1024) / 1024.0f,
            has_transparent,
            transparent_value,
            approx_cull,
            &was_culled);
        if (err != ESP_OK) {
            return err;
        }

        if (out_stats) {
            if (was_culled) {
                out_stats->culled_count++;
            } else {
                out_stats->executed_count++;
            }
        }
    }

    return ESP_OK;
}

static esp_err_t lgfx_push_rotate_zoom_locked_impl(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    float angle,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value,
    bool approx_cull,
    bool *out_was_culled)
{
    if (out_was_culled) {
        *out_was_culled = false;
    }

    if (!lgfx_device_is_sprite_target(src_handle)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lgfx_dev::protocol_valid_target(dst_target)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lgfx_rotate_zoom_args_are_valid(angle, zoom_x, zoom_y)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!has_transparent && (transparent_is_index || transparent_value != 0u)) {
        return ESP_ERR_INVALID_ARG;
    }

    auto *src = lgfx_dev::resolve_sprite_locked(src_handle);
    if (!src) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = lgfx_dev::validate_sprite_transparent_scalar(
        src,
        has_transparent,
        transparent_is_index,
        transparent_value);
    if (err != ESP_OK) {
        return err;
    }

    if (lgfx_device_is_lcd_target(dst_target)) {
        auto *render_surface = lgfx_dev::resolve_render_surface_locked(dst_target);
        if (!render_surface) {
            return ESP_ERR_INVALID_STATE;
        }

        if (approx_cull && !lgfx_approx_transform_may_touch(src, render_surface, x, y, zoom_x, zoom_y)) {
            if (out_was_culled) {
                *out_was_culled = true;
            }
            return ESP_OK;
        }

        if (has_transparent) {
            src->pushRotateZoom(
                render_surface,
                static_cast<float>(x),
                static_cast<float>(y),
                angle,
                zoom_x,
                zoom_y,
                static_cast<uint32_t>(transparent_value));
        } else {
            src->pushRotateZoom(
                render_surface,
                static_cast<float>(x),
                static_cast<float>(y),
                angle,
                zoom_x,
                zoom_y);
        }

        return ESP_OK;
    }

    auto *dst_spr = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst_spr) {
        return ESP_ERR_NOT_FOUND;
    }

    if (approx_cull && !lgfx_approx_transform_may_touch(src, dst_spr, x, y, zoom_x, zoom_y)) {
        if (out_was_culled) {
            *out_was_culled = true;
        }
        return ESP_OK;
    }

    if (has_transparent) {
        src->pushRotateZoom(
            dst_spr,
            static_cast<float>(x),
            static_cast<float>(y),
            angle,
            zoom_x,
            zoom_y,
            static_cast<uint32_t>(transparent_value));
    } else {
        src->pushRotateZoom(
            dst_spr,
            static_cast<float>(x),
            static_cast<float>(y),
            angle,
            zoom_x,
            zoom_y);
    }

    return ESP_OK;
}

} // namespace

// -----------------------------------------------------------------------------
// Internal locked hot-path helpers (batch path)
// -----------------------------------------------------------------------------

esp_err_t lgfx_dev::push_sprite_locked(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t dst_x,
    int16_t dst_y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value)
{
    return lgfx_push_sprite_locked_impl(
        src_handle,
        dst_target,
        dst_x,
        dst_y,
        has_transparent,
        transparent_is_index,
        transparent_value);
}

esp_err_t lgfx_dev::push_sprite_list_locked(
    uint8_t dst_target,
    const uint8_t *instance_records,
    size_t instance_count,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value,
    PushSpriteListStats *out_stats)
{
    if (out_stats) {
        *out_stats = PushSpriteListStats{};
    }

    if (!lgfx_dev::protocol_valid_target(dst_target)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (instance_count == 0 || !instance_records) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!has_transparent && (transparent_is_index || transparent_value != 0u)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (lgfx_device_is_lcd_target(dst_target)) {
        auto *dst = lgfx_dev::resolve_render_surface_locked(dst_target);
        if (!dst) {
            return ESP_ERR_INVALID_STATE;
        }

        return lgfx_push_sprite_list_to_resolved_target_locked(
            dst,
            instance_records,
            instance_count,
            has_transparent,
            transparent_is_index,
            transparent_value,
            out_stats);
    }

    auto *dst = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst) {
        return ESP_ERR_NOT_FOUND;
    }

    return lgfx_push_sprite_list_to_resolved_target_locked(
        dst,
        instance_records,
        instance_count,
        has_transparent,
        transparent_is_index,
        transparent_value,
        out_stats);
}

esp_err_t lgfx_dev::push_sprite_region_list_locked(
    uint8_t dst_target,
    const uint8_t *instance_records,
    size_t instance_count,
    bool has_transparent,
    uint32_t transparent_value,
    PushSpriteRegionListStats *out_stats)
{
    if (out_stats) {
        *out_stats = PushSpriteRegionListStats{};
    }

    if (!lgfx_dev::protocol_valid_target(dst_target)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (instance_count == 0 || !instance_records) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!has_transparent && transparent_value != 0u) {
        return ESP_ERR_INVALID_ARG;
    }

    if (lgfx_device_is_lcd_target(dst_target)) {
        auto *dst = lgfx_dev::resolve_render_surface_locked(dst_target);
        if (!dst) {
            return ESP_ERR_INVALID_STATE;
        }

        return lgfx_push_sprite_region_list_to_resolved_target_locked(
            dst,
            instance_records,
            instance_count,
            has_transparent,
            transparent_value,
            out_stats);
    }

    auto *dst = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst) {
        return ESP_ERR_NOT_FOUND;
    }

    return lgfx_push_sprite_region_list_to_resolved_target_locked(
        dst,
        instance_records,
        instance_count,
        has_transparent,
        transparent_value,
        out_stats);
}

esp_err_t lgfx_dev::push_rotate_zoom_locked(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t dst_x,
    int16_t dst_y,
    float angle,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value,
    bool approx_cull)
{
    return lgfx_push_rotate_zoom_locked_impl(
        src_handle,
        dst_target,
        dst_x,
        dst_y,
        angle,
        zoom_x,
        zoom_y,
        has_transparent,
        transparent_is_index,
        transparent_value,
        approx_cull,
        nullptr);
}

esp_err_t lgfx_dev::push_rotate_zoom_checked_locked(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t dst_x,
    int16_t dst_y,
    float angle,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value,
    bool approx_cull,
    bool *out_was_culled)
{
    return lgfx_push_rotate_zoom_locked_impl(
        src_handle,
        dst_target,
        dst_x,
        dst_y,
        angle,
        zoom_x,
        zoom_y,
        has_transparent,
        transparent_is_index,
        transparent_value,
        approx_cull,
        out_was_culled);
}

esp_err_t lgfx_dev::push_rotate_zoom_list_locked(
    uint8_t dst_target,
    const uint8_t *instance_records,
    size_t instance_count,
    int16_t y_offset,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value,
    bool approx_cull,
    PushRotateZoomListStats *out_stats)
{
    if (out_stats) {
        *out_stats = PushRotateZoomListStats{};
    }

    if (!lgfx_dev::protocol_valid_target(dst_target)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (instance_count == 0 || !instance_records) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!has_transparent && (transparent_is_index || transparent_value != 0u)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (lgfx_device_is_lcd_target(dst_target)) {
        auto *dst = lgfx_dev::resolve_render_surface_locked(dst_target);
        if (!dst) {
            return ESP_ERR_INVALID_STATE;
        }

        return lgfx_push_rotate_zoom_list_to_resolved_target_locked(
            dst,
            instance_records,
            instance_count,
            y_offset,
            has_transparent,
            transparent_is_index,
            transparent_value,
            approx_cull,
            out_stats);
    }

    auto *dst = lgfx_dev::resolve_sprite_locked(dst_target);
    if (!dst) {
        return ESP_ERR_NOT_FOUND;
    }

    return lgfx_push_rotate_zoom_list_to_resolved_target_locked(
        dst,
        instance_records,
        instance_count,
        y_offset,
        has_transparent,
        transparent_is_index,
        transparent_value,
        approx_cull,
        out_stats);
}

esp_err_t lgfx_dev::push_rotate_zoom_frame_strips_locked(
    uint16_t frame_height,
    uint32_t background_color,
    const uint8_t *instance_records,
    size_t instance_count,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value,
    bool approx_cull,
    PushRotateZoomFrameStats *out_stats)
{
    if (out_stats) {
        *out_stats = PushRotateZoomFrameStats{};
    }

    if (frame_height == 0u || instance_count == 0u || !instance_records) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!has_transparent && (transparent_is_index || transparent_value != 0u)) {
        return ESP_ERR_INVALID_ARG;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = lgfx_dev::presentation_ensure_buffers_locked();
    if (err == ESP_ERR_NO_MEM) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (err != ESP_OK) {
        return err;
    }

    const uint16_t strip_height = lgfx_dev::presentation_strip_height_locked();
    if (strip_height == 0u) {
        return ESP_ERR_INVALID_STATE;
    }

    for (uint16_t y0 = 0u; y0 < frame_height;) {
        err = lgfx_dev::presentation_begin_strip_locked(y0);
        if (err != ESP_OK) {
            return err;
        }

        auto *dst = lgfx_dev::resolve_render_surface_locked(0u);
        if (!dst) {
            (void) lgfx_dev::presentation_cancel_strip_locked();
            return ESP_ERR_INVALID_STATE;
        }

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
        const int64_t clear_started_at_us = esp_timer_get_time();
#endif

        dst->clear(background_color);

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
        const int64_t draw_started_at_us = esp_timer_get_time();
#endif

        PushRotateZoomListStats strip_stats{};
        err = lgfx_push_rotate_zoom_list_to_resolved_target_locked(
            dst,
            instance_records,
            instance_count,
            static_cast<int16_t>(y0),
            has_transparent,
            transparent_is_index,
            transparent_value,
            approx_cull,
            &strip_stats);
        if (err != ESP_OK) {
            (void) lgfx_dev::presentation_cancel_strip_locked();
            return err;
        }

#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
        const int64_t present_started_at_us = esp_timer_get_time();
#endif

        err = lgfx_dev::presentation_present_strip_locked();
        if (err != ESP_OK) {
            (void) lgfx_dev::presentation_cancel_strip_locked();
            return err;
        }

        if (out_stats) {
            out_stats->strip_count++;
            out_stats->instance_count += strip_stats.instance_count;
            out_stats->executed_count += strip_stats.executed_count;
            out_stats->culled_count += strip_stats.culled_count;
#if LGFX_PORT_ENABLE_RENDER_BATCH_TRACE
            const int64_t strip_finished_at_us = esp_timer_get_time();
            out_stats->clear_us += draw_started_at_us - clear_started_at_us;
            out_stats->draw_us += present_started_at_us - draw_started_at_us;
            out_stats->present_us += strip_finished_at_us - present_started_at_us;
#endif
        }

        const uint32_t next_y = static_cast<uint32_t>(y0) + static_cast<uint32_t>(strip_height);
        if (next_y > UINT16_MAX) {
            break;
        }
        y0 = static_cast<uint16_t>(next_y);
    }

    return ESP_OK;
}


// -----------------------------------------------------------------------------
// Public C ABI (ordinary sync path)
// -----------------------------------------------------------------------------

extern "C" esp_err_t lgfx_device_sprite_create_at(uint8_t handle, uint16_t w, uint16_t h, uint8_t color_depth)
{
    if (!lgfx_device_is_sprite_target(handle) || w == 0 || h == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    // Enforce advertised sprite capacity even with caller-selected handles.
    if (lgfx_dev::sprite_count_locked() >= static_cast<uint32_t>(lgfx_dev::max_sprites_const())) {
        return ESP_ERR_NO_MEM;
    }

    // Caller-selected handle; reject occupied slots.
    if (lgfx_dev::resolve_sprite_locked(handle) != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    auto *spr = new (std::nothrow) lgfx::LGFX_Sprite(lcd);
    if (!spr) {
        return ESP_ERR_NO_MEM;
    }

    spr->setPsram(lgfx_dev::should_use_psram_sprites());

    if (color_depth != 0) {
        spr->setColorDepth(color_depth);
    }

    spr->createSprite(w, h);
    if (spr->getBuffer() == nullptr) {
        delete spr;
        return ESP_ERR_NO_MEM;
    }

    lgfx_dev::set_sprite_locked(handle, spr);
    lgfx_dev::increment_sprite_count_locked();

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_sprite_delete(uint8_t handle)
{
    if (!lgfx_device_is_sprite_target(handle)) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *spr = lgfx_dev::resolve_sprite_locked(handle);
    if (!spr) {
        return ESP_ERR_NOT_FOUND;
    }

    // Release internal sprite buffers before deleting the object.
    spr->deleteSprite();
    delete spr;

    lgfx_dev::clear_sprite_locked(handle);
    lgfx_dev::decrement_sprite_count_locked();

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_sprite_create_palette(uint8_t handle)
{
    if (!lgfx_device_is_sprite_target(handle)) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *spr = lgfx_dev::resolve_sprite_locked(handle);
    if (!spr) {
        return ESP_ERR_NOT_FOUND;
    }

    if (!lgfx_dev::sprite_supports_palette_storage(spr)) {
        return ESP_ERR_INVALID_ARG;
    }

    spr->createPalette();

    if (!lgfx_dev::sprite_uses_palette_indices(spr)) {
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

namespace lgfx_dev
{

esp_err_t set_palette_color_locked(uint8_t handle, uint8_t palette_index, uint32_t rgb888)
{
    if (!lgfx_device_is_sprite_target(handle) || !scalar_rgb888_is_valid(rgb888)) {
        return ESP_ERR_INVALID_ARG;
    }

    auto *spr = resolve_sprite_locked(handle);
    if (!spr) {
        return ESP_ERR_NOT_FOUND;
    }

    if (!sprite_uses_palette_indices(spr)) {
        return ESP_ERR_INVALID_STATE;
    }

    if (palette_index > sprite_palette_index_max(spr)) {
        return ESP_ERR_INVALID_ARG;
    }

    spr->setPaletteColor(palette_index, rgb888);
    return ESP_OK;
}

esp_err_t set_pivot_locked(uint8_t target, int16_t px, int16_t py)
{
    return with_render_target_locked(target, [&](lgfx::LGFXBase *gfx) {
        gfx->setPivot(px, py);
    });
}

} // namespace lgfx_dev

extern "C" esp_err_t lgfx_device_sprite_set_palette_color(uint8_t handle, uint8_t palette_index, uint32_t rgb888)
{
    if (!lgfx_device_is_sprite_target(handle) || !lgfx_dev::scalar_rgb888_is_valid(rgb888)) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    return lgfx_dev::set_palette_color_locked(handle, palette_index, rgb888);
}

extern "C" esp_err_t lgfx_device_set_pivot(uint8_t target, int16_t px, int16_t py)
{
    if (!lgfx_dev::protocol_valid_target(target)) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    return lgfx_dev::set_pivot_locked(target, px, py);
}

extern "C" esp_err_t lgfx_device_sprite_push_sprite(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value)
{
    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    return lgfx_dev::push_sprite_locked(
        src_handle,
        dst_target,
        x,
        y,
        has_transparent,
        transparent_is_index,
        transparent_value);
}

extern "C" esp_err_t lgfx_device_sprite_push_rotate_zoom(
    uint8_t src_handle,
    uint8_t dst_target,
    int16_t x,
    int16_t y,
    float angle,
    float zoom_x,
    float zoom_y,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value)
{
    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    return lgfx_dev::push_rotate_zoom_locked(
        src_handle,
        dst_target,
        x,
        y,
        angle,
        zoom_x,
        zoom_y,
        has_transparent,
        transparent_is_index,
        transparent_value,
        false);
}

extern "C" esp_err_t lgfx_device_sprite_push_rotate_zoom_list(
    uint8_t dst_target,
    const uint8_t *instance_records,
    size_t instance_count,
    int16_t y_offset,
    bool has_transparent,
    bool transparent_is_index,
    uint32_t transparent_value,
    bool approx_cull)
{
    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    return lgfx_dev::push_rotate_zoom_list_locked(
        dst_target,
        instance_records,
        instance_count,
        y_offset,
        has_transparent,
        transparent_is_index,
        transparent_value,
        approx_cull,
        nullptr);
}
