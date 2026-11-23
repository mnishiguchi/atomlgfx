/*
 * SPDX-FileCopyrightText: 2020 lovyan03 (https://github.com/lovyan03)
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 * SPDX-License-Identifier: BSD-2-Clause
 */

// Native benchmark based on LovyanGFX examples/Sprite/MovingIcons.
// Kept separate from the general-purpose drawing API.
#include <atomic>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "LovyanGFX.hpp"
#include "device_internal.hpp"
#include "atom_lgfx/moving_icons_demo.h"

namespace {
constexpr size_t kIconPixels = 32u * 32u;
constexpr size_t kIconBytes = kIconPixels * sizeof(uint16_t);
constexpr uint8_t kMaxObjects = 50u;
constexpr uint32_t kStopTimeoutMs = 5000u;

struct Object {
    int16_t x = 0;
    int16_t y = 0;
    int16_t dx = 0;
    int16_t dy = 0;
    uint8_t image = 0;
    float angle = 0.0f;
    float zoom = 1.0f;
    float da = 1.0f;
    float dz = 0.01f;

    void advance(int width, int height) {
        angle += da;
        x += dx;
        y += dy;
        zoom += dz;
        if (x < 0 || x >= width) {
            x = x < 0 ? 0 : width - 1;
            dx = -dx;
        }
        if (y < 0 || y >= height) {
            y = y < 0 ? 0 : height - 1;
            dy = -dy;
        }
        if (zoom < 0.5f || zoom >= 2.0f) {
            zoom = zoom < 0.5f ? 0.5f : 2.0f;
            dz = -dz;
        }
    }
};

static std::atomic<bool> active{false};
static std::atomic<bool> stopping{false};
static std::atomic<bool> stop_requested{false};
static std::atomic<uint32_t> frame_counter{0};
static std::atomic<uint32_t> last_frame_time_us{0};
static std::atomic<uint32_t> start_time_ms{0};
static SemaphoreHandle_t stopped_sem = nullptr;
static lgfx::LGFX_Sprite strips[2];
static lgfx::LGFX_Sprite icons[3];
static Object objects[kMaxObjects];
static uint8_t object_count = 0;
static int lcd_width = 0;
static int lcd_height = 0;
static int strip_height = 0;
static uint32_t last_fps = 0;
static uint32_t rng_state = 12345;

static uint32_t next_random() {
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state;
}

static void release_sprites() {
    for (auto &strip : strips) strip.deleteSprite();
    for (auto &icon : icons) icon.deleteSprite();
}

static void initialize_objects() {
    rng_state = 12345;
    for (unsigned i = 0; i < object_count; ++i) {
        Object &obj = objects[i];
        obj.image = i % 3;
        obj.x = next_random() % lcd_width;
        obj.y = next_random() % lcd_height;
        obj.dx = ((next_random() & 3u) + 1u) * ((i & 1u) ? 1 : -1);
        obj.dy = ((next_random() & 3u) + 1u) * ((i & 2u) ? 1 : -1);
        obj.da = ((next_random() & 3u) + 1u) * ((i & 2u) ? 1.0f : -1.0f);
        obj.angle = 0;
        obj.zoom = static_cast<float>((next_random() % 10u) + 10u) / 10.0f;
        obj.dz = static_cast<float>((next_random() % 10u) + 1u) / 100.0f;
    }
}

static bool setup_sprites(const uint8_t *data[3]) {
    for (int i = 0; i < 3; ++i) {
        icons[i].setColorDepth(16);
        if (!icons[i].createSprite(32, 32)) return false;
        icons[i].setSwapBytes(true);
        // AtomVM binaries need not be aligned to uint16_t boundaries.
        uint16_t aligned_pixels[kIconPixels];
        memcpy(aligned_pixels, data[i], kIconBytes);
        icons[i].pushImage(0, 0, 32, 32, aligned_pixels);
        icons[i].setSwapBytes(false);
        icons[i].setPivot(16, 16);
    }

    // Match the upstream adaptive two-strip strategy. Smaller strips also
    // allow constrained boards to report a clean out-of-memory error.
    for (unsigned divisor = 2; divisor <= static_cast<unsigned>(lcd_height); ++divisor) {
        const int h = (lcd_height + static_cast<int>(divisor) - 1) / static_cast<int>(divisor);
        if (h < 8) break;
        bool ok = true;
        for (auto &strip : strips) {
            strip.setColorDepth(16);
            if (!strip.createSprite(lcd_width, h)) ok = false;
        }
        if (ok) {
            strip_height = h;
            return true;
        }
        for (auto &strip : strips) strip.deleteSprite();
    }
    return false;
}

static void task_main(void *) {
    uint32_t interval_frames = 0;
    int64_t previous_log_us = esp_timer_get_time();
    uint8_t flip = 0;

    while (!stop_requested.load()) {
        const int64_t frame_started_us = esp_timer_get_time();
        {
            lgfx_dev::ScopedLcdLock lock;
            lock.lock();
            auto *lcd = lock.is_locked() ? lgfx_dev::lcd_device_locked() : nullptr;
            if (!lcd || lcd->width() != lcd_width || lcd->height() != lcd_height) {
                break;
            }
            for (unsigned i = 0; i < object_count; ++i) {
                objects[i].advance(lcd_width, lcd_height);
            }

            lcd->startWrite();
            for (int y = 0; y < lcd_height; y += strip_height) {
                flip = !flip;
                auto &strip = strips[flip];
                strip.clear();
                for (unsigned i = 0; i < object_count; ++i) {
                    const auto &obj = objects[i];
                    icons[obj.image].pushRotateZoom(&strip, obj.x, obj.y - y,
                        obj.angle, obj.zoom, obj.zoom, 0);
                }
                if (y == 0) {
                    strip.setCursor(0, 0);
                    strip.setTextColor(0xffff);
                    strip.printf("obj:%u fps:%lu", (unsigned)object_count,
                                 (unsigned long)last_fps);
                }
                strip.pushSprite(lcd, 0, y);
                // LovyanGFX 1.3 may DMA from PSRAM asynchronously while
                // startWrite is open. Do not overwrite the source strip yet.
                lcd->waitDMA();
            }
            lcd->endWrite();
            lcd->display();
        }
        last_frame_time_us.store(static_cast<uint32_t>(esp_timer_get_time() - frame_started_us));
        frame_counter.fetch_add(1);
        interval_frames++;
        const int64_t now_us = esp_timer_get_time();
        if (now_us - previous_log_us >= 1000000) {
            last_fps = static_cast<uint32_t>(interval_frames * 1000000LL / (now_us - previous_log_us));
            interval_frames = 0;
            previous_log_us = now_us;
        }
        // Keep the AtomVM scheduler and other ESP-IDF tasks responsive.
        vTaskDelay(1);
    }

    {
        lgfx_dev::ScopedLcdLock lock;
        lock.lock();
        release_sprites();
    }
    // Release the waiter only after all display buffers have been destroyed.
    xSemaphoreGive(stopped_sem);
    vTaskDelete(nullptr);
}
} // namespace

