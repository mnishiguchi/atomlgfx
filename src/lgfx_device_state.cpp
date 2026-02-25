// src/lgfx_device_state.cpp

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

#include <new>
#include <stddef.h>
#include <stdint.h>

#include <LovyanGFX.hpp>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#include "lgfx_port/caps.h"

namespace
{

// ----------------------------------------------------------------------------
// Setup / state
// ----------------------------------------------------------------------------

static constexpr const char *TAG = "lgfx_device";

// piyopiyo-pcb v1.5 GPIO mapping
static constexpr int PIN_SCLK = 7;
static constexpr int PIN_MOSI = 9;
static constexpr int PIN_MISO = 8;

static constexpr int PIN_LCD_CS = 43;
static constexpr int PIN_LCD_DC = 3;
static constexpr int PIN_LCD_RST = 2;

static constexpr uint16_t PANEL_W = 320;
static constexpr uint16_t PANEL_H = 480;

// Lazily created to avoid C++ global ctors at boot
static bool is_initialized = false;

static constexpr uint16_t MAX_SPRITES = static_cast<uint16_t>(LGFX_PORT_MAX_SPRITES);
static constexpr uint8_t MAX_HANDLE = 254;

// NOTE: Feature bits are intentionally kept local to this component.
// If your protocol expects a specific bit layout, align these with protocol caps.
static constexpr uint32_t FEATURE_STRIDED_PUSH_IMAGE = 1u << 0;
static constexpr uint32_t FEATURE_SPRITES = 1u << 1;
static constexpr uint32_t FEATURE_SPRITE_REGION = 1u << 2;
static constexpr uint32_t FEATURE_LCD_WRITE_PATH = 1u << 3;

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

            // Conservative clocks for stability (tune later)
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

            cfg.panel_width = PANEL_W;
            cfg.panel_height = PANEL_H;

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

static uint8_t alloc_sprite_handle_locked()
{
    if (sprite_count >= MAX_SPRITES) {
        return 0;
    }
    for (int i = 1; i <= MAX_HANDLE; i++) {
        if (!sprites[i]) {
            return (uint8_t) i;
        }
    }
    return 0;
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

uint32_t feature_bits_const()
{
    uint32_t bits = 0;
    bits |= FEATURE_STRIDED_PUSH_IMAGE;
    bits |= FEATURE_SPRITES;
    bits |= FEATURE_SPRITE_REGION;
    bits |= FEATURE_LCD_WRITE_PATH;
    return bits;
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

uint8_t alloc_sprite_handle_locked()
{
    return ::alloc_sprite_handle_locked();
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
// API: validation
// ----------------------------------------------------------------------------

extern "C" bool lgfx_device_is_valid_target(uint8_t target)
{
    if (target == 0) {
        return true;
    }

    return target <= MAX_HANDLE;
}

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

    // Defensive: close()/deinit() may have raced after ensure_allocated() and before we got the lock.
    if (!lcd) {
        unlock_lcd();
        return ESP_ERR_INVALID_STATE;
    }

    if (!is_initialized) {
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

extern "C" esp_err_t lgfx_device_deinit(void)
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
