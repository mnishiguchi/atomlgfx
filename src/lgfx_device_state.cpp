// src/lgfx_device_state.cpp
#include <new>
#include <stddef.h>
#include <stdint.h>

#include <LovyanGFX.hpp>

// Build-time configuration generated
#include "lgfx_port/lgfx_port_config.h"

#if (LGFX_PORT_ENABLE_TOUCH == 1)
#include <lgfx/v1/touch/Touch_XPT2046.hpp>
#endif

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

// ----------------------------------------------------------------------------
// Semantic config validation
// ----------------------------------------------------------------------------
//
// Missing generated macros already fail loudly at compile use sites.
// Keep only checks that prevent silent misconfiguration or narrowing surprises.
//

#ifndef LGFX_PORT_PANEL_DRIVER_ILI9341_2
#error "LGFX_PORT_PANEL_DRIVER_ILI9341_2 must be defined by lgfx_port_config.h"
#endif

#ifndef LGFX_PORT_PANEL_DRIVER_ST7789
#error "LGFX_PORT_PANEL_DRIVER_ST7789 must be defined by lgfx_port_config.h"
#endif

#define LGFX_PORT_ASSERT_BOOL01(name) \
    static_assert(((name) == 0) || ((name) == 1), #name " must be 0 or 1")

LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_ENABLE_TOUCH);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_PANEL_DRIVER_ILI9488);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_PANEL_DRIVER_ILI9341);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_PANEL_DRIVER_ILI9341_2);
LGFX_PORT_ASSERT_BOOL01(LGFX_PORT_PANEL_DRIVER_ST7789);
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
        + LGFX_PORT_PANEL_DRIVER_ST7789)
        == 1,
    "Exactly one panel driver must be selected");

static_assert((LGFX_PORT_MAX_SPRITES) <= 254u, "LGFX_PORT_MAX_SPRITES must be <= 254");

static_assert((LGFX_PORT_PANEL_WIDTH) > 0, "LGFX_PORT_PANEL_WIDTH must be > 0");
static_assert((LGFX_PORT_PANEL_HEIGHT) > 0, "LGFX_PORT_PANEL_HEIGHT must be > 0");
static_assert((LGFX_PORT_PANEL_WIDTH) <= 65535u, "LGFX_PORT_PANEL_WIDTH must fit in uint16_t");
static_assert((LGFX_PORT_PANEL_HEIGHT) <= 65535u, "LGFX_PORT_PANEL_HEIGHT must fit in uint16_t");

static_assert((LGFX_PORT_LCD_SPI_MODE) >= 0 && (LGFX_PORT_LCD_SPI_MODE) <= 3, "LGFX_PORT_LCD_SPI_MODE must be 0..3");
static_assert((LGFX_PORT_LCD_OFFSET_ROTATION) <= 7u, "LGFX_PORT_LCD_OFFSET_ROTATION must be 0..7");

#if (LGFX_PORT_ENABLE_TOUCH == 1)
static_assert((LGFX_PORT_TOUCH_OFFSET_ROTATION) <= 7u, "LGFX_PORT_TOUCH_OFFSET_ROTATION must be 0..7");
#endif