extern "C" esp_err_t lgfx_moving_icons_start(const uint8_t *info, size_t info_len,
                                                const uint8_t *alert, size_t alert_len,
                                                const uint8_t *close_icon, size_t close_len,
                                                uint8_t count) {
    if (!info || !alert || !close_icon || info_len != kIconBytes ||
        alert_len != kIconBytes || close_len != kIconBytes || count == 0 || count > kMaxObjects) {
        return ESP_ERR_INVALID_ARG;
    }

    bool expected = false;
    if (!active.compare_exchange_strong(expected, true)) return ESP_ERR_INVALID_STATE;
    stopping.store(false);
    stop_requested.store(false);
    frame_counter.store(0);
    last_frame_time_us.store(0);
    start_time_ms.store(0);
    last_fps = 0;
    object_count = count;

    stopped_sem = xSemaphoreCreateBinary();
    if (!stopped_sem) {
        active.store(false);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ESP_OK;
    bool sprites_ready = false;
    {
        lgfx_dev::ScopedLcdLock lock;
        err = lgfx_dev::lock_ready(lock);
        auto *lcd = err == ESP_OK ? lgfx_dev::lcd_device_locked() : nullptr;
        if (!lcd || err != ESP_OK) err = ESP_ERR_INVALID_STATE;
        if (err == ESP_OK) {
            lcd_width = lcd->width();
            lcd_height = lcd->height();
            if (lcd_width <= 0 || lcd_height <= 0 || lcd_width > INT16_MAX || lcd_height > INT16_MAX) {
                err = ESP_ERR_INVALID_ARG;
            }
        }
        if (err == ESP_OK) {
            const uint8_t *data[3] = {info, alert, close_icon};
            if (!setup_sprites(data)) err = ESP_ERR_NO_MEM;
            else {
                initialize_objects();
                sprites_ready = true;
            }
        }
        if (err != ESP_OK) release_sprites();
    }

    if (err == ESP_OK) {
        start_time_ms.store(static_cast<uint32_t>(esp_timer_get_time() / 1000));
        if (xTaskCreate(task_main, "lgfx_moving_icons", 8192, nullptr,
                        tskIDLE_PRIORITY + 1, nullptr) != pdPASS) {
            err = ESP_ERR_NO_MEM;
        }
    }
    if (err != ESP_OK) {
        if (sprites_ready) {
            lgfx_dev::ScopedLcdLock lock;
            lock.lock();
            release_sprites();
        }
        vSemaphoreDelete(stopped_sem);
        stopped_sem = nullptr;
        active.store(false);
    }
    return err;
}

extern "C" esp_err_t lgfx_moving_icons_stop(void) {
    if (!active.load()) return ESP_OK;
    if (stopping.exchange(true)) return ESP_ERR_INVALID_STATE;
    stop_requested.store(true);
    if (xSemaphoreTake(stopped_sem, pdMS_TO_TICKS(kStopTimeoutMs)) != pdTRUE) {
        stopping.store(false);
        return ESP_ERR_TIMEOUT;
    }
    vSemaphoreDelete(stopped_sem);
    stopped_sem = nullptr;
    active.store(false);
    stopping.store(false);
    return ESP_OK;
}

extern "C" esp_err_t lgfx_moving_icons_stats(uint32_t *frames, uint32_t *elapsed_ms,
                                                uint32_t *last_frame_us) {
    if (!frames || !elapsed_ms || !last_frame_us) return ESP_ERR_INVALID_ARG;
    if (!active.load()) return ESP_ERR_INVALID_STATE;
    *frames = frame_counter.load();
    *elapsed_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000) - start_time_ms.load();
    *last_frame_us = last_frame_time_us.load();
    return ESP_OK;
}
