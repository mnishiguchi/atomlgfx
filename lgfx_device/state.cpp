// SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
//
// SPDX-License-Identifier: Apache-2.0

// lgfx_device/state.cpp

#include <new>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/semphr.h"

#include "lgfx_device/lgfx_device.h"
#include "lgfx_device/lgfx_device_internal.hpp"
#include "lgfx_device/state_runtime.hpp"

namespace
{

// ----------------------------------------------------------------------------
// Setup / state
// ----------------------------------------------------------------------------

static constexpr const char *TAG = "lgfx_device";

// Protects singleton publication state. Keep critical sections short and allocation-free.
static portMUX_TYPE g_publication_mux = portMUX_INITIALIZER_UNLOCKED;

static lgfx::LGFX_Device *g_lcd_device = nullptr;
static SemaphoreHandle_t g_lcd_mutex = nullptr;

// Live singleton owner token. Null means no current owner.
static const void *g_device_owner_token = nullptr;

// True only after begin() succeeds for the currently published singleton.
static bool g_device_ready = false;

// ----------------------------------------------------------------------------
// Shared compile-time constants
// ----------------------------------------------------------------------------

static_assert((LGFX_PORT_MAX_SPRITES) >= 1u && (LGFX_PORT_MAX_SPRITES) <= 254u,
    "LGFX_PORT_MAX_SPRITES must be in 1..254");

static constexpr uint16_t MAX_SPRITES = static_cast<uint16_t>(LGFX_PORT_MAX_SPRITES);
static constexpr uint8_t MAX_HANDLE = 254;

// ----------------------------------------------------------------------------
// Publication / ownership snapshots
// ----------------------------------------------------------------------------

struct DevicePublicationSnapshot
{
    lgfx::LGFX_Device *lcd;
    const void *owner_token;
    bool ready;
};

// Internal LCD presentation state.
//
// Current behavior:
//
// - target 0 resolves to the active native strip buffer only while a strip
//   frame is open
// - presentation_present_strip_locked() blits that strip to the live LCD at y0
// - display() remains the final LCD flush
// - public target numbering stays unchanged
//
// This is a transitional native presentation layer:
// - strip buffers are allocated lazily
// - allocation uses adaptive double strip buffers
// - failure falls back to direct LCD rendering
// - the current Elixir demo may still own higher-level strip orchestration
struct LcdPresentationState
{
    bool enabled;
    bool attempted;

    uint16_t lcd_width;
    uint16_t lcd_height;
    uint16_t strip_height;
    uint16_t current_strip_y;

    bool frame_active;
    bool swap_bytes_enabled;

    uint8_t next_buffer_index;