namespace
{

// ----------------------------------------------------------------------------
// Setup / state
// ----------------------------------------------------------------------------

static constexpr const char *TAG = "lgfx_device";

// Protects publication of the process-global singleton pointer, owner token,
// and ready flag. Keep critical sections short and allocation-free.
static portMUX_TYPE g_publication_mux = portMUX_INITIALIZER_UNLOCKED;

// Singleton device instance.
class PiyopiyoLGFX;
static PiyopiyoLGFX *g_lcd_device = nullptr;
static SemaphoreHandle_t g_lcd_mutex = nullptr;

// Live singleton owner token (currently the owning lgfx_port_t *).
// Null means no port currently owns a published singleton device.
static const void *g_device_owner_token = nullptr;

// True only after begin() completed successfully for the currently published
// singleton device.
static bool g_device_ready = false;

// ----------------------------------------------------------------------------
// Build-default panel driver selection
// ----------------------------------------------------------------------------

#if (LGFX_PORT_PANEL_DRIVER_ILI9488 == 1)
static constexpr lgfx_panel_driver_id_t BUILD_PANEL_DRIVER_ID = LGFX_PANEL_DRIVER_ID_ILI9488;
#elif (LGFX_PORT_PANEL_DRIVER_ILI9341 == 1)
static constexpr lgfx_panel_driver_id_t BUILD_PANEL_DRIVER_ID = LGFX_PANEL_DRIVER_ID_ILI9341;
#elif (LGFX_PORT_PANEL_DRIVER_ILI9341_2 == 1)
static constexpr lgfx_panel_driver_id_t BUILD_PANEL_DRIVER_ID = LGFX_PANEL_DRIVER_ID_ILI9341_2;
#elif (LGFX_PORT_PANEL_DRIVER_ST7789 == 1)
static constexpr lgfx_panel_driver_id_t BUILD_PANEL_DRIVER_ID = LGFX_PANEL_DRIVER_ID_ST7789;
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
            return true;
        default:
            return false;
    }
}

// ----------------------------------------------------------------------------
// Compile-time constants still shared across split files
// ----------------------------------------------------------------------------

static constexpr uint16_t PANEL_W = (uint16_t) (LGFX_PORT_PANEL_WIDTH);
static constexpr uint16_t PANEL_H = (uint16_t) (LGFX_PORT_PANEL_HEIGHT);

static constexpr uint16_t MAX_SPRITES = static_cast<uint16_t>(LGFX_PORT_MAX_SPRITES);
static constexpr uint8_t MAX_HANDLE = 254;

// ----------------------------------------------------------------------------
// Publication / ownership snapshots
// ----------------------------------------------------------------------------

struct DevicePublicationSnapshot
{
    PiyopiyoLGFX *lcd;
    const void *owner_token;
    bool ready;
};

static DevicePublicationSnapshot snapshot_device_publication()
{
    DevicePublicationSnapshot snapshot = {};

    portENTER_CRITICAL(&g_publication_mux);
    snapshot.lcd = g_lcd_device;
    snapshot.owner_token = g_device_owner_token;
    snapshot.ready = g_device_ready;
    portEXIT_CRITICAL(&g_publication_mux);

    return snapshot;
}

static inline bool snapshot_has_published_device(const DevicePublicationSnapshot &snapshot)
{
    return snapshot.lcd != nullptr;
}

static inline bool snapshot_is_owned_by(const DevicePublicationSnapshot &snapshot, const void *owner_token)
{
    return owner_token != nullptr && snapshot.owner_token == owner_token;
}

static inline bool snapshot_is_owned_live_device(const DevicePublicationSnapshot &snapshot, const void *owner_token)
{
    return snapshot_has_published_device(snapshot) && snapshot_is_owned_by(snapshot, owner_token);
}

static inline bool snapshot_is_fully_unpublished(const DevicePublicationSnapshot &snapshot)
{
    return snapshot.lcd == nullptr && snapshot.owner_token == nullptr && !snapshot.ready;
}

// ----------------------------------------------------------------------------
// Native runtime config
// ----------------------------------------------------------------------------

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

static void apply_panel_driver_baseline(
    LgfxRuntimeConfig &config,
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

        default:
            break;
    }
}

static LgfxRuntimeConfig runtime_config_from_build_defaults()
{
    LgfxRuntimeConfig config = {};

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
    config.touch.pin_cs = (int) (LGFX_PORT_TOUCH_CS_GPIO);
    config.touch.pin_irq = (int) (LGFX_PORT_TOUCH_IRQ_GPIO);
    config.touch.spi_host = (int) (LGFX_PORT_TOUCH_SPI_HOST);
    config.touch.spi_freq_hz = (uint32_t) (LGFX_PORT_TOUCH_SPI_FREQ_HZ);
    config.touch.offset_rotation = (uint8_t) (LGFX_PORT_TOUCH_OFFSET_ROTATION);
    config.touch.bus_shared = ((LGFX_PORT_TOUCH_BUS_SHARED) != 0);
    config.touch.attached = config.touch.compiled && (config.touch.pin_cs >= 0);

    return config;
}

