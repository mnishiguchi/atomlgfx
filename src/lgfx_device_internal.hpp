// src/lgfx_device_internal.hpp
//
// Internal-only shared contract for split lgfx_device implementation files.
// Mutable state is owned by src/lgfx_device_state.cpp.
//
// This header is C++-only and must not be exposed as public API.

#ifndef LGFX_DEVICE_INTERNAL_HPP
#define LGFX_DEVICE_INTERNAL_HPP

#include <stdint.h>

#include <LovyanGFX.hpp>

#include "esp_err.h"

namespace lgfx_dev
{

// -----------------------------------------------------------------------------
// Shared constants / metadata (owned by lgfx_device_state.cpp)
// -----------------------------------------------------------------------------

uint16_t panel_width_const();
uint16_t panel_height_const();

uint8_t max_handle_const();
uint16_t max_sprites_const();

// -----------------------------------------------------------------------------
// Shared lock / lifecycle helpers (owned by lgfx_device_state.cpp)
// -----------------------------------------------------------------------------

esp_err_t ensure_allocated();

bool lock_lcd();
void unlock_lcd();

class ScopedLcdLock
{
public:
    ScopedLcdLock() = default;
    ScopedLcdLock(const ScopedLcdLock &) = delete;
    ScopedLcdLock &operator=(const ScopedLcdLock &) = delete;

    void lock();
    bool is_locked() const;
    ~ScopedLcdLock();

private:
    bool locked_ = false;
};

esp_err_t lock_ready(ScopedLcdLock &lock);

// -----------------------------------------------------------------------------
// Shared state accessors (must be called while LCD lock is held unless noted)
// -----------------------------------------------------------------------------

// Returns LCD singleton as a generic LGFX device pointer (or nullptr).
lgfx::LGFX_Device *lcd_device_locked();

// Returns true iff init/begin has completed successfully.
bool is_initialized_locked();

// Resolve target 0 => LCD, 1..MAX_HANDLE => sprite. Returns nullptr if invalid/missing.
lgfx::LGFXBase *resolve_target_locked(uint8_t target);

// Resolve sprite handle only (1..MAX_HANDLE). Returns nullptr if invalid/missing.
lgfx::LGFX_Sprite *resolve_sprite_locked(uint8_t handle);

// Sprite slot mutation helpers (for sprite create/delete split into sprites.cpp).
void set_sprite_locked(uint8_t handle, lgfx::LGFX_Sprite *spr);
void clear_sprite_locked(uint8_t handle);

void increment_sprite_count_locked();
void decrement_sprite_count_locked();
uint32_t sprite_count_locked();

// Teardown helper: deletes all sprite buffers + objects and resets registry.
void destroy_all_sprites_locked();

// -----------------------------------------------------------------------------
// Shared wrappers (inline templates)
// -----------------------------------------------------------------------------

template <typename F>
inline esp_err_t with_lcd(F &&fn)
{
    ScopedLcdLock lock;
    esp_err_t err = lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    auto *d = lcd_device_locked();
    if (!d) {
        return ESP_ERR_INVALID_STATE;
    }

    fn(d);
    return ESP_OK;
}

template <typename F>
inline esp_err_t with_target(uint8_t target, F &&fn)
{
    ScopedLcdLock lock;
    esp_err_t err = lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    lgfx::LGFXBase *gfx = resolve_target_locked(target);
    if (!gfx) {
        return ESP_ERR_NOT_FOUND;
    }

    fn(gfx);
    return ESP_OK;
}

template <typename F>
inline esp_err_t with_sprite(uint8_t handle, F &&fn)
{
    ScopedLcdLock lock;
    esp_err_t err = lock_ready(lock);
    if (err != ESP_OK) {
        return err;
    }

    lgfx::LGFX_Sprite *spr = resolve_sprite_locked(handle);
    if (!spr) {
        return ESP_ERR_NOT_FOUND;
    }

    fn(spr);
    return ESP_OK;
}

} // namespace lgfx_dev

#endif // LGFX_DEVICE_INTERNAL_HPP
