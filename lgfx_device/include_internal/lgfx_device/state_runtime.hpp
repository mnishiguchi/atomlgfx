// SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
//
// SPDX-License-Identifier: Apache-2.0

// lgfx_device/include_internal/lgfx_device/state_runtime.hpp
//
// Internal-only runtime config + concrete LCD construction helpers.
// This header is C++-only and must not be exposed as public API.

#ifndef LGFX_DEVICE_STATE_RUNTIME_HPP
#define LGFX_DEVICE_STATE_RUNTIME_HPP

#include <stdint.h>

#include <LovyanGFX.hpp>

#include "lgfx_device/lgfx_device.h"

namespace lgfx_dev
{

struct LgfxRuntimeConfig
{
    struct SpiBusConfig
    {
        int host;
        uint8_t mode;
        uint32_t freq_write_hz;
        uint32_t freq_read_hz;
        int dma_channel;
        bool spi_3wire;
        bool use_lock;
        int pin_sclk;
        int pin_mosi;
        int pin_miso;
        int pin_dc;
    };

    struct PanelConfig
    {
        lgfx_panel_driver_id_t driver_id;
        const char *driver_name;
        uint16_t width;
        uint16_t height;
        int pin_cs;
        int pin_rst;
        int pin_busy;
        int offset_x;
        int offset_y;
        uint8_t offset_rotation;
        uint8_t dummy_read_pixel;
        uint8_t dummy_read_bits;
        bool readable;
        bool invert;
        bool rgb_order;
        bool dlen_16bit;
        bool bus_shared;
    };

    struct TouchConfig
    {
        bool compiled;
        bool attached;
        int pin_cs;
        int pin_irq;
        int spi_host;
        uint32_t spi_freq_hz;
        uint8_t offset_rotation;
        bool bus_shared;
    };

    SpiBusConfig lcd_bus;
    PanelConfig lcd_panel;
    TouchConfig touch;
};

bool validate_runtime_config(const LgfxRuntimeConfig &config, const char **reason);

LgfxRuntimeConfig runtime_config_with_overrides(const lgfx_open_config_overrides_t *overrides);

void log_runtime_config(const LgfxRuntimeConfig &config);

// Creates the concrete LGFX device instance for the effective runtime config.
// Returns nullptr on allocation/configuration failure.
lgfx::LGFX_Device *create_lcd_device(const LgfxRuntimeConfig &config);

// Destroys a device created by create_lcd_device().
void destroy_lcd_device(lgfx::LGFX_Device *device);

} // namespace lgfx_dev

#endif // LGFX_DEVICE_STATE_RUNTIME_HPP
