// SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
//
// SPDX-License-Identifier: Apache-2.0

// lgfx_device/state_runtime.cpp

#include <new>
#include <stddef.h>
#include <stdint.h>

#include <LovyanGFX.hpp>

// Generated build config
#include "lgfx_port/lgfx_port_config.h"

#if (LGFX_PORT_ENABLE_TOUCH == 1)
#include <lgfx/v1/touch/Touch_FT5x06.hpp>
#include <lgfx/v1/touch/Touch_XPT2046.hpp>
#endif

#include "esp_log.h"

#include "lgfx_device/state_runtime.hpp"

#ifndef LGFX_PORT_PANEL_DRIVER_ILI9341_2
#error "LGFX_PORT_PANEL_DRIVER_ILI9341_2 must be defined by lgfx_port_config.h"
#endif

#ifndef LGFX_PORT_PANEL_DRIVER_ST7789
#error "LGFX_PORT_PANEL_DRIVER_ST7789 must be defined by lgfx_port_config.h"
#endif

#ifndef LGFX_PORT_PANEL_DRIVER_ILI9342C
#error "LGFX_PORT_PANEL_DRIVER_ILI9342C must be defined by lgfx_port_config.h"
#endif

#define LGFX_PORT_ASSERT_BOOL01(name) \
    static_assert(((name) == 0) || ((name) == 1), #name " must be 0 or 1")

LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_ENABLE_TOUCH);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_PANEL_DRIVER_ILI9488);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_PANEL_DRIVER_ILI9341);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_PANEL_DRIVER_ILI9341_2);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_PANEL_DRIVER_ST7789);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_PANEL_DRIVER_ILI9342C);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_LCD_SPI_3WIRE);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_LCD_USE_LOCK);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_LCD_READABLE);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_LCD_INVERT);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_LCD_RGB_ORDER);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_LCD_DLEN_16BIT);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_LCD_BUS_SHARED);

#if (LGFX_PORT_ENABLE_TOUCH == 1)
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_TOUCH_BUS_SHARED);
#endif

#undef LGFX_PORT_ASSERT_BOOL01

static_assert(
    (LGFX_PORT_PANEL_DRIVER_ILI9488
        + LGFX_PORT_PANEL_DRIVER_ILI9341
        + LGFX_PORT_PANEL_DRIVER_ILI9341_2
        + LGFX_PORT_PANEL_DRIVER_ST7789
        + LGFX_PORT_PANEL_DRIVER_ILI9342C)
        == 1,
    "Exactly one panel driver must be selected");

static_assert((LGFX_PORT_PANEL_WIDTH) >= 1 && (LGFX_PORT_PANEL_WIDTH) <= 65535u,
    "LGFX_PORT_PANEL_WIDTH must be in 1..65535");
static_assert((LGFX_PORT_PANEL_HEIGHT) >= 1 && (LGFX_PORT_PANEL_HEIGHT) <= 65535u,
    "LGFX_PORT_PANEL_HEIGHT must be in 1..65535");
static_assert((LGFX_PORT_LCD_SPI_MODE) >= 0 && (LGFX_PORT_LCD_SPI_MODE) <= 3,
    "LGFX_PORT_LCD_SPI_MODE must be in 0..3");
static_assert((LGFX_PORT_LCD_OFFSET_ROTATION) <= 7u,
    "LGFX_PORT_LCD_OFFSET_ROTATION must be in 0..7");

#if (LGFX_PORT_ENABLE_TOUCH == 1)
static_assert((LGFX_PORT_TOUCH_OFFSET_ROTATION) <= 7u,
    "LGFX_PORT_TOUCH_OFFSET_ROTATION must be 0..7");
#endif