    lgfx::LGFX_Sprite *front;
    lgfx::LGFX_Sprite *back;
    lgfx::LGFX_Sprite *current;
};

static LcdPresentationState g_presentation = {};

static inline void reset_presentation_state_locked()
{
    g_presentation.enabled = false;
    g_presentation.attempted = false;
    g_presentation.lcd_width = 0;
    g_presentation.lcd_height = 0;
    g_presentation.strip_height = 0;
    g_presentation.current_strip_y = 0;
    g_presentation.frame_active = false;
    g_presentation.swap_bytes_enabled = false;
    g_presentation.next_buffer_index = 0;
    g_presentation.front = nullptr;
    g_presentation.back = nullptr;
    g_presentation.current = nullptr;
}

static inline void destroy_presentation_sprite(lgfx::LGFX_Sprite *&spr)
{
    if (spr != nullptr) {
        spr->deleteSprite();
        delete spr;
        spr = nullptr;
    }
}

static inline uint16_t clamp_requested_strip_height(uint16_t lcd_h, uint16_t requested)
{
    if (lcd_h == 0) {
        return 0;
    }

    if (requested == 0 || requested > lcd_h) {
        return (lcd_h > 160) ? 160 : lcd_h;
    }

    return requested;
}

static inline uint16_t next_smaller_strip_height(uint16_t current)
{
    if (current <= 1) {
        return 0;
    }

    uint16_t next = current / 2;
    return next == 0 ? 1 : next;
}

static lgfx::LGFX_Sprite *create_presentation_strip_sprite(
    lgfx::LGFX_Device *lcd,
    uint16_t strip_w,
    uint16_t strip_h,
    bool swap_bytes_enabled)
{
    auto *spr = new (std::nothrow) lgfx::LGFX_Sprite(lcd);
    if (!spr) {
        return nullptr;
    }

    const uint8_t depth = static_cast<uint8_t>(lcd->getColorDepth());
    if (depth != 0) {
        spr->setColorDepth(depth);
    }

    spr->createSprite(strip_w, strip_h);
    if (spr->getBuffer() == nullptr) {
        delete spr;
        return nullptr;
    }

    spr->setSwapBytes(swap_bytes_enabled);
    spr->setTextSize(1.0f);
    spr->setTextDatum(textdatum_t::top_left);
    return spr;
}

static esp_err_t presentation_try_allocate_locked(
    lgfx::LGFX_Device *lcd,
    uint16_t lcd_w,
    uint16_t lcd_h,
    uint16_t strip_h)
{
    lgfx::LGFX_Sprite *front = create_presentation_strip_sprite(
        lcd,
        lcd_w,
        strip_h,
        g_presentation.swap_bytes_enabled);

    if (!front) {
        return ESP_ERR_NO_MEM;
    }

    lgfx::LGFX_Sprite *back = create_presentation_strip_sprite(
        lcd,
        lcd_w,
        strip_h,
        g_presentation.swap_bytes_enabled);

    if (!back) {
        destroy_presentation_sprite(front);
        return ESP_ERR_NO_MEM;
    }

    destroy_presentation_sprite(g_presentation.front);
    destroy_presentation_sprite(g_presentation.back);

    g_presentation.front = front;
    g_presentation.back = back;
    g_presentation.current = nullptr;

    g_presentation.enabled = true;
    g_presentation.attempted = true;
    g_presentation.lcd_width = lcd_w;
    g_presentation.lcd_height = lcd_h;
    g_presentation.strip_height = strip_h;
    g_presentation.current_strip_y = 0;
    g_presentation.frame_active = false;
    g_presentation.next_buffer_index = 0;

    return ESP_OK;
}

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
// Sprite registry
// ----------------------------------------------------------------------------

static constexpr size_t SPRITE_SLOTS = (size_t) MAX_HANDLE + 1u; // handle 0 reserved for LCD
static lgfx::LGFX_Sprite *sprites[SPRITE_SLOTS] = { 0 };
static uint16_t sprite_count = 0;

// ----------------------------------------------------------------------------
// Mutex helpers
// ----------------------------------------------------------------------------

static inline void ensure_lcd_mutex_created()
{
    if (g_lcd_mutex) {
        return;
    }

    // Create outside the critical section because xSemaphoreCreateMutex may allocate.
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
    (void) created; // Best-effort leak only on a rare init race when delete is unavailable.
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

static inline void force_end_write_all(lgfx::LGFX_Device *lcd)
{
    if (lcd == nullptr) {
        return;
    }

    while (lcd->getStartCount() != 0u) {
        lcd->endWrite();
    }
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

    lgfx_dev::LgfxRuntimeConfig config = lgfx_dev::runtime_config_with_overrides(overrides);

    const char *validation_error = nullptr;
    if (!lgfx_dev::validate_runtime_config(config, &validation_error)) {
        ESP_LOGE(TAG, "invalid open-time runtime config: %s", validation_error);
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::log_runtime_config(config);

    // Allocate outside the critical section because new may allocate.
    lgfx::LGFX_Device *created = lgfx_dev::create_lcd_device(config);
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
        lgfx_dev::destroy_lcd_device(created);
    }

    return publish_err;
}

} // namespace

// ----------------------------------------------------------------------------
// Internal shared API for split files
// ----------------------------------------------------------------------------

namespace lgfx_dev
{

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

esp_err_t start_write()
{
    ScopedLcdLock lock;
    esp_err_t err = lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *lcd = lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    lcd->startWrite();
    return ESP_OK;
}

esp_err_t end_write()
{
    ScopedLcdLock lock;
    esp_err_t err = lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *lcd = lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    // Keep protocol/device contract aligned with LovyanGFX:
    // endWrite() is tolerated even when the nesting count is already zero.
    lcd->endWrite();
    return ESP_OK;
}

esp_err_t start_write_locked()
{
    auto *lcd = lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    lcd->startWrite();
    return ESP_OK;
}

esp_err_t end_write_locked()
{
    auto *lcd = lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    lcd->endWrite();
    return ESP_OK;
}

lgfx::LGFX_Device *lcd_device_locked()
{
    return g_lcd_device;
}

lgfx::LGFXBase *resolve_target_locked(uint8_t target)
{
    return ::resolve_target(target);
}

lgfx::LGFXBase *resolve_render_target_locked(uint8_t target)
{
    if (target != 0) {
        return ::resolve_target(target);
    }

    if (g_presentation.enabled && g_presentation.frame_active && g_presentation.current != nullptr) {
        return g_presentation.current;
    }

    return g_lcd_device;
}

lgfx::LovyanGFX *resolve_render_surface_locked(uint8_t target)
{
    if (target != 0) {
        return nullptr;
    }

    if (g_presentation.enabled && g_presentation.frame_active && g_presentation.current != nullptr) {
        return g_presentation.current;
    }

    return g_lcd_device;
}

bool presentation_enabled_locked()
{
    return g_presentation.enabled;
}

esp_err_t presentation_reset_locked()
{
    reset_presentation_state_locked();
    return ESP_OK;
}

esp_err_t presentation_configure_locked(uint16_t lcd_w, uint16_t lcd_h, uint16_t strip_h)
{
    g_presentation.enabled = false;
    g_presentation.attempted = false;
    g_presentation.lcd_width = lcd_w;
    g_presentation.lcd_height = lcd_h;
    g_presentation.strip_height = strip_h;
    g_presentation.current_strip_y = 0;
    g_presentation.frame_active = false;
    g_presentation.next_buffer_index = 0;
    g_presentation.front = nullptr;
    g_presentation.back = nullptr;
    g_presentation.current = nullptr;
    return ESP_OK;
}

uint16_t presentation_strip_height_locked()
{
    return g_presentation.strip_height;
}

esp_err_t presentation_ensure_buffers_locked()
{
    if (g_presentation.enabled && g_presentation.front != nullptr && g_presentation.back != nullptr) {
        return ESP_OK;
    }

    if (g_presentation.attempted) {
        return ESP_ERR_NO_MEM;
    }

    auto *lcd = lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t lcd_w = g_presentation.lcd_width;
    uint16_t lcd_h = g_presentation.lcd_height;

    if (lcd_w == 0 || lcd_h == 0) {
        lcd_w = static_cast<uint16_t>(lcd->width());
        lcd_h = static_cast<uint16_t>(lcd->height());
    }

    uint16_t strip_h = clamp_requested_strip_height(lcd_h, g_presentation.strip_height);
    g_presentation.attempted = true;

    while (strip_h != 0) {
        esp_err_t err = presentation_try_allocate_locked(lcd, lcd_w, lcd_h, strip_h);
        if (err == ESP_OK) {
            ESP_LOGI(
                TAG,
                "presentation strips enabled width=%u height=%u strip_h=%u",
                static_cast<unsigned>(lcd_w),
                static_cast<unsigned>(lcd_h),
                static_cast<unsigned>(strip_h));
            return ESP_OK;
        }

        strip_h = next_smaller_strip_height(strip_h);
    }

    g_presentation.enabled = false;
    g_presentation.current = nullptr;
    destroy_presentation_sprite(g_presentation.front);
    destroy_presentation_sprite(g_presentation.back);
    return ESP_ERR_NO_MEM;
}

esp_err_t presentation_begin_strip_locked(uint16_t y0)
{
    if (!g_presentation.enabled) {
        esp_err_t err = presentation_ensure_buffers_locked();
        if (err == ESP_ERR_NO_MEM) {
            return ESP_ERR_NOT_SUPPORTED;
        }

        if (err != ESP_OK) {
            return err;
        }
    }

    if (y0 >= g_presentation.lcd_height) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx::LGFX_Sprite *next = (g_presentation.next_buffer_index == 0) ? g_presentation.front : g_presentation.back;

    if (next == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    g_presentation.next_buffer_index ^= 1;
    g_presentation.current = next;
    g_presentation.current_strip_y = y0;
    g_presentation.frame_active = true;

    next->clearClipRect();
    next->setCursor(0, 0);
    next->setTextSize(1.0f);
    next->setTextDatum(textdatum_t::top_left);

    return ESP_OK;
}

esp_err_t presentation_present_strip_locked()
{
    if (!g_presentation.enabled) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    auto *lcd = lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    auto *current = g_presentation.current;
    if (!g_presentation.frame_active || current == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    current->pushSprite(lcd, 0, static_cast<int32_t>(g_presentation.current_strip_y));
    g_presentation.current = nullptr;
    g_presentation.frame_active = false;
    return ESP_OK;
}

esp_err_t presentation_rebuild_locked()
{
    auto *lcd = lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    const bool swap_bytes_enabled = g_presentation.swap_bytes_enabled;

    const uint16_t lcd_w = static_cast<uint16_t>(lcd->width());
    const uint16_t lcd_h = static_cast<uint16_t>(lcd->height());
    const uint16_t strip_h = g_presentation.strip_height;

    (void) presentation_destroy_buffers_locked();
    (void) presentation_configure_locked(lcd_w, lcd_h, strip_h);

    g_presentation.swap_bytes_enabled = swap_bytes_enabled;
    lcd->setSwapBytes(swap_bytes_enabled);

    // Do not auto-reallocate native strip buffers here.
    // Keep native strip presentation dormant until explicitly requested
    // through presentation_begin_strip_locked().
    return ESP_OK;
}

esp_err_t presentation_present_locked()
{
    if (!g_presentation.enabled) {
        return ESP_OK;
    }

    if (g_presentation.frame_active) {
        return presentation_present_strip_locked();
    }

    return ESP_OK;
}

esp_err_t presentation_destroy_buffers_locked()
{
    g_presentation.current = nullptr;
    destroy_presentation_sprite(g_presentation.front);
    destroy_presentation_sprite(g_presentation.back);
    reset_presentation_state_locked();
    return ESP_OK;
}

esp_err_t presentation_set_color_depth_locked(uint8_t depth)
{
    auto *lcd = lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    lcd->setColorDepth(depth);

    if (!g_presentation.enabled && !g_presentation.attempted) {
        return ESP_OK;
    }

    return presentation_rebuild_locked();
}

esp_err_t presentation_set_swap_bytes_locked(bool enabled)
{
    auto *lcd = lcd_device_locked();
    if (!lcd) {
        return ESP_ERR_INVALID_STATE;
    }

    lcd->setSwapBytes(enabled);
    g_presentation.swap_bytes_enabled = enabled;

    if (g_presentation.front != nullptr) {
        g_presentation.front->setSwapBytes(enabled);
    }

    if (g_presentation.back != nullptr) {
        g_presentation.back->setSwapBytes(enabled);
    }

    return ESP_OK;
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

        // Release sprite buffers before deleting the object.
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

    lgfx_dev::LgfxRuntimeConfig config = lgfx_dev::runtime_config_with_overrides(overrides);

    const char *validation_error = nullptr;
    if (!lgfx_dev::validate_runtime_config(config, &validation_error)) {
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

    // Defensive: close() may have raced before the mutex was acquired.
    if (!snapshot_is_owned_live_device(snapshot, owner_token)) {
        release_lcd_mutex();
        return ESP_ERR_INVALID_STATE;
    }

    if (!g_device_ready) {
        ESP_LOGI(TAG, "init/begin");
        snapshot.lcd->begin();
        g_device_ready = true;

        // Default text state after begin().
        snapshot.lcd->setTextSize(1);
        snapshot.lcd->setTextDatum(textdatum_t::top_left);
    }

    // Tear down any previously allocated native presentation buffers before
    // reseeding presentation state for this init call.
    (void) lgfx_dev::presentation_destroy_buffers_locked();
    (void) lgfx_dev::presentation_configure_locked(
        static_cast<uint16_t>(snapshot.lcd->width()),
        static_cast<uint16_t>(snapshot.lcd->height()),
        160);

    // Do not allocate native strip buffers during init yet.
    // The current Elixir MovingIcons sample still owns its own strip sprites.
    // Eager native strip allocation here causes two strip-buffer systems to
    // coexist and increases memory pressure unnecessarily.

    release_lcd_mutex();
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_presentation_get_strip_height(uint16_t *out_strip_height)
{
    if (out_strip_height == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    if (!lgfx_dev::presentation_enabled_locked()) {
        err = lgfx_dev::presentation_ensure_buffers_locked();
        if (err == ESP_ERR_NO_MEM) {
            return ESP_ERR_NOT_SUPPORTED;
        }

        if (err != ESP_OK) {
            return err;
        }
    }

    *out_strip_height = lgfx_dev::presentation_strip_height_locked();
    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_presentation_begin_strip(uint16_t y0)
{
    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    return lgfx_dev::presentation_begin_strip_locked(y0);
}

extern "C" esp_err_t lgfx_device_presentation_present_strip(void)
{
    lgfx_dev::ScopedLcdLock lock;
    esp_err_t err = lgfx_dev::lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    return lgfx_dev::presentation_present_strip_locked();
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

    // Idempotent teardown for the owning port. Allow close even if begin() never ran.
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

    // Keep the held mutex handle so it can be deleted at the end.
    SemaphoreHandle_t mutex_to_delete = g_lcd_mutex;

    (void) lgfx_dev::presentation_destroy_buffers_locked();
    lgfx_dev::destroy_all_sprites_locked();

    // Swap publication state under mux to avoid publish/depublish races.
    lgfx::LGFX_Device *to_delete = nullptr;

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
        // Force-unwind any open LovyanGFX write nesting before teardown.
        force_end_write_all(to_delete);
        lgfx_dev::destroy_lcd_device(to_delete);
    }

#if defined(INCLUDE_vSemaphoreDelete) && (INCLUDE_vSemaphoreDelete == 1)
    // Do not call release_lcd_mutex() after deleting the mutex.
    vSemaphoreDelete(mutex_to_delete);
#else
    // Fallback: keep the mutex alive if delete is unavailable.
    xSemaphoreGive(mutex_to_delete);
#endif

    return ESP_OK;
}

extern "C" esp_err_t lgfx_device_close_for_owner(const void *owner_token)
{
    // Protocol-level close maps to full teardown for the current owner.
    return lgfx_device_deinit_for_owner(owner_token);
}
