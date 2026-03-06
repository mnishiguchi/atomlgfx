// src/lgfx_device_state.cpp
#include <new>
#include <stddef.h>
#include <stdint.h>

#include <LovyanGFX.hpp>

// Build-time configuration (generated from include/lgfx_port/lgfx_port_config.h.in)
#include "lgfx_port/lgfx_port_config.h"

#if defined(LGFX_PORT_ENABLE_TOUCH) && (LGFX_PORT_ENABLE_TOUCH == 1)
#include <lgfx/v1/touch/Touch_XPT2046.hpp>
#endif

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

namespace
{

// ----------------------------------------------------------------------------
// Setup / state
// ----------------------------------------------------------------------------

static constexpr const char *TAG = "lgfx_device";

// ----------------------------------------------------------------------------
// Configuration requirements (no silent defaults)
// ----------------------------------------------------------------------------
//
// The port must include the generated config header and that header must define
// all of the knobs used here. If any are missing, fail the build loudly.
//
// This eliminates nondeterministic "fallback defaults" when the config header is
// missing, stale, or not included correctly.
//
#ifndef LGFX_PORT_MAX_SPRITES
#error "LGFX_PORT_MAX_SPRITES must be defined by lgfx_port_config.h"
#endif

#ifndef LGFX_PORT_SPI_SCLK_GPIO
#error "LGFX_PORT_SPI_SCLK_GPIO must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_SPI_MOSI_GPIO
#error "LGFX_PORT_SPI_MOSI_GPIO must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_SPI_MISO_GPIO
#error "LGFX_PORT_SPI_MISO_GPIO must be defined by lgfx_port_config.h"
#endif

#ifndef LGFX_PORT_LCD_CS_GPIO
#error "LGFX_PORT_LCD_CS_GPIO must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_DC_GPIO
#error "LGFX_PORT_LCD_DC_GPIO must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_RST_GPIO
#error "LGFX_PORT_LCD_RST_GPIO must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_SPI_HOST
#error "LGFX_PORT_LCD_SPI_HOST must be defined by lgfx_port_config.h"
#endif

#ifndef LGFX_PORT_PANEL_WIDTH
#error "LGFX_PORT_PANEL_WIDTH must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_PANEL_HEIGHT
#error "LGFX_PORT_PANEL_HEIGHT must be defined by lgfx_port_config.h"
#endif

#ifndef LGFX_PORT_LCD_SPI_MODE
#error "LGFX_PORT_LCD_SPI_MODE must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_FREQ_WRITE_HZ
#error "LGFX_PORT_LCD_FREQ_WRITE_HZ must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_FREQ_READ_HZ
#error "LGFX_PORT_LCD_FREQ_READ_HZ must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_SPI_3WIRE
#error "LGFX_PORT_LCD_SPI_3WIRE must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_USE_LOCK
#error "LGFX_PORT_LCD_USE_LOCK must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_DMA_CHANNEL
#error "LGFX_PORT_LCD_DMA_CHANNEL must be defined by lgfx_port_config.h"
#endif

#ifndef LGFX_PORT_LCD_PIN_BUSY
#error "LGFX_PORT_LCD_PIN_BUSY must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_OFFSET_X
#error "LGFX_PORT_LCD_OFFSET_X must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_OFFSET_Y
#error "LGFX_PORT_LCD_OFFSET_Y must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_OFFSET_ROTATION
#error "LGFX_PORT_LCD_OFFSET_ROTATION must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_DUMMY_READ_PIXEL
#error "LGFX_PORT_LCD_DUMMY_READ_PIXEL must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_DUMMY_READ_BITS
#error "LGFX_PORT_LCD_DUMMY_READ_BITS must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_READABLE
#error "LGFX_PORT_LCD_READABLE must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_INVERT
#error "LGFX_PORT_LCD_INVERT must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_RGB_ORDER
#error "LGFX_PORT_LCD_RGB_ORDER must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_DLEN_16BIT
#error "LGFX_PORT_LCD_DLEN_16BIT must be defined by lgfx_port_config.h"
#endif
#ifndef LGFX_PORT_LCD_BUS_SHARED
#error "LGFX_PORT_LCD_BUS_SHARED must be defined by lgfx_port_config.h"
#endif

#if defined(LGFX_PORT_ENABLE_TOUCH) && (LGFX_PORT_ENABLE_TOUCH == 1)
#ifndef LGFX_PORT_TOUCH_CS_GPIO
#error "LGFX_PORT_TOUCH_CS_GPIO must be defined by lgfx_port_config.h when touch is enabled"
#endif
#ifndef LGFX_PORT_TOUCH_IRQ_GPIO
#error "LGFX_PORT_TOUCH_IRQ_GPIO must be defined by lgfx_port_config.h when touch is enabled"
#endif
#ifndef LGFX_PORT_TOUCH_SPI_HOST
#error "LGFX_PORT_TOUCH_SPI_HOST must be defined by lgfx_port_config.h when touch is enabled"
#endif
#ifndef LGFX_PORT_TOUCH_SPI_FREQ_HZ
#error "LGFX_PORT_TOUCH_SPI_FREQ_HZ must be defined by lgfx_port_config.h when touch is enabled"
#endif
#ifndef LGFX_PORT_TOUCH_OFFSET_ROTATION
#error "LGFX_PORT_TOUCH_OFFSET_ROTATION must be defined by lgfx_port_config.h when touch is enabled"
#endif
#ifndef LGFX_PORT_TOUCH_BUS_SHARED
#error "LGFX_PORT_TOUCH_BUS_SHARED must be defined by lgfx_port_config.h when touch is enabled"
#endif
#endif

// ----------------------------------------------------------------------------
// Wiring / geometry / knobs (all sourced from generated config)
// ----------------------------------------------------------------------------

// SPI pins
static constexpr int PIN_SCLK = (int) (LGFX_PORT_SPI_SCLK_GPIO);
static constexpr int PIN_MOSI = (int) (LGFX_PORT_SPI_MOSI_GPIO);
static constexpr int PIN_MISO = (int) (LGFX_PORT_SPI_MISO_GPIO);

// LCD pins / host
static constexpr int PIN_LCD_CS = (int) (LGFX_PORT_LCD_CS_GPIO);
static constexpr int PIN_LCD_DC = (int) (LGFX_PORT_LCD_DC_GPIO);
static constexpr int PIN_LCD_RST = (int) (LGFX_PORT_LCD_RST_GPIO);

// Panel geometry
static constexpr uint16_t PANEL_W = (uint16_t) (LGFX_PORT_PANEL_WIDTH);
static constexpr uint16_t PANEL_H = (uint16_t) (LGFX_PORT_PANEL_HEIGHT);

#if defined(LGFX_PORT_ENABLE_TOUCH) && (LGFX_PORT_ENABLE_TOUCH == 1)
// XPT2046 touch (SPI).
// Leave CS as -1 to compile touch but keep it unattached.
static constexpr int PIN_TOUCH_CS = (int) (LGFX_PORT_TOUCH_CS_GPIO);
static constexpr int PIN_TOUCH_IRQ = (int) (LGFX_PORT_TOUCH_IRQ_GPIO);

static constexpr int TOUCH_SPI_HOST = (int) (LGFX_PORT_TOUCH_SPI_HOST);
static constexpr uint32_t TOUCH_SPI_FREQ_HZ = (uint32_t) (LGFX_PORT_TOUCH_SPI_FREQ_HZ);
static constexpr uint8_t TOUCH_OFFSET_ROTATION = (uint8_t) (LGFX_PORT_TOUCH_OFFSET_ROTATION);
#endif

// Lazily created to avoid C++ global ctors at boot
static bool is_initialized = false;

static constexpr uint16_t MAX_SPRITES = static_cast<uint16_t>(LGFX_PORT_MAX_SPRITES);
static constexpr uint8_t MAX_HANDLE = 254;

class PiyopiyoLGFX : public lgfx::LGFX_Device
{
    lgfx::Panel_ILI9488 panel_;
    lgfx::Bus_SPI bus_;

#if defined(LGFX_PORT_ENABLE_TOUCH) && (LGFX_PORT_ENABLE_TOUCH == 1)
    lgfx::Touch_XPT2046 touch_;
#endif

public:
    PiyopiyoLGFX()
    {
        // SPI bus config
        {
            auto cfg = bus_.config();

            cfg.spi_host = LGFX_PORT_LCD_SPI_HOST;
            cfg.spi_mode = (uint8_t) (LGFX_PORT_LCD_SPI_MODE);

            // Parenthesize expression-ish macros before casting (avoids precedence footguns)
            cfg.freq_write = (uint32_t) (LGFX_PORT_LCD_FREQ_WRITE_HZ);
            cfg.freq_read = (uint32_t) (LGFX_PORT_LCD_FREQ_READ_HZ);

            cfg.spi_3wire = ((LGFX_PORT_LCD_SPI_3WIRE) != 0);
            cfg.use_lock = ((LGFX_PORT_LCD_USE_LOCK) != 0);
            cfg.dma_channel = (LGFX_PORT_LCD_DMA_CHANNEL);

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
            cfg.pin_busy = (int) (LGFX_PORT_LCD_PIN_BUSY);

            cfg.panel_width = PANEL_W;
            cfg.panel_height = PANEL_H;

            cfg.offset_x = (int) (LGFX_PORT_LCD_OFFSET_X);
            cfg.offset_y = (int) (LGFX_PORT_LCD_OFFSET_Y);
            cfg.offset_rotation = (uint8_t) (LGFX_PORT_LCD_OFFSET_ROTATION);

            cfg.dummy_read_pixel = (uint8_t) (LGFX_PORT_LCD_DUMMY_READ_PIXEL);
            cfg.dummy_read_bits = (uint8_t) (LGFX_PORT_LCD_DUMMY_READ_BITS);

            cfg.readable = ((LGFX_PORT_LCD_READABLE) != 0);
            cfg.invert = ((LGFX_PORT_LCD_INVERT) != 0);
            cfg.rgb_order = ((LGFX_PORT_LCD_RGB_ORDER) != 0);
            cfg.dlen_16bit = ((LGFX_PORT_LCD_DLEN_16BIT) != 0);

            cfg.bus_shared = ((LGFX_PORT_LCD_BUS_SHARED) != 0);

            panel_.config(cfg);
        }

#if defined(LGFX_PORT_ENABLE_TOUCH) && (LGFX_PORT_ENABLE_TOUCH == 1)
        // Touch config (XPT2046)
        // - shares SPI host + SCLK/MOSI/MISO with the LCD bus
        // - requires a dedicated CS pin for touch
        // - optional IRQ pin reduces unnecessary reads when not touched
        if constexpr (PIN_TOUCH_CS >= 0) {
            auto cfg = touch_.config();

            cfg.spi_host = TOUCH_SPI_HOST; // should match the LCD bus host
            cfg.freq = TOUCH_SPI_FREQ_HZ; // start conservative for stability

            cfg.pin_sclk = PIN_SCLK;
            cfg.pin_mosi = PIN_MOSI;
            cfg.pin_miso = PIN_MISO;

            cfg.pin_cs = PIN_TOUCH_CS;
            cfg.pin_int = PIN_TOUCH_IRQ; // -1 if not wired

            cfg.bus_shared = ((LGFX_PORT_TOUCH_BUS_SHARED) != 0);

            // Touch coordinate alignment relative to the panel orientation.
            // If left/right is swapped or rotation feels off, try adjusting:
            //   -DLGFX_PORT_TOUCH_OFFSET_ROTATION=1/2/3/... (0..7)
            // Then run SampleApp :touch_calibrate for best results.
            cfg.offset_rotation = TOUCH_OFFSET_ROTATION;

            touch_.config(cfg);
            panel_.setTouch(&touch_);

            ESP_LOGI(
                TAG,
                "touch attached: cs=%d irq=%d host=%d freq=%u offset_rotation=%u",
                PIN_TOUCH_CS,
                PIN_TOUCH_IRQ,
                (int) TOUCH_SPI_HOST,
                (unsigned) TOUCH_SPI_FREQ_HZ,
                (unsigned) TOUCH_OFFSET_ROTATION);
        } else {
            ESP_LOGI(TAG, "touch compiled but unattached (LGFX_PORT_TOUCH_CS_GPIO=-1)");
        }
#endif

        setPanel(&panel_);
    }
};

static PiyopiyoLGFX *lcd = nullptr;
static SemaphoreHandle_t lcd_lock = nullptr;

// Protects lazy init / pointer publication (kept very short; do not block while holding it).
static portMUX_TYPE g_init_mux = portMUX_INITIALIZER_UNLOCKED;

static constexpr size_t SPRITE_SLOTS = (size_t) MAX_HANDLE + 1u; // handle 0 reserved for LCD
static lgfx::LGFX_Sprite *sprites[SPRITE_SLOTS] = { 0 };
static uint16_t sprite_count = 0;

static inline void ensure_lock_created()
{
    if (lcd_lock) {
        return;
    }

    // Create outside the critical section (xSemaphoreCreateMutex may allocate).
    SemaphoreHandle_t created = xSemaphoreCreateMutex();
    if (!created) {
        ESP_LOGE(TAG, "failed to create mutex");
        return;
    }

    portENTER_CRITICAL(&g_init_mux);
    if (!lcd_lock) {
        lcd_lock = created;
        created = nullptr;
    }
    portEXIT_CRITICAL(&g_init_mux);

#if defined(INCLUDE_vSemaphoreDelete) && (INCLUDE_vSemaphoreDelete == 1)
    // If we lost the race, delete the extra mutex.
    if (created) {
        vSemaphoreDelete(created);
    }
#else
    (void) created; // best-effort: leak only on rare init race if delete not available
#endif
}

static esp_err_t ensure_allocated()
{
    ensure_lock_created();
    if (!lcd_lock) {
        return ESP_ERR_NO_MEM;
    }

    if (lcd) {
        return ESP_OK;
    }

    // Allocate outside the critical section (new may allocate).
    PiyopiyoLGFX *created = new (std::nothrow) PiyopiyoLGFX();
    if (!created) {
        ESP_LOGE(TAG, "failed to allocate LGFX device");
        return ESP_ERR_NO_MEM;
    }

    portENTER_CRITICAL(&g_init_mux);
    if (!lcd) {
        lcd = created;
        created = nullptr;
    }
    portEXIT_CRITICAL(&g_init_mux);

    // If we lost the race, destroy the extra instance.
    if (created) {
        delete created;
    }

    return ESP_OK;
}

static bool lock_lcd()
{
    ensure_lock_created();
    if (!lcd_lock) {
        return false;
    }
    return xSemaphoreTake(lcd_lock, portMAX_DELAY) == pdTRUE;
}

static void unlock_lcd()
{
    if (lcd_lock) {
        xSemaphoreGive(lcd_lock);
    }
}

static inline lgfx::LGFXBase *resolve_target(uint8_t target)
{
    if (target == 0) {
        return lcd;
    }
    if (target > MAX_HANDLE) {
        return nullptr;
    }
    return sprites[target];
}

} // namespace