static void apply_open_config_overrides(
    LgfxRuntimeConfig &config,
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

    if (overrides.has_touch_cs_gpio) {
        config.touch.pin_cs = (int) overrides.touch_cs_gpio;
    }

    if (overrides.has_touch_irq_gpio) {
        config.touch.pin_irq = (int) overrides.touch_irq_gpio;
    }

    if (overrides.has_touch_spi_host) {
        config.touch.spi_host = (int) overrides.touch_spi_host;
    }

    if (overrides.has_touch_spi_freq_hz) {
        config.touch.spi_freq_hz = overrides.touch_spi_freq_hz;
    }

    if (overrides.has_touch_offset_rotation) {
        config.touch.offset_rotation = overrides.touch_offset_rotation;
    }

    if (overrides.has_touch_bus_shared) {
        config.touch.bus_shared = (overrides.touch_bus_shared != 0);
    }

    config.touch.attached = config.touch.compiled && (config.touch.pin_cs >= 0);
}

static bool validate_runtime_config(const LgfxRuntimeConfig &config, const char **reason)
{
    if (!is_known_panel_driver_id(config.lcd_panel.driver_id)) {
        *reason = "panel_driver must be ili9488, ili9341, ili9341_2, or st7789";
        return false;
    }

    if (config.lcd_panel.width == 0) {
        *reason = "width must be > 0";
        return false;
    }

    if (config.lcd_panel.height == 0) {
        *reason = "height must be > 0";
        return false;
    }

    if (config.lcd_panel.offset_rotation > 7) {
        *reason = "offset_rotation must be in 0..7";
        return false;
    }

    if (config.lcd_bus.mode > 3) {
        *reason = "lcd_spi_mode must be in 0..3";
        return false;
    }

    if (config.touch.offset_rotation > 7) {
        *reason = "touch_offset_rotation must be in 0..7";
        return false;
    }

    return true;
}

static LgfxRuntimeConfig runtime_config_with_overrides(const lgfx_open_config_overrides_t *overrides)
{
    LgfxRuntimeConfig config = runtime_config_from_build_defaults();

    if (overrides != nullptr) {
        apply_open_config_overrides(config, *overrides);
    }

    return config;
}

static void log_runtime_config(const LgfxRuntimeConfig &config)
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
        ESP_LOGI(
            TAG,
            "effective touch compiled=1 attached=%u cs=%d irq=%d host=%d freq=%u offset_rotation=%u bus_shared=%u",
            (unsigned) config.touch.attached,
            config.touch.pin_cs,
            config.touch.pin_irq,
            config.touch.spi_host,
            (unsigned) config.touch.spi_freq_hz,
            (unsigned) config.touch.offset_rotation,
            (unsigned) config.touch.bus_shared);
    } else {
        ESP_LOGI(TAG, "effective touch compiled=0");
    }
}

