#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

extern "C" esp_err_t lgfx_device_set_clip_rect(
    uint8_t target,
    int16_t x,
    int16_t y,
    uint16_t w,
    uint16_t h)
{
    if (w == 0 || h == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
        gfx->setClipRect(x, y, w, h);
    });
}

extern "C" esp_err_t lgfx_device_clear_clip_rect(uint8_t target)
{
    return lgfx_dev::with_target(target, [&](lgfx::LGFXBase *gfx) {
        gfx->clearClipRect();
    });
}