// -----------------------------------------------------------------------------
// Internal shared API for split files (definitions)
// -----------------------------------------------------------------------------

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

esp_err_t ensure_allocated()
{
    return ::ensure_allocated();
}

bool lock_lcd()
{
    return ::lock_lcd();
}

void unlock_lcd()
{
    ::unlock_lcd();
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
    esp_err_t err = lgfx_dev::ensure_allocated();
    if (err != ESP_OK) {
        return err;
    }

    lock.lock();
    if (!lock.is_locked()) {
        return ESP_ERR_NO_MEM;
    }

    if (!is_initialized || !lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

lgfx::LGFX_Device *lcd_device_locked()
{
    return lcd;
}

bool is_initialized_locked()
{
    return is_initialized;
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

extern "C" esp_err_t lgfx_device_init(void)
{
    esp_err_t err = ensure_allocated();
    if (err != ESP_OK) {
        return err;
    }

    if (!lock_lcd()) {
        return ESP_ERR_NO_MEM;
    }

    // Defensive: close() may have raced after ensure_allocated() and before we got the lock.
    if (!lcd) {
        unlock_lcd();
        return ESP_ERR_INVALID_STATE;
    }

    if (!is_initialized) {
#if defined(LGFX_PORT_ENABLE_TOUCH) && (LGFX_PORT_ENABLE_TOUCH == 1)
        if constexpr (PIN_TOUCH_CS < 0) {
            ESP_LOGW(TAG, "touch enabled (LGFX_PORT_ENABLE_TOUCH=1) but not attached (LGFX_PORT_TOUCH_CS_GPIO=-1)");
        }
#endif

        ESP_LOGI(TAG, "init/begin");
        lcd->begin();
        is_initialized = true;

        // sane defaults
        lcd->setTextSize(1);
        lcd->setTextDatum(textdatum_t::top_left);
    }

    unlock_lcd();
    return ESP_OK;
}

static esp_err_t lgfx_device_deinit(void)
{
    // Idempotent teardown.
    // Allow deinit/close even if init never happened.
    ensure_lock_created();
    if (!lcd_lock) {
        return ESP_ERR_NO_MEM;
    }

    if (!lock_lcd()) {
        return ESP_ERR_NO_MEM;
    }

    // Keep the held mutex handle so we can delete it at the end.
    SemaphoreHandle_t lock_to_delete = lcd_lock;

    lgfx_dev::destroy_all_sprites_locked();

    // Tear down LCD device (swap pointers under init mux to avoid init/deinit races)
    PiyopiyoLGFX *to_delete = nullptr;

    portENTER_CRITICAL(&g_init_mux);
    to_delete = lcd;
    lcd = nullptr;
    is_initialized = false;
#if defined(INCLUDE_vSemaphoreDelete) && (INCLUDE_vSemaphoreDelete == 1)
    lcd_lock = nullptr;
#endif
    portEXIT_CRITICAL(&g_init_mux);

    if (to_delete) {
        // Best-effort cleanup. If you distrust these calls, remove them.
        to_delete->endWrite();
        to_delete->endTransaction();
        delete to_delete;
    }

#if defined(INCLUDE_vSemaphoreDelete) && (INCLUDE_vSemaphoreDelete == 1)
    // Do not call unlock_lcd() after deleting the mutex.
    vSemaphoreDelete(lock_to_delete);
#else
    // Fallback: keep mutex alive if delete API is unavailable.
    xSemaphoreGive(lock_to_delete);
#endif

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_close(void)
{
    // Protocol-level close maps to full teardown.
    return lgfx_device_deinit();
}
