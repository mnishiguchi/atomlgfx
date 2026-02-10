// src/lgfx_device_control.cpp
// LCD-only control APIs (rotation, brightness, display).

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

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
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) {
        // For TFT this is typically a no-op / "display on" depending on panel.
        d->display();
    });
}