template <typename PanelT>
static void configure_selected_panel(
    PanelT &panel,
    lgfx::Bus_SPI &bus,
    const LgfxRuntimeConfig &runtime_config)
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
static void configure_touch_if_needed(
    PanelT &panel,
    lgfx::Touch_XPT2046 &touch,
    const LgfxRuntimeConfig &runtime_config)
{
    if (runtime_config.touch.attached) {
        auto cfg = touch.config();

        cfg.spi_host = static_cast<spi_host_device_t>(runtime_config.touch.spi_host);
        cfg.freq = runtime_config.touch.spi_freq_hz;

        cfg.pin_sclk = runtime_config.lcd_bus.pin_sclk;
        cfg.pin_mosi = runtime_config.lcd_bus.pin_mosi;
        cfg.pin_miso = runtime_config.lcd_bus.pin_miso;

        cfg.pin_cs = runtime_config.touch.pin_cs;
        cfg.pin_int = runtime_config.touch.pin_irq;

        cfg.bus_shared = runtime_config.touch.bus_shared;
        cfg.offset_rotation = runtime_config.touch.offset_rotation;

        touch.config(cfg);
        panel.setTouch(&touch);

        ESP_LOGI(
            TAG,
            "touch attached: cs=%d irq=%d host=%d freq=%u offset_rotation=%u",
            runtime_config.touch.pin_cs,
            runtime_config.touch.pin_irq,
            runtime_config.touch.spi_host,
            (unsigned) runtime_config.touch.spi_freq_hz,
            (unsigned) runtime_config.touch.offset_rotation);
    } else if (runtime_config.touch.compiled) {
        ESP_LOGI(TAG, "touch compiled but unattached (effective touch_cs_gpio=-1)");
    }
}
#endif

class PiyopiyoLGFX : public lgfx::LGFX_Device
{
    LgfxRuntimeConfig runtime_config_;
    lgfx::Bus_SPI bus_;
    lgfx::Panel_Device *selected_panel_ = nullptr;

    lgfx::Panel_ILI9488 panel_ili9488_;
    lgfx::Panel_ILI9341 panel_ili9341_;
    lgfx::Panel_ILI9341_2 panel_ili9341_2_;
    lgfx::Panel_ST7789 panel_st7789_;

#if (LGFX_PORT_ENABLE_TOUCH == 1)
    lgfx::Touch_XPT2046 touch_;
#endif

public:
    explicit PiyopiyoLGFX(const LgfxRuntimeConfig &runtime_config)
        : runtime_config_(runtime_config)
    {
        ESP_LOGI(
            TAG,
            "panel driver=%s size=%ux%u",
            runtime_config_.lcd_panel.driver_name,
            (unsigned) runtime_config_.lcd_panel.width,
            (unsigned) runtime_config_.lcd_panel.height);

        // SPI bus config
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
                configure_touch_if_needed(panel_ili9488_, touch_, runtime_config_);
#endif
                selected_panel_ = &panel_ili9488_;
                break;

            case LGFX_PANEL_DRIVER_ID_ILI9341:
                configure_selected_panel(panel_ili9341_, bus_, runtime_config_);
#if (LGFX_PORT_ENABLE_TOUCH == 1)
                configure_touch_if_needed(panel_ili9341_, touch_, runtime_config_);
#endif
                selected_panel_ = &panel_ili9341_;
                break;

            case LGFX_PANEL_DRIVER_ID_ILI9341_2:
                configure_selected_panel(panel_ili9341_2_, bus_, runtime_config_);
#if (LGFX_PORT_ENABLE_TOUCH == 1)
                configure_touch_if_needed(panel_ili9341_2_, touch_, runtime_config_);
#endif
                selected_panel_ = &panel_ili9341_2_;
                break;

            case LGFX_PANEL_DRIVER_ID_ST7789:
                configure_selected_panel(panel_st7789_, bus_, runtime_config_);
#if (LGFX_PORT_ENABLE_TOUCH == 1)
                configure_touch_if_needed(panel_st7789_, touch_, runtime_config_);
#endif
                selected_panel_ = &panel_st7789_;
                break;

            default:
                ESP_LOGE(TAG, "unsupported runtime panel_driver=%d", (int) runtime_config_.lcd_panel.driver_id);
                break;
        }

        if (selected_panel_ != nullptr) {
            setPanel(selected_panel_);
        }
    }
};

static constexpr size_t SPRITE_SLOTS = (size_t) MAX_HANDLE + 1u; // handle 0 reserved for LCD
static lgfx::LGFX_Sprite *sprites[SPRITE_SLOTS] = { 0 };
static uint16_t sprite_count = 0;