namespace
{

static constexpr const char *TAG = "lgfx_device";

#if (LGFX_PORT_PANEL_DRIVER_ILI9488 == 1)
static constexpr lgfx_panel_driver_id_t BUILD_PANEL_DRIVER_ID = LGFX_PANEL_DRIVER_ID_ILI9488;
#elif (LGFX_PORT_PANEL_DRIVER_ILI9341 == 1)
static constexpr lgfx_panel_driver_id_t BUILD_PANEL_DRIVER_ID = LGFX_PANEL_DRIVER_ID_ILI9341;
#elif (LGFX_PORT_PANEL_DRIVER_ILI9341_2 == 1)
static constexpr lgfx_panel_driver_id_t BUILD_PANEL_DRIVER_ID = LGFX_PANEL_DRIVER_ID_ILI9341_2;
#elif (LGFX_PORT_PANEL_DRIVER_ST7789 == 1)
static constexpr lgfx_panel_driver_id_t BUILD_PANEL_DRIVER_ID = LGFX_PANEL_DRIVER_ID_ST7789;
#elif (LGFX_PORT_PANEL_DRIVER_ILI9342C == 1)
static constexpr lgfx_panel_driver_id_t BUILD_PANEL_DRIVER_ID = LGFX_PANEL_DRIVER_ID_ILI9342C;
#else
#error "Unsupported LGFX panel driver selection"
#endif

static inline const char *panel_driver_name(lgfx_panel_driver_id_t driver_id)
{
    switch (driver_id) {
        case LGFX_PANEL_DRIVER_ID_ILI9488:
            return "ILI9488";
        case LGFX_PANEL_DRIVER_ID_ILI9341:
            return "ILI9341";
        case LGFX_PANEL_DRIVER_ID_ILI9341_2:
            return "ILI9341_2";
        case LGFX_PANEL_DRIVER_ID_ST7789:
            return "ST7789";
        case LGFX_PANEL_DRIVER_ID_ILI9342C:
            return "ILI9342C";
        default:
            return "unknown";
    }
}

static inline bool is_known_panel_driver_id(lgfx_panel_driver_id_t driver_id)
{
    switch (driver_id) {
        case LGFX_PANEL_DRIVER_ID_ILI9488:
        case LGFX_PANEL_DRIVER_ID_ILI9341:
        case LGFX_PANEL_DRIVER_ID_ILI9341_2:
        case LGFX_PANEL_DRIVER_ID_ST7789:
        case LGFX_PANEL_DRIVER_ID_ILI9342C:
            return true;
        default:
            return false;
    }
}


static inline const char *touch_driver_name(lgfx_touch_driver_id_t driver_id)
{
    switch (driver_id) {
        case LGFX_TOUCH_DRIVER_ID_XPT2046:
            return "XPT2046";
        case LGFX_TOUCH_DRIVER_ID_FT6336U:
            return "FT6336U";
        default:
            return "unknown";
    }
}

static inline bool is_known_touch_driver_id(lgfx_touch_driver_id_t driver_id)
{
    switch (driver_id) {
        case LGFX_TOUCH_DRIVER_ID_XPT2046:
        case LGFX_TOUCH_DRIVER_ID_FT6336U:
            return true;
        default:
            return false;
    }
}

static bool touch_driver_is_effectively_attached(const lgfx_dev::LgfxRuntimeConfig::TouchConfig &touch)
{
    if (!touch.compiled) {
        return false;
    }

    switch (touch.driver_id) {
        case LGFX_TOUCH_DRIVER_ID_XPT2046:
            return touch.xpt2046.pin_cs >= 0;
        case LGFX_TOUCH_DRIVER_ID_FT6336U:
            return touch.ft6336u.pin_sda >= 0 && touch.ft6336u.pin_scl >= 0;
        default:
            return false;
    }
}

static void apply_panel_driver_baseline(
    lgfx_dev::LgfxRuntimeConfig &config,
    lgfx_panel_driver_id_t driver_id)
{
    config.lcd_panel.driver_id = driver_id;
    config.lcd_panel.driver_name = panel_driver_name(driver_id);

    switch (driver_id) {
        case LGFX_PANEL_DRIVER_ID_ILI9488:
            config.lcd_panel.width = 320;
            config.lcd_panel.height = 480;
            config.lcd_panel.offset_rotation = 0;
            config.lcd_panel.dummy_read_pixel = 8;
            config.lcd_panel.dummy_read_bits = 1;
            config.lcd_panel.readable = false;
            config.lcd_panel.invert = false;
            config.lcd_panel.rgb_order = false;
            config.lcd_panel.dlen_16bit = false;
            config.touch.offset_rotation = 0;
            break;

        case LGFX_PANEL_DRIVER_ID_ILI9341:
            config.lcd_panel.width = 240;
            config.lcd_panel.height = 320;
            config.lcd_panel.offset_rotation = 0;
            config.lcd_panel.dummy_read_pixel = 8;
            config.lcd_panel.dummy_read_bits = 1;
            config.lcd_panel.readable = false;
            config.lcd_panel.invert = false;
            config.lcd_panel.rgb_order = false;
            config.lcd_panel.dlen_16bit = false;
            config.touch.offset_rotation = 0;
            break;

        case LGFX_PANEL_DRIVER_ID_ILI9341_2:
            config.lcd_panel.width = 240;
            config.lcd_panel.height = 320;
            config.lcd_panel.offset_rotation = 4;
            config.lcd_panel.dummy_read_pixel = 8;
            config.lcd_panel.dummy_read_bits = 1;
            config.lcd_panel.readable = false;
            config.lcd_panel.invert = true;
            config.lcd_panel.rgb_order = false;
            config.lcd_panel.dlen_16bit = false;
            config.touch.offset_rotation = 4;
            break;

        case LGFX_PANEL_DRIVER_ID_ST7789:
            config.lcd_panel.width = 240;
            config.lcd_panel.height = 240;
            config.lcd_panel.offset_rotation = 0;
            config.lcd_panel.dummy_read_pixel = 16;
            config.lcd_panel.dummy_read_bits = 1;
            config.lcd_panel.readable = false;
            config.lcd_panel.invert = false;
            config.lcd_panel.rgb_order = false;
            config.lcd_panel.dlen_16bit = false;
            config.touch.offset_rotation = 0;
            break;

        case LGFX_PANEL_DRIVER_ID_ILI9342C:
            config.lcd_panel.width = 320;
            config.lcd_panel.height = 240;
            config.lcd_panel.offset_rotation = 0;
            config.lcd_panel.dummy_read_pixel = 8;
            config.lcd_panel.dummy_read_bits = 1;
            config.lcd_panel.readable = false;
            config.lcd_panel.invert = false;
            config.lcd_panel.rgb_order = false;
            config.lcd_panel.dlen_16bit = false;
            config.touch.offset_rotation = 0;
            break;

        default:
            break;
    }
}

static lgfx_dev::LgfxRuntimeConfig runtime_config_from_build_defaults()
{
    lgfx_dev::LgfxRuntimeConfig config = {};

    config.lcd_bus.host = (int) (LGFX_PORT_LCD_SPI_HOST);
    config.lcd_bus.mode = (uint8_t) (LGFX_PORT_LCD_SPI_MODE);
    config.lcd_bus.freq_write_hz = (uint32_t) (LGFX_PORT_LCD_FREQ_WRITE_HZ);
    config.lcd_bus.freq_read_hz = (uint32_t) (LGFX_PORT_LCD_FREQ_READ_HZ);
    config.lcd_bus.dma_channel = (int) (LGFX_PORT_LCD_DMA_CHANNEL);
    config.lcd_bus.spi_3wire = ((LGFX_PORT_LCD_SPI_3WIRE) != 0);
    config.lcd_bus.use_lock = ((LGFX_PORT_LCD_USE_LOCK) != 0);
    config.lcd_bus.pin_sclk = (int) (LGFX_PORT_SPI_SCLK_GPIO);
    config.lcd_bus.pin_mosi = (int) (LGFX_PORT_SPI_MOSI_GPIO);
    config.lcd_bus.pin_miso = (int) (LGFX_PORT_SPI_MISO_GPIO);
    config.lcd_bus.pin_dc = (int) (LGFX_PORT_LCD_DC_GPIO);

    config.lcd_panel.driver_id = BUILD_PANEL_DRIVER_ID;
    config.lcd_panel.driver_name = panel_driver_name(BUILD_PANEL_DRIVER_ID);
    config.lcd_panel.width = (uint16_t) (LGFX_PORT_PANEL_WIDTH);
    config.lcd_panel.height = (uint16_t) (LGFX_PORT_PANEL_HEIGHT);
    config.lcd_panel.pin_cs = (int) (LGFX_PORT_LCD_CS_GPIO);
    config.lcd_panel.pin_rst = (int) (LGFX_PORT_LCD_RST_GPIO);
    config.lcd_panel.pin_busy = (int) (LGFX_PORT_LCD_PIN_BUSY);
    config.lcd_panel.offset_x = (int) (LGFX_PORT_LCD_OFFSET_X);
    config.lcd_panel.offset_y = (int) (LGFX_PORT_LCD_OFFSET_Y);
    config.lcd_panel.offset_rotation = (uint8_t) (LGFX_PORT_LCD_OFFSET_ROTATION);
    config.lcd_panel.dummy_read_pixel = (uint8_t) (LGFX_PORT_LCD_DUMMY_READ_PIXEL);
    config.lcd_panel.dummy_read_bits = (uint8_t) (LGFX_PORT_LCD_DUMMY_READ_BITS);
    config.lcd_panel.readable = ((LGFX_PORT_LCD_READABLE) != 0);
    config.lcd_panel.invert = ((LGFX_PORT_LCD_INVERT) != 0);
    config.lcd_panel.rgb_order = ((LGFX_PORT_LCD_RGB_ORDER) != 0);
    config.lcd_panel.dlen_16bit = ((LGFX_PORT_LCD_DLEN_16BIT) != 0);
    config.lcd_panel.bus_shared = ((LGFX_PORT_LCD_BUS_SHARED) != 0);

    config.touch.compiled = ((LGFX_PORT_ENABLE_TOUCH) != 0);
    config.touch.driver_id = LGFX_TOUCH_DRIVER_ID_XPT2046;
    config.touch.driver_name = touch_driver_name(config.touch.driver_id);
    config.touch.offset_rotation = (uint8_t) (LGFX_PORT_TOUCH_OFFSET_ROTATION);
    config.touch.bus_shared = ((LGFX_PORT_TOUCH_BUS_SHARED) != 0);

    config.touch.xpt2046.pin_cs = (int) (LGFX_PORT_TOUCH_CS_GPIO);
    config.touch.xpt2046.pin_irq = (int) (LGFX_PORT_TOUCH_IRQ_GPIO);
    config.touch.xpt2046.spi_host = (int) (LGFX_PORT_TOUCH_SPI_HOST);
    config.touch.xpt2046.spi_freq_hz = (uint32_t) (LGFX_PORT_TOUCH_SPI_FREQ_HZ);

    config.touch.ft6336u.i2c_port = 0;
    config.touch.ft6336u.pin_sda = -1;
    config.touch.ft6336u.pin_scl = -1;
    config.touch.ft6336u.i2c_addr = 0x38u;
    config.touch.ft6336u.pin_irq = -1;
    config.touch.ft6336u.pin_rst = -1;

    config.touch.attached = touch_driver_is_effectively_attached(config.touch);

    return config;
}

static void apply_open_config_overrides(
    lgfx_dev::LgfxRuntimeConfig &config,
    const lgfx_open_config_overrides_t &overrides)
{
    if (overrides.has_panel_driver) {
        apply_panel_driver_baseline(config, overrides.panel_driver);
    }

    if (overrides.has_width) {
        config.lcd_panel.width = overrides.width;
    }

    if (overrides.has_height) {
        config.lcd_panel.height = overrides.height;
    }

    if (overrides.has_offset_x) {
        config.lcd_panel.offset_x = (int) overrides.offset_x;
    }

    if (overrides.has_offset_y) {
        config.lcd_panel.offset_y = (int) overrides.offset_y;
    }

    if (overrides.has_offset_rotation) {
        config.lcd_panel.offset_rotation = overrides.offset_rotation;
    }

    if (overrides.has_readable) {
        config.lcd_panel.readable = (overrides.readable != 0);
    }

    if (overrides.has_invert) {
        config.lcd_panel.invert = (overrides.invert != 0);
    }

    if (overrides.has_rgb_order) {
        config.lcd_panel.rgb_order = (overrides.rgb_order != 0);
    }

    if (overrides.has_dlen_16bit) {
        config.lcd_panel.dlen_16bit = (overrides.dlen_16bit != 0);
    }

    if (overrides.has_lcd_spi_mode) {
        config.lcd_bus.mode = overrides.lcd_spi_mode;
    }

    if (overrides.has_lcd_freq_write_hz) {
        config.lcd_bus.freq_write_hz = overrides.lcd_freq_write_hz;
    }

    if (overrides.has_lcd_freq_read_hz) {
        config.lcd_bus.freq_read_hz = overrides.lcd_freq_read_hz;
    }

    if (overrides.has_lcd_dma_channel) {
        config.lcd_bus.dma_channel = (int) overrides.lcd_dma_channel;
    }

    if (overrides.has_lcd_spi_3wire) {
        config.lcd_bus.spi_3wire = (overrides.lcd_spi_3wire != 0);
    }

    if (overrides.has_lcd_use_lock) {
        config.lcd_bus.use_lock = (overrides.lcd_use_lock != 0);
    }

    if (overrides.has_lcd_bus_shared) {
        config.lcd_panel.bus_shared = (overrides.lcd_bus_shared != 0);
    }

    if (overrides.has_spi_sclk_gpio) {
        config.lcd_bus.pin_sclk = (int) overrides.spi_sclk_gpio;
    }

    if (overrides.has_spi_mosi_gpio) {
        config.lcd_bus.pin_mosi = (int) overrides.spi_mosi_gpio;
    }

    if (overrides.has_spi_miso_gpio) {
        config.lcd_bus.pin_miso = (int) overrides.spi_miso_gpio;
    }

    if (overrides.has_lcd_spi_host) {
        config.lcd_bus.host = (int) overrides.lcd_spi_host;
    }

    if (overrides.has_lcd_cs_gpio) {
        config.lcd_panel.pin_cs = (int) overrides.lcd_cs_gpio;
    }

    if (overrides.has_lcd_dc_gpio) {
        config.lcd_bus.pin_dc = (int) overrides.lcd_dc_gpio;
    }

    if (overrides.has_lcd_rst_gpio) {
        config.lcd_panel.pin_rst = (int) overrides.lcd_rst_gpio;
    }

    if (overrides.has_lcd_pin_busy) {
        config.lcd_panel.pin_busy = (int) overrides.lcd_pin_busy;
    }

    if (overrides.has_touch_driver) {
        config.touch.driver_id = overrides.touch_driver;
        config.touch.driver_name = touch_driver_name(config.touch.driver_id);
    } else if (overrides.has_touch_i2c_port
        || overrides.has_touch_sda_gpio
        || overrides.has_touch_scl_gpio
        || overrides.has_touch_i2c_addr
        || overrides.has_touch_rst_gpio) {
        config.touch.driver_id = LGFX_TOUCH_DRIVER_ID_FT6336U;
        config.touch.driver_name = touch_driver_name(config.touch.driver_id);
    } else if (overrides.has_touch_cs_gpio
        || overrides.has_touch_spi_host
        || overrides.has_touch_spi_freq_hz) {
        config.touch.driver_id = LGFX_TOUCH_DRIVER_ID_XPT2046;
        config.touch.driver_name = touch_driver_name(config.touch.driver_id);
    }

    if (overrides.has_touch_cs_gpio) {
        config.touch.xpt2046.pin_cs = (int) overrides.touch_cs_gpio;
    }

    if (overrides.has_touch_irq_gpio) {
        config.touch.xpt2046.pin_irq = (int) overrides.touch_irq_gpio;
        config.touch.ft6336u.pin_irq = (int) overrides.touch_irq_gpio;
    }

    if (overrides.has_touch_spi_host) {
        config.touch.xpt2046.spi_host = (int) overrides.touch_spi_host;
    }

    if (overrides.has_touch_spi_freq_hz) {
        config.touch.xpt2046.spi_freq_hz = overrides.touch_spi_freq_hz;
    }

    if (overrides.has_touch_i2c_port) {
        config.touch.ft6336u.i2c_port = (int) overrides.touch_i2c_port;
    }

    if (overrides.has_touch_sda_gpio) {
        config.touch.ft6336u.pin_sda = (int) overrides.touch_sda_gpio;
    }

    if (overrides.has_touch_scl_gpio) {
        config.touch.ft6336u.pin_scl = (int) overrides.touch_scl_gpio;
    }

    if (overrides.has_touch_i2c_addr) {
        config.touch.ft6336u.i2c_addr = (uint8_t) overrides.touch_i2c_addr;
    }

    if (overrides.has_touch_rst_gpio) {
        config.touch.ft6336u.pin_rst = (int) overrides.touch_rst_gpio;
    }

    if (overrides.has_touch_offset_rotation) {
        config.touch.offset_rotation = overrides.touch_offset_rotation;
    }

    if (overrides.has_touch_bus_shared) {
        config.touch.bus_shared = (overrides.touch_bus_shared != 0);
    }

    config.touch.attached = touch_driver_is_effectively_attached(config.touch);
}

template <typename PanelT>
static void configure_selected_panel(
    PanelT &panel,
    lgfx::Bus_SPI &bus,
    const lgfx_dev::LgfxRuntimeConfig &runtime_config)
{
    panel.setBus(&bus);

    auto cfg = panel.config();

    cfg.pin_cs = runtime_config.lcd_panel.pin_cs;
    cfg.pin_rst = runtime_config.lcd_panel.pin_rst;
    cfg.pin_busy = runtime_config.lcd_panel.pin_busy;

    cfg.panel_width = runtime_config.lcd_panel.width;
    cfg.panel_height = runtime_config.lcd_panel.height;

    cfg.offset_x = runtime_config.lcd_panel.offset_x;
    cfg.offset_y = runtime_config.lcd_panel.offset_y;
    cfg.offset_rotation = runtime_config.lcd_panel.offset_rotation;

    cfg.dummy_read_pixel = runtime_config.lcd_panel.dummy_read_pixel;
    cfg.dummy_read_bits = runtime_config.lcd_panel.dummy_read_bits;

    cfg.readable = runtime_config.lcd_panel.readable;
    cfg.invert = runtime_config.lcd_panel.invert;
    cfg.rgb_order = runtime_config.lcd_panel.rgb_order;
    cfg.dlen_16bit = runtime_config.lcd_panel.dlen_16bit;

    cfg.bus_shared = runtime_config.lcd_panel.bus_shared;

    panel.config(cfg);
}

#if (LGFX_PORT_ENABLE_TOUCH == 1)
template <typename PanelT>
static void configure_xpt2046_if_needed(
    PanelT &panel,
    lgfx::Touch_XPT2046 &touch,
    const lgfx_dev::LgfxRuntimeConfig &runtime_config)
{
    if (runtime_config.touch.driver_id != LGFX_TOUCH_DRIVER_ID_XPT2046) {
        return;
    }

    if (!runtime_config.touch.attached) {
        ESP_LOGI(
            TAG,
            "touch compiled but unattached (driver=%s)",
            runtime_config.touch.driver_name);
        return;
    }

    auto cfg = touch.config();

    cfg.spi_host = static_cast<spi_host_device_t>(runtime_config.touch.xpt2046.spi_host);
    cfg.freq = runtime_config.touch.xpt2046.spi_freq_hz;

    cfg.pin_sclk = runtime_config.lcd_bus.pin_sclk;
    cfg.pin_mosi = runtime_config.lcd_bus.pin_mosi;
    cfg.pin_miso = runtime_config.lcd_bus.pin_miso;

    cfg.pin_cs = runtime_config.touch.xpt2046.pin_cs;
    cfg.pin_int = runtime_config.touch.xpt2046.pin_irq;

    cfg.bus_shared = runtime_config.touch.bus_shared;
    cfg.offset_rotation = runtime_config.touch.offset_rotation;

    touch.config(cfg);
    panel.setTouch(&touch);

    ESP_LOGI(
        TAG,
        "touch attached: driver=%s cs=%d irq=%d host=%d freq=%u offset_rotation=%u",
        runtime_config.touch.driver_name,
        runtime_config.touch.xpt2046.pin_cs,
        runtime_config.touch.xpt2046.pin_irq,
        runtime_config.touch.xpt2046.spi_host,
        (unsigned) runtime_config.touch.xpt2046.spi_freq_hz,
        (unsigned) runtime_config.touch.offset_rotation);
}

template <typename PanelT>
static void configure_ft5x06_if_needed(
    PanelT &panel,
    lgfx::Touch_FT5x06 &touch,
    const lgfx_dev::LgfxRuntimeConfig &runtime_config)
{
    if (runtime_config.touch.driver_id != LGFX_TOUCH_DRIVER_ID_FT6336U) {
        return;
    }

    if (!runtime_config.touch.attached) {
        ESP_LOGI(
            TAG,
            "touch compiled but unattached (driver=%s)",
            runtime_config.touch.driver_name);
        return;
    }

    auto cfg = touch.config();

    cfg.i2c_port = runtime_config.touch.ft6336u.i2c_port;
    cfg.pin_sda = runtime_config.touch.ft6336u.pin_sda;
    cfg.pin_scl = runtime_config.touch.ft6336u.pin_scl;
    cfg.i2c_addr = runtime_config.touch.ft6336u.i2c_addr;
    cfg.pin_int = runtime_config.touch.ft6336u.pin_irq;
    cfg.freq = 400000;
    cfg.bus_shared = runtime_config.touch.bus_shared;
    cfg.offset_rotation = runtime_config.touch.offset_rotation;
    cfg.x_min = 0;
    cfg.y_min = 0;
    cfg.x_max = runtime_config.lcd_panel.width - 1;
    cfg.y_max = runtime_config.lcd_panel.height - 1;

    touch.config(cfg);
    panel.setTouch(&touch);

    if (runtime_config.touch.ft6336u.pin_rst >= 0) {
        ESP_LOGI(
            TAG,
            "touch reset pin configured but not driven by LovyanGFX (driver=%s rst=%d)",
            runtime_config.touch.driver_name,
            runtime_config.touch.ft6336u.pin_rst);
    }

    ESP_LOGI(
        TAG,
        "touch attached: driver=%s i2c_port=%d sda=%d scl=%d addr=%u irq=%d offset_rotation=%u",
        runtime_config.touch.driver_name,
        runtime_config.touch.ft6336u.i2c_port,
        runtime_config.touch.ft6336u.pin_sda,
        runtime_config.touch.ft6336u.pin_scl,
        (unsigned) runtime_config.touch.ft6336u.i2c_addr,
        runtime_config.touch.ft6336u.pin_irq,
        (unsigned) runtime_config.touch.offset_rotation);
}

template <typename PanelT>
static void configure_touch_if_needed(
    PanelT &panel,
    lgfx::Touch_XPT2046 &touch_xpt2046,
    lgfx::Touch_FT5x06 &touch_ft5x06,
    const lgfx_dev::LgfxRuntimeConfig &runtime_config)
{
    if (!runtime_config.touch.compiled) {
        return;
    }

    switch (runtime_config.touch.driver_id) {
        case LGFX_TOUCH_DRIVER_ID_XPT2046:
            configure_xpt2046_if_needed(panel, touch_xpt2046, runtime_config);
            break;

        case LGFX_TOUCH_DRIVER_ID_FT6336U:
            configure_ft5x06_if_needed(panel, touch_ft5x06, runtime_config);
            break;

        default:
            ESP_LOGI(TAG, "touch compiled but has unknown driver id=%d", (int) runtime_config.touch.driver_id);
            break;
    }
}
#endif

class PiyopiyoLGFX : public lgfx::LGFX_Device
{
    lgfx_dev::LgfxRuntimeConfig runtime_config_;
    lgfx::Bus_SPI bus_;
    lgfx::Panel_Device *selected_panel_ = nullptr;

