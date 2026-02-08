#include "lgfx_device.h"

#include <new>
#include <string.h>

#include <LovyanGFX.hpp>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace
{
static constexpr const char *TAG = "lgfx_device";

// piyopiyo-pcb v1.5 GPIO mapping
static constexpr int PIN_SCLK = 7;
static constexpr int PIN_MOSI = 9;
static constexpr int PIN_MISO = 8;

static constexpr int PIN_LCD_CS = 43;
static constexpr int PIN_LCD_DC = 3;
static constexpr int PIN_LCD_RST = 2;

static bool is_initialized = false;

class PiyopiyoLGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9488 panel_;
    lgfx::Bus_SPI bus_;

public:
    PiyopiyoLGFX()
    {
        // SPI bus config
        {
            auto cfg = bus_.config();

            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;

            // Conservative clocks for stability
            cfg.freq_write = 20 * 1000 * 1000;
            cfg.freq_read = 10 * 1000 * 1000;

            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;

            cfg.pin_sclk = PIN_SCLK;
            cfg.pin_mosi = PIN_MOSI;
            cfg.pin_miso = PIN_MISO;
            cfg.pin_dc = PIN_LCD_DC;

            bus_.config(cfg);
            panel_.setBus(&bus_);
        }

        // Panel config
        {
            auto cfg = panel_.config();

            cfg.pin_cs = PIN_LCD_CS;
            cfg.pin_rst = PIN_LCD_RST;
            cfg.pin_busy = -1;

            cfg.panel_width = 320;
            cfg.panel_height = 480;

            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;

            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;

            cfg.readable = false;
            cfg.invert = false;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;

            cfg.bus_shared = true;

            panel_.config(cfg);
        }

        setPanel(&panel_);
    }
};

// Lazily created to avoid C++ global constructors at boot
static PiyopiyoLGFX *lcd = nullptr;
static SemaphoreHandle_t lcd_lock = nullptr;

static esp_err_t ensure_allocated()
{
    if (!lcd_lock) {
        lcd_lock = xSemaphoreCreateMutex();
        if (!lcd_lock) {
            ESP_LOGE(TAG, "failed to create mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    if (!lcd) {
        lcd = new (std::nothrow) PiyopiyoLGFX();
        if (!lcd) {
            ESP_LOGE(TAG, "failed to allocate LGFX device");
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}

static void lock_lcd()
{
    if (lcd_lock) {
        xSemaphoreTake(lcd_lock, portMAX_DELAY);
    }
}

static void unlock_lcd()
{
    if (lcd_lock) {
        xSemaphoreGive(lcd_lock);
    }
}

} // namespace

extern "C" esp_err_t lgfx_device_init(uint8_t rotation)
{
    esp_err_t err = ensure_allocated();
    if (err != ESP_OK) {
        return err;
    }

    lock_lcd();

    if (!is_initialized) {
        ESP_LOGI(TAG, "begin");
        lcd->begin();
        is_initialized = true;
    }

    lcd->setRotation(rotation & 0x07);

    unlock_lcd();
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_fill_screen(uint16_t rgb565)
{
    if (!is_initialized || !lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    lock_lcd();
    lcd->fillScreen(rgb565);
    unlock_lcd();

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_draw_text(
    int16_t x,
    int16_t y,
    uint16_t rgb565,
    uint8_t text_size,
    const uint8_t *text_bytes,
    uint16_t text_len)
{
    if (!is_initialized || !lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!text_bytes || text_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Avoid heap allocations for MVP
    char buf[256];
    size_t n = text_len;
    if (n > sizeof(buf) - 1) {
        n = sizeof(buf) - 1;
    }
    memcpy(buf, text_bytes, n);
    buf[n] = '\0';

    // Clamp text size
    if (text_size == 0) {
        text_size = 1;
    } else if (text_size > 8) {
        text_size = 8;
    }

    lock_lcd();
    lcd->setTextSize(text_size);
    lcd->setTextColor(rgb565);
    lcd->drawString(buf, x, y);
    unlock_lcd();

    return ESP_OK;
}