static inline void ensure_lcd_mutex_created()
{
    if (g_lcd_mutex) {
        return;
    }

    // Create outside the critical section (xSemaphoreCreateMutex may allocate).
    SemaphoreHandle_t created = xSemaphoreCreateMutex();
    if (!created) {
        ESP_LOGE(TAG, "failed to create mutex");
        return;
    }

    portENTER_CRITICAL(&g_publication_mux);
    if (!g_lcd_mutex) {
        g_lcd_mutex = created;
        created = nullptr;
    }
    portEXIT_CRITICAL(&g_publication_mux);

#if defined(INCLUDE_vSemaphoreDelete) && (INCLUDE_vSemaphoreDelete == 1)
    // If we lost the race, delete the extra mutex.
    if (created) {
        vSemaphoreDelete(created);
    }
#else
    (void) created; // best-effort: leak only on rare init race if delete not available
#endif
}

static bool acquire_lcd_mutex()
{
    ensure_lcd_mutex_created();
    if (!g_lcd_mutex) {
        return false;
    }

    return xSemaphoreTake(g_lcd_mutex, portMAX_DELAY) == pdTRUE;
}

static void release_lcd_mutex()
{
    if (g_lcd_mutex) {
        xSemaphoreGive(g_lcd_mutex);
    }
}

static inline lgfx::LGFXBase *resolve_target(uint8_t target)
{
    if (target == 0) {
        return g_lcd_device;
    }

    if (target > MAX_HANDLE) {
        return nullptr;
    }

    return sprites[target];
}