    lgfx::Panel_ILI9488 panel_ili9488_;
    lgfx::Panel_ILI9341 panel_ili9341_;
    lgfx::Panel_ILI9341_2 panel_ili9341_2_;
    lgfx::Panel_ST7789 panel_st7789_;
    lgfx::Panel_ILI9342 panel_ili9342_;

#if (LGFX_PORT_ENABLE_TOUCH == 1)
    lgfx::Touch_XPT2046 touch_xpt2046_;
    lgfx::Touch_FT5x06 touch_ft5x06_;
#endif

public:
    explicit PiyopiyoLGFX(const lgfx_dev::LgfxRuntimeConfig &runtime_config)
        : runtime_config_(runtime_config)
    {
        ESP_LOGI(
            TAG,
            "panel driver=%s size=%ux%u",
            runtime_config_.lcd_panel.driver_name,
            (unsigned) runtime_config_.lcd_panel.width,
            (unsigned) runtime_config_.lcd_panel.height);

        {
            auto cfg = bus_.config();

            cfg.spi_host = static_cast<spi_host_device_t>(runtime_config_.lcd_bus.host);
            cfg.spi_mode = runtime_config_.lcd_bus.mode;
            cfg.freq_write = runtime_config_.lcd_bus.freq_write_hz;
            cfg.freq_read = runtime_config_.lcd_bus.freq_read_hz;
            cfg.spi_3wire = runtime_config_.lcd_bus.spi_3wire;
            cfg.use_lock = runtime_config_.lcd_bus.use_lock;
            cfg.dma_channel = runtime_config_.lcd_bus.dma_channel;

            cfg.pin_sclk = runtime_config_.lcd_bus.pin_sclk;
            cfg.pin_mosi = runtime_config_.lcd_bus.pin_mosi;
            cfg.pin_miso = runtime_config_.lcd_bus.pin_miso;
            cfg.pin_dc = runtime_config_.lcd_bus.pin_dc;

            bus_.config(cfg);
        }

        switch (runtime_config_.lcd_panel.driver_id) {
            case LGFX_PANEL_DRIVER_ID_ILI9488:
                configure_selected_panel(panel_ili9488_, bus_, runtime_config_);
#if (LGFX_PORT_ENABLE_TOUCH == 1)
                configure_touch_if_needed(panel_ili9488_, touch_xpt2046_, touch_ft5x06_, runtime_config_);
#endif
                selected_panel_ = &panel_ili9488_;
                break;

            case LGFX_PANEL_DRIVER_ID_ILI9341:
                configure_selected_panel(panel_ili9341_, bus_, runtime_config_);
#if (LGFX_PORT_ENABLE_TOUCH == 1)
                configure_touch_if_needed(panel_ili9341_, touch_xpt2046_, touch_ft5x06_, runtime_config_);
#endif
                selected_panel_ = &panel_ili9341_;
                break;

            case LGFX_PANEL_DRIVER_ID_ILI9341_2:
                configure_selected_panel(panel_ili9341_2_, bus_, runtime_config_);
#if (LGFX_PORT_ENABLE_TOUCH == 1)
                configure_touch_if_needed(panel_ili9341_2_, touch_xpt2046_, touch_ft5x06_, runtime_config_);
#endif
                selected_panel_ = &panel_ili9341_2_;
                break;

            case LGFX_PANEL_DRIVER_ID_ST7789:
                configure_selected_panel(panel_st7789_, bus_, runtime_config_);
#if (LGFX_PORT_ENABLE_TOUCH == 1)
                configure_touch_if_needed(panel_st7789_, touch_xpt2046_, touch_ft5x06_, runtime_config_);
#endif
                selected_panel_ = &panel_st7789_;
                break;

            case LGFX_PANEL_DRIVER_ID_ILI9342C:
                configure_selected_panel(panel_ili9342_, bus_, runtime_config_);
#if (LGFX_PORT_ENABLE_TOUCH == 1)
                configure_touch_if_needed(panel_ili9342_, touch_xpt2046_, touch_ft5x06_, runtime_config_);
#endif
                selected_panel_ = &panel_ili9342_;
                break;

            default:
                ESP_LOGE(TAG, "unsupported runtime panel_driver=%d", (int) runtime_config_.lcd_panel.driver_id);
                break;
        }

        if (selected_panel_ != nullptr) {
            setPanel(selected_panel_);
        }
    }

