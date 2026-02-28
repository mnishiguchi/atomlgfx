// src/lgfx_device_api.cpp
// Caps + LCD-only control + size + touch + text APIs.

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <algorithm>
#include <string.h>
#include <utility>

// -----------------------------------------------------------------------------
// Caps / feature discovery (LCD-only)
// -----------------------------------------------------------------------------

extern "C" uint32_t lgfx_device_feature_bits(void)
{
    return lgfx_dev::feature_bits_const();
}

extern "C" uint32_t lgfx_device_max_sprites(void)
{
    return static_cast<uint32_t>(lgfx_dev::max_sprites_const());
}

// -----------------------------------------------------------------------------
// LCD-only control APIs (rotation, brightness, display).
// -----------------------------------------------------------------------------

extern "C" esp_err_t lgfx_device_set_rotation(uint8_t rotation)
{
    if (rotation > 7) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->setRotation(rotation); });
}

extern "C" esp_err_t lgfx_device_set_brightness(uint8_t brightness)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->setBrightness(brightness); });
}

extern "C" esp_err_t lgfx_device_display(void)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->display(); });
}

// -----------------------------------------------------------------------------
// Size query APIs (LCD or sprite target) with pre-init LCD fallback behavior.
// -----------------------------------------------------------------------------

extern "C" uint16_t lgfx_device_width(uint8_t target)
{
    if (!lgfx_dev::lock_lcd()) {
        return (target == 0) ? lgfx_dev::panel_width_const() : 0;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (target == 0 && (!lcd || !lgfx_dev::is_initialized_locked())) {
        lgfx_dev::unlock_lcd();
        return lgfx_dev::panel_width_const();
    }

    lgfx::LGFXBase *gfx = lgfx_dev::resolve_target_locked(target);
    uint16_t w = gfx ? static_cast<uint16_t>(gfx->width()) : 0;

    lgfx_dev::unlock_lcd();

    return w ? w : (target == 0 ? lgfx_dev::panel_width_const() : 0);
}

extern "C" uint16_t lgfx_device_height(uint8_t target)
{
    if (!lgfx_dev::lock_lcd()) {
        return (target == 0) ? lgfx_dev::panel_height_const() : 0;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (target == 0 && (!lcd || !lgfx_dev::is_initialized_locked())) {
        lgfx_dev::unlock_lcd();
        return lgfx_dev::panel_height_const();
    }

    lgfx::LGFXBase *gfx = lgfx_dev::resolve_target_locked(target);
    uint16_t h = gfx ? static_cast<uint16_t>(gfx->height()) : 0;

    lgfx_dev::unlock_lcd();

    return h ? h : (target == 0 ? lgfx_dev::panel_height_const() : 0);
}

extern "C" esp_err_t lgfx_device_get_dims(uint16_t *out_w, uint16_t *out_h)
{
    if (!out_w || !out_h) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!lgfx_dev::lock_lcd()) {
        *out_w = lgfx_dev::panel_width_const();
        *out_h = lgfx_dev::panel_height_const();
        return ESP_OK;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd || !lgfx_dev::is_initialized_locked()) {
        lgfx_dev::unlock_lcd();
        *out_w = lgfx_dev::panel_width_const();
        *out_h = lgfx_dev::panel_height_const();
        return ESP_OK;
    }

    uint16_t w = static_cast<uint16_t>(lcd->width());
    uint16_t h = static_cast<uint16_t>(lcd->height());

    lgfx_dev::unlock_lcd();

    if (w == 0) {
        w = lgfx_dev::panel_width_const();
    }
    if (h == 0) {
        h = lgfx_dev::panel_height_const();
    }

    *out_w = w;
    *out_h = h;
    return ESP_OK;
}

// -----------------------------------------------------------------------------
// Touch (LCD-only by protocol semantics)
// -----------------------------------------------------------------------------

extern "C" esp_err_t lgfx_device_get_touch(
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size)
{
    if (!out_touched || !out_x || !out_y || !out_size) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!lcd->touch()) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    lgfx::touch_point_t tp{};
    const uint32_t n = lcd->getTouch(&tp, 1);

    if (n == 0) {
        *out_touched = false;
        *out_x = 0;
        *out_y = 0;
        *out_size = 0;
        return ESP_OK;
    }

    *out_touched = true;
    *out_x = (int16_t) tp.x;
    *out_y = (int16_t) tp.y;
    *out_size = (uint16_t) tp.size;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_get_touch_raw(
    bool *out_touched,
    int16_t *out_x,
    int16_t *out_y,
    uint16_t *out_size)
{
    if (!out_touched || !out_x || !out_y || !out_size) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!lcd->touch()) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    lgfx::touch_point_t tp{};
    const uint32_t n = lcd->getTouchRaw(&tp, 1);

    if (n == 0) {
        *out_touched = false;
        *out_x = 0;
        *out_y = 0;
        *out_size = 0;
        return ESP_OK;
    }

    *out_touched = true;
    *out_x = (int16_t) tp.x;
    *out_y = (int16_t) tp.y;
    *out_size = (uint16_t) tp.size;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_set_touch_calibrate(const uint16_t params[8])
{
    if (!params) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!lcd->touch()) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint16_t mutable_params[8];
    for (int i = 0; i < 8; i++) {
        mutable_params[i] = params[i];
    }

    lcd->setTouchCalibrate(mutable_params);
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_calibrate_touch(uint16_t out_params[8])
{
    if (!out_params) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *lcd = lgfx_dev::lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!lcd->touch()) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    uint16_t fg = 0xFFFF;
    uint16_t bg = 0x0000;
    if (lcd->isEPD()) {
        std::swap(fg, bg);
    }

    const uint16_t w = (uint16_t) lcd->width();
    const uint16_t h = (uint16_t) lcd->height();
    const uint16_t m = (uint16_t) std::max(w, h);

    uint8_t marker_size = (uint8_t) (m >> 3);
    if (marker_size == 0) {
        marker_size = 1;
    }

    lcd->calibrateTouch(out_params, fg, bg, marker_size);
    return ESP_OK;
}

// -----------------------------------------------------------------------------
// Text configuration and text drawing APIs (LCD or sprite target).
// -----------------------------------------------------------------------------

#if defined(LGFX_PORT_ENABLE_JP_FONTS) && (LGFX_PORT_ENABLE_JP_FONTS == 1)
extern const lgfx::U8g2font ui_font_ja_16_min;
#endif

template <typename G>
static auto set_text_size_xy_impl(G *gfx, uint8_t sx, uint8_t sy, int)
    -> decltype(gfx->setTextSize(sx, sy), void())
{
    gfx->setTextSize(sx, sy);
}

template <typename G>
static void set_text_size_xy_impl(G *gfx, uint8_t sx, uint8_t sy, long)
{
    (void) sy;
    gfx->setTextSize(sx);
}

template <typename G>
static void set_text_size_xy(G *gfx, uint8_t sx, uint8_t sy)
{
    set_text_size_xy_impl(gfx, sx, sy, 0);
}

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

    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) { set_text_size_xy(gfx, sx, sy); });
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