static esp_err_t ensure_published_device_for_owner(
    const lgfx_open_config_overrides_t *overrides,
    const void *owner_token)
{
    if (overrides == nullptr || owner_token == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    ensure_lcd_mutex_created();
    if (!g_lcd_mutex) {
        return ESP_ERR_NO_MEM;
    }

    const DevicePublicationSnapshot snapshot = snapshot_device_publication();

    if (snapshot.lcd != nullptr) {
        return snapshot.owner_token == owner_token ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    if (snapshot.owner_token != nullptr && snapshot.owner_token != owner_token) {
        return ESP_ERR_INVALID_STATE;
    }

    LgfxRuntimeConfig config = runtime_config_with_overrides(overrides);

    const char *validation_error = nullptr;
    if (!validate_runtime_config(config, &validation_error)) {
        ESP_LOGE(TAG, "invalid open-time runtime config: %s", validation_error);
        return ESP_ERR_INVALID_ARG;
    }

    log_runtime_config(config);

    // Allocate outside the critical section (new may allocate).
    PiyopiyoLGFX *created = new (std::nothrow) PiyopiyoLGFX(config);
    if (!created) {
        ESP_LOGE(TAG, "failed to allocate LGFX device");
        return ESP_ERR_NO_MEM;
    }

    esp_err_t publish_err = ESP_OK;

    portENTER_CRITICAL(&g_publication_mux);
    if (g_lcd_device == nullptr && (g_device_owner_token == nullptr || g_device_owner_token == owner_token)) {
        g_lcd_device = created;
        g_device_owner_token = owner_token;
        created = nullptr;
    } else if (g_device_owner_token != owner_token) {
        publish_err = ESP_ERR_INVALID_STATE;
    }
    portEXIT_CRITICAL(&g_publication_mux);

    // If we lost the race, destroy the extra instance.
    if (created) {
        delete created;
    }

    return publish_err;
}

} // namespace

// ----------------------------------------------------------------------------
// Internal shared API for split files (definitions)
// ----------------------------------------------------------------------------

namespace lgfx_dev
{

uint16_t panel_width_const()
{
    return PANEL_W;
}

uint16_t panel_height_const()
{
    return PANEL_H;
}

uint8_t max_handle_const()
{
    return MAX_HANDLE;
}

uint16_t max_sprites_const()
{
    return MAX_SPRITES;
}

esp_err_t ensure_published()
{
    return (g_lcd_device != nullptr) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

bool lock_lcd()
{
    return ::acquire_lcd_mutex();
}

void unlock_lcd()
{
    ::release_lcd_mutex();
}

void ScopedLcdLock::lock()
{
    locked_ = lgfx_dev::lock_lcd();
}

bool ScopedLcdLock::is_locked() const
{
    return locked_;
}

ScopedLcdLock::~ScopedLcdLock()
{
    if (locked_) {
        lgfx_dev::unlock_lcd();
    }
}

esp_err_t lock_ready(ScopedLcdLock &lock)
{
    esp_err_t err = lgfx_dev::ensure_published();
    if (err != ESP_OK) {
        return err;
    }

    lock.lock();
    if (!lock.is_locked()) {
        return ESP_ERR_NO_MEM;
    }

    if (!g_device_ready || !g_lcd_device) {
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

lgfx::LGFX_Device *lcd_device_locked()
{
    return g_lcd_device;
}

bool is_initialized_locked()
{
    return g_device_ready;
}

lgfx::LGFXBase *resolve_target_locked(uint8_t target)
{
    return ::resolve_target(target);
}

lgfx::LGFX_Sprite *resolve_sprite_locked(uint8_t handle)
{
    if (handle == 0 || handle > MAX_HANDLE) {
        return nullptr;
    }

    return sprites[handle];
}

void set_sprite_locked(uint8_t handle, lgfx::LGFX_Sprite *spr)
{
    if (handle == 0 || handle > MAX_HANDLE) {
        return;
    }

    sprites[handle] = spr;
}

void clear_sprite_locked(uint8_t handle)
{
    if (handle == 0 || handle > MAX_HANDLE) {
        return;
    }

    sprites[handle] = nullptr;
}

void increment_sprite_count_locked()
{
    if (sprite_count < MAX_SPRITES) {
        sprite_count++;
    }
}

void decrement_sprite_count_locked()
{
    if (sprite_count > 0) {
        sprite_count--;
    }
}

uint32_t sprite_count_locked()
{
    return static_cast<uint32_t>(sprite_count);
}

void destroy_all_sprites_locked()
{
    for (uint16_t handle = 1; handle <= MAX_HANDLE; handle++) {
        lgfx::LGFX_Sprite *spr = sprites[handle];
        if (!spr) {
            continue;
        }

        // Explicitly release sprite buffers before deleting the object.
        spr->deleteSprite();
        delete spr;

        sprites[handle] = nullptr;
    }

    sprite_count = 0;
}

} // namespace lgfx_dev

// ----------------------------------------------------------------------------
// API: setup / lifecycle
// ----------------------------------------------------------------------------

extern "C" esp_err_t lgfx_device_get_dims_for_open_config(
    const lgfx_open_config_overrides_t *overrides,
    const void *owner_token,
    uint16_t *out_w,
    uint16_t *out_h)
{
    if (out_w == nullptr || out_h == nullptr || owner_token == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const DevicePublicationSnapshot snapshot = snapshot_device_publication();

    if (snapshot_is_owned_live_device(snapshot, owner_token)) {
        if (!acquire_lcd_mutex()) {
            return ESP_ERR_NO_MEM;
        }

        const DevicePublicationSnapshot locked_snapshot = snapshot_device_publication();

        if (snapshot_is_owned_live_device(locked_snapshot, owner_token)) {
            *out_w = static_cast<uint16_t>(locked_snapshot.lcd->width());
            *out_h = static_cast<uint16_t>(locked_snapshot.lcd->height());
            release_lcd_mutex();
            return ESP_OK;
        }

        release_lcd_mutex();
    }

    LgfxRuntimeConfig config = runtime_config_with_overrides(overrides);

    const char *validation_error = nullptr;
    if (!validate_runtime_config(config, &validation_error)) {
        ESP_LOGE(TAG, "invalid open-time runtime config for get_dims: %s", validation_error);
        return ESP_ERR_INVALID_ARG;
    }

    *out_w = config.lcd_panel.width;
    *out_h = config.lcd_panel.height;
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_init_with_open_config(
    const lgfx_open_config_overrides_t *overrides,
    const void *owner_token)
{
    esp_err_t err = ensure_published_device_for_owner(overrides, owner_token);
    if (err != ESP_OK) {
        return err;
    }

    if (!acquire_lcd_mutex()) {
        return ESP_ERR_NO_MEM;
    }

    const DevicePublicationSnapshot snapshot = snapshot_device_publication();

    // Defensive: close() may have raced before we acquired the mutex.
    if (!snapshot_is_owned_live_device(snapshot, owner_token)) {
        release_lcd_mutex();
        return ESP_ERR_INVALID_STATE;
    }

    if (!g_device_ready) {
        LgfxRuntimeConfig config = runtime_config_with_overrides(overrides);

#if (LGFX_PORT_ENABLE_TOUCH == 1)
        if (config.touch.compiled && !config.touch.attached) {
            ESP_LOGW(TAG, "touch enabled but not attached (effective touch_cs_gpio=-1)");
        }
#endif

        ESP_LOGI(TAG, "init/begin");
        snapshot.lcd->begin();
        g_device_ready = true;

        // Default text state after begin().
        snapshot.lcd->setTextSize(1);
        snapshot.lcd->setTextDatum(textdatum_t::top_left);
    }

    release_lcd_mutex();
    return ESP_OK;
}

static esp_err_t lgfx_device_deinit_for_owner(const void *owner_token)
{
    if (owner_token == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const DevicePublicationSnapshot snapshot = snapshot_device_publication();

    if (!snapshot_is_owned_by(snapshot, owner_token)) {
        return snapshot_is_fully_unpublished(snapshot) ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    // Idempotent teardown for the owning port.
    // Allow close even if begin() never happened, as long as this owner owns the singleton.
    ensure_lcd_mutex_created();
    if (!g_lcd_mutex) {
        return ESP_ERR_NO_MEM;
    }

    if (!acquire_lcd_mutex()) {
        return ESP_ERR_NO_MEM;
    }

    const DevicePublicationSnapshot locked_snapshot = snapshot_device_publication();
    if (!snapshot_is_owned_live_device(locked_snapshot, owner_token)) {
        release_lcd_mutex();
        return snapshot_is_fully_unpublished(locked_snapshot) ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    // Keep the held mutex handle so we can delete it at the end.
    SemaphoreHandle_t mutex_to_delete = g_lcd_mutex;

    lgfx_dev::destroy_all_sprites_locked();

    // Tear down LCD device (swap publication under mux to avoid publish/depublish races).
    PiyopiyoLGFX *to_delete = nullptr;

    portENTER_CRITICAL(&g_publication_mux);
    if (g_device_owner_token != owner_token) {
        portEXIT_CRITICAL(&g_publication_mux);
        release_lcd_mutex();
        return ESP_ERR_INVALID_STATE;
    }

    to_delete = g_lcd_device;
    g_lcd_device = nullptr;
    g_device_ready = false;
    g_device_owner_token = nullptr;
#if defined(INCLUDE_vSemaphoreDelete) && (INCLUDE_vSemaphoreDelete == 1)
    g_lcd_mutex = nullptr;
#endif
    portEXIT_CRITICAL(&g_publication_mux);

    if (to_delete) {
        // Best-effort cleanup of any in-flight write/transaction state.
        to_delete->endWrite();
        to_delete->endTransaction();
        delete to_delete;
    }

#if defined(INCLUDE_vSemaphoreDelete) && (INCLUDE_vSemaphoreDelete == 1)
    // Do not call release_lcd_mutex() after deleting the mutex.
    vSemaphoreDelete(mutex_to_delete);
#else
    // Fallback: keep mutex alive if delete API is unavailable.
    xSemaphoreGive(mutex_to_delete);
#endif

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_close_for_owner(const void *owner_token)
{
    // Protocol-level close maps to full teardown for the current owner.
    return lgfx_device_deinit_for_owner(owner_token);
}