    bool is_configured() const
    {
        return selected_panel_ != nullptr;
    }
};

} // namespace

namespace lgfx_dev
{

bool validate_runtime_config(const LgfxRuntimeConfig &config, const char **reason)
{
    const char *local_reason = nullptr;

    if (!is_known_panel_driver_id(config.lcd_panel.driver_id)) {
        local_reason = "panel_driver must be ili9488, ili9341, ili9341_2, st7789, or ili9342c";
    } else if (!is_known_touch_driver_id(config.touch.driver_id)) {
        local_reason = "touch_driver must be xpt2046 or ft6336u";
    } else if (config.lcd_panel.width == 0) {
        local_reason = "width must be > 0";
    } else if (config.lcd_panel.height == 0) {
        local_reason = "height must be > 0";
    } else if (config.lcd_panel.offset_rotation > 7) {
        local_reason = "offset_rotation must be in 0..7";
    } else if (config.lcd_bus.mode > 3) {
        local_reason = "lcd_spi_mode must be in 0..3";
    } else if (config.touch.offset_rotation > 7) {
        local_reason = "touch_offset_rotation must be in 0..7";
    } else if (config.touch.driver_id == LGFX_TOUCH_DRIVER_ID_XPT2046 && config.touch.xpt2046.spi_freq_hz == 0) {
        local_reason = "touch_spi_freq_hz must be > 0";
    } else if (config.touch.driver_id == LGFX_TOUCH_DRIVER_ID_FT6336U && (config.touch.ft6336u.i2c_port < 0 || config.touch.ft6336u.i2c_port > 1)) {
        local_reason = "touch_i2c_port must be 0 or 1";
    } else if (config.touch.driver_id == LGFX_TOUCH_DRIVER_ID_FT6336U && config.touch.ft6336u.i2c_addr > 127) {
        local_reason = "touch_i2c_addr must be in 0..127";
    }

    if (reason != nullptr) {
        *reason = local_reason;
    }

    return local_reason == nullptr;
}

LgfxRuntimeConfig runtime_config_with_overrides(const lgfx_open_config_overrides_t *overrides)
{
    LgfxRuntimeConfig config = runtime_config_from_build_defaults();

    if (overrides != nullptr) {
        apply_open_config_overrides(config, *overrides);
    }

    return config;
}

void log_runtime_config(const LgfxRuntimeConfig &config)
{
    ESP_LOGI(
        TAG,
        "effective config panel=%s size=%ux%u offset=(%d,%d) rot=%u readable=%u invert=%u rgb_order=%u dlen_16bit=%u bus_shared=%u",
        config.lcd_panel.driver_name,
        (unsigned) config.lcd_panel.width,
        (unsigned) config.lcd_panel.height,
        config.lcd_panel.offset_x,
        config.lcd_panel.offset_y,
        (unsigned) config.lcd_panel.offset_rotation,
        (unsigned) config.lcd_panel.readable,
        (unsigned) config.lcd_panel.invert,
        (unsigned) config.lcd_panel.rgb_order,
        (unsigned) config.lcd_panel.dlen_16bit,
        (unsigned) config.lcd_panel.bus_shared);

    ESP_LOGI(
        TAG,
        "effective bus host=%d mode=%u write_hz=%u read_hz=%u dma=%d sclk=%d mosi=%d miso=%d dc=%d spi_3wire=%u use_lock=%u",
        config.lcd_bus.host,
        (unsigned) config.lcd_bus.mode,
        (unsigned) config.lcd_bus.freq_write_hz,
        (unsigned) config.lcd_bus.freq_read_hz,
        config.lcd_bus.dma_channel,
        config.lcd_bus.pin_sclk,
        config.lcd_bus.pin_mosi,
        config.lcd_bus.pin_miso,
        config.lcd_bus.pin_dc,
        (unsigned) config.lcd_bus.spi_3wire,
        (unsigned) config.lcd_bus.use_lock);

    if (config.touch.compiled) {
        switch (config.touch.driver_id) {
            case LGFX_TOUCH_DRIVER_ID_XPT2046:
                ESP_LOGI(
                    TAG,
                    "effective touch compiled=1 driver=%s attached=%u cs=%d irq=%d host=%d freq=%u offset_rotation=%u bus_shared=%u",
                    config.touch.driver_name,
                    (unsigned) config.touch.attached,
                    config.touch.xpt2046.pin_cs,
                    config.touch.xpt2046.pin_irq,
                    config.touch.xpt2046.spi_host,
                    (unsigned) config.touch.xpt2046.spi_freq_hz,
                    (unsigned) config.touch.offset_rotation,
                    (unsigned) config.touch.bus_shared);
                break;

            case LGFX_TOUCH_DRIVER_ID_FT6336U:
                ESP_LOGI(
                    TAG,
                    "effective touch compiled=1 driver=%s attached=%u i2c_port=%d sda=%d scl=%d addr=%u irq=%d rst=%d offset_rotation=%u bus_shared=%u",
                    config.touch.driver_name,
                    (unsigned) config.touch.attached,
                    config.touch.ft6336u.i2c_port,
                    config.touch.ft6336u.pin_sda,
                    config.touch.ft6336u.pin_scl,
                    (unsigned) config.touch.ft6336u.i2c_addr,
                    config.touch.ft6336u.pin_irq,
                    config.touch.ft6336u.pin_rst,
                    (unsigned) config.touch.offset_rotation,
                    (unsigned) config.touch.bus_shared);
                break;

            default:
                ESP_LOGI(TAG, "effective touch compiled=1 driver=unknown attached=%u", (unsigned) config.touch.attached);
                break;
        }
    } else {
        ESP_LOGI(TAG, "effective touch compiled=0");
    }
}

lgfx::LGFX_Device *create_lcd_device(const LgfxRuntimeConfig &config)
{
    PiyopiyoLGFX *device = new (std::nothrow) PiyopiyoLGFX(config);
    if (device == nullptr) {
        return nullptr;
    }

    if (!device->is_configured()) {
        delete device;
        return nullptr;
    }

    return device;
}

void destroy_lcd_device(lgfx::LGFX_Device *device)
{
    delete static_cast<PiyopiyoLGFX *>(device);
}

} // namespace lgfx_dev
