// src/lgfx_device_transactions.cpp
// LCD write-path and transaction helpers (LCD-only by protocol semantics).

#include "lgfx_device.h"
#include "lgfx_device_internal.hpp"

extern "C" esp_err_t lgfx_device_begin_transaction(void)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->beginTransaction(); });
}

extern "C" esp_err_t lgfx_device_end_transaction(void)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->endTransaction(); });
}

extern "C" esp_err_t lgfx_device_start_write(void)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->startWrite(); });
}

extern "C" esp_err_t lgfx_device_end_write(void)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->endWrite(); });
}

extern "C" esp_err_t lgfx_device_write_pixel(int16_t x, int16_t y, uint16_t rgb565)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->writePixel(x, y, rgb565); });
}

extern "C" esp_err_t lgfx_device_write_fast_vline(int16_t x, int16_t y, uint16_t h, uint16_t rgb565)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->writeFastVLine(x, y, h, rgb565); });
}

extern "C" esp_err_t lgfx_device_write_fast_hline(int16_t x, int16_t y, uint16_t w, uint16_t rgb565)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->writeFastHLine(x, y, w, rgb565); });
}

extern "C" esp_err_t lgfx_device_write_fill_rect(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t rgb565)
{
    return lgfx_dev::with_lcd([&](lgfx::LGFX_Device *d) { d->writeFillRect(x, y, w, h, rgb565); });
}
