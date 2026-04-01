// SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
//
// SPDX-License-Identifier: Apache-2.0

#include "lgfx_device/lgfx_device.h"
#include "lgfx_device/lgfx_device_internal.hpp"

namespace
{

static esp_err_t set_clip_rect_common(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h,
    bool already_locked)
{
    if (w == 0 || h == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (already_locked) {
        return lgfx_dev::with_render_target_locked(target, [&](lgfx::LGFXBase *gfx) {
            gfx->setClipRect(x, y, w, h);
        });
    }

    return lgfx_dev::with_render_target(target, [&](lgfx::LGFXBase *gfx) {
        gfx->setClipRect(x, y, w, h);
    });
}

static esp_err_t clear_clip_rect_common(uint8_t target, bool already_locked)
{
    if (already_locked) {
        return lgfx_dev::with_render_target_locked(target, [&](lgfx::LGFXBase *gfx) {
            gfx->clearClipRect();
        });
    }

    return lgfx_dev::with_render_target(target, [&](lgfx::LGFXBase *gfx) {
        gfx->clearClipRect();
    });
}

} // namespace

esp_err_t lgfx_dev::set_clip_rect_locked(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h)
{
    return set_clip_rect_common(target, x, y, w, h, true);
}

esp_err_t lgfx_dev::clear_clip_rect_locked(uint8_t target)
{
    return clear_clip_rect_common(target, true);
}

extern "C" esp_err_t lgfx_device_set_clip_rect(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h)
{
    return set_clip_rect_common(target, x, y, w, h, false);
}

extern "C" esp_err_t lgfx_device_clear_clip_rect(uint8_t target)
{
    return clear_clip_rect_common(target, false);
}
