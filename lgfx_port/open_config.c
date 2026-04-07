/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/open_config.c

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "defaultatoms.h" // ATOM_STR
#include "globalcontext.h"
#include "term.h"

#include "driver/spi_common.h"

#include "lgfx_port/lgfx_port_internal.h"

#define LGFX_ATOM(global, len_bytes, atom_text) \
    globalcontext_make_atom((global), ATOM_STR(len_bytes, atom_text))

static bool lgfx_term_to_int32_checked(term value, int32_t *out_value)
{
    if (!out_value || !term_is_integer(value)) {
        return false;
    }

    avm_int_t parsed = term_to_int(value);

    if (parsed < (avm_int_t) INT32_MIN || parsed > (avm_int_t) INT32_MAX) {
        return false;
    }

    *out_value = (int32_t) parsed;
    return true;
}

static bool lgfx_parse_bool_value(GlobalContext *global, term value, uint8_t *out_value)
{
    term true_atom = LGFX_ATOM(global, "\x04", "true");
    term false_atom = LGFX_ATOM(global, "\x05", "false");

    if (value == true_atom) {
        *out_value = 1u;
        return true;
    }

    if (value == false_atom) {
        *out_value = 0u;
        return true;
    }

    return false;
}

static bool lgfx_parse_panel_driver_value(
    GlobalContext *global,
    term value,
    lgfx_panel_driver_id_t *out_value)
{
    if (value == LGFX_ATOM(global, "\x07", "ili9488")) {
        *out_value = LGFX_PANEL_DRIVER_ID_ILI9488;
        return true;
    }

    if (value == LGFX_ATOM(global, "\x07", "ili9341")) {
        *out_value = LGFX_PANEL_DRIVER_ID_ILI9341;
        return true;
    }

    if (value == LGFX_ATOM(global, "\x09", "ili9341_2")) {
        *out_value = LGFX_PANEL_DRIVER_ID_ILI9341_2;
        return true;
    }

    if (value == LGFX_ATOM(global, "\x06", "st7789")) {
        *out_value = LGFX_PANEL_DRIVER_ID_ST7789;
        return true;
    }

    if (value == LGFX_ATOM(global, "\x08", "ili9342c")) {
        *out_value = LGFX_PANEL_DRIVER_ID_ILI9342C;
        return true;
    }

    return false;
}

static bool lgfx_parse_touch_driver_value(
    GlobalContext *global,
    term value,
    lgfx_touch_driver_id_t *out_value)
{
    if (value == LGFX_ATOM(global, "\x07", "xpt2046")) {
        *out_value = LGFX_TOUCH_DRIVER_ID_XPT2046;
        return true;
    }

    if (value == LGFX_ATOM(global, "\x07", "ft6336u")) {
        *out_value = LGFX_TOUCH_DRIVER_ID_FT6336U;
        return true;
    }

    return false;
}

static bool lgfx_parse_positive_u16_value(term value, uint16_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed <= 0 || parsed > (int32_t) UINT16_MAX) {
        return false;
    }

    *out_value = (uint16_t) parsed;
    return true;
}

/*
 * Current effective accepted range is 1..INT32_MAX.
 *
 * The storage slot is uint32_t, but the current term decoder path first routes
 * through int32_t (`term_to_int`), so values above INT32_MAX are not currently
 * representable here.
 */
static bool lgfx_parse_positive_u32_value(term value, uint32_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed <= 0) {
        return false;
    }

    *out_value = (uint32_t) parsed;
    return true;
}

static bool lgfx_parse_i32_value(term value, int32_t *out_value)
{
    return lgfx_term_to_int32_checked(value, out_value);
}

static bool lgfx_parse_rotation_value(term value, uint8_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed < 0 || parsed > 7) {
        return false;
    }

    *out_value = (uint8_t) parsed;
    return true;
}

static bool lgfx_parse_spi_mode_value(term value, uint8_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed < 0 || parsed > 3) {
        return false;
    }

    *out_value = (uint8_t) parsed;
    return true;
}

static bool lgfx_parse_gpio_value(term value, int32_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed < 0 || parsed > 255) {
        return false;
    }

    *out_value = parsed;
    return true;
}

static bool lgfx_parse_gpio_or_disabled_value(term value, int32_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed < -1 || parsed > 255) {
        return false;
    }

    *out_value = parsed;
    return true;
}

static bool lgfx_parse_spi_host_value(GlobalContext *global, term value, int32_t *out_value)
{
    if (value == LGFX_ATOM(global, "\x09", "spi2_host")) {
        *out_value = (int32_t) SPI2_HOST;
        return true;
    }

    if (value == LGFX_ATOM(global, "\x09", "spi3_host")) {
        *out_value = (int32_t) SPI3_HOST;
        return true;
    }

    return false;
}

static bool lgfx_parse_dma_channel_value(GlobalContext *global, term value, int32_t *out_value)
{
    int32_t parsed = 0;

    if (value == LGFX_ATOM(global, "\x0F", "spi_dma_ch_auto")) {
        *out_value = (int32_t) SPI_DMA_CH_AUTO;
        return true;
    }

    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed != 1 && parsed != 2) {
        return false;
    }

    *out_value = parsed;
    return true;
}

static bool lgfx_parse_i2c_port_value(term value, int32_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed < 0 || parsed > 1) {
        return false;
    }

    *out_value = parsed;
    return true;
}

static bool lgfx_parse_i2c_addr_value(term value, uint32_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_term_to_int32_checked(value, &parsed)) {
        return false;
    }

    if (parsed < 0 || parsed > 127) {
        return false;
    }

    *out_value = (uint32_t) parsed;
    return true;
}

static bool lgfx_parse_open_option_tuple(
    GlobalContext *global,
    term entry,
    lgfx_open_config_overrides_t *overrides,
    const char **error_detail)
{
    if (!term_is_tuple(entry) || term_get_tuple_arity(entry) != 2) {
        *error_detail = "each open_port option must be a {key, value} tuple";
        return false;
    }

    term key = term_get_tuple_element(entry, 0);
    term value = term_get_tuple_element(entry, 1);

    if (!term_is_atom(key)) {
        *error_detail = "open_port option key must be an atom";
        return false;
    }

    if (key == LGFX_ATOM(global, "\x0C", "panel_driver")) {
        if (!lgfx_parse_panel_driver_value(global, value, &overrides->panel_driver)) {
            *error_detail = "bad value for panel_driver (expected ili9488, ili9341, ili9341_2, st7789, or ili9342c)";
            return false;
        }
        overrides->has_panel_driver = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0C", "touch_driver")) {
        if (!lgfx_parse_touch_driver_value(global, value, &overrides->touch_driver)) {
            *error_detail = "bad value for touch_driver (expected xpt2046 or ft6336u)";
            return false;
        }
        overrides->has_touch_driver = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x05", "width")) {
        if (!lgfx_parse_positive_u16_value(value, &overrides->width)) {
            *error_detail = "bad value for width (expected positive integer in 1..65535)";
            return false;
        }
        overrides->has_width = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x06", "height")) {
        if (!lgfx_parse_positive_u16_value(value, &overrides->height)) {
            *error_detail = "bad value for height (expected positive integer in 1..65535)";
            return false;
        }
        overrides->has_height = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x08", "offset_x")) {
        if (!lgfx_parse_i32_value(value, &overrides->offset_x)) {
            *error_detail = "bad value for offset_x (expected signed 32-bit integer)";
            return false;
        }
        overrides->has_offset_x = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x08", "offset_y")) {
        if (!lgfx_parse_i32_value(value, &overrides->offset_y)) {
            *error_detail = "bad value for offset_y (expected signed 32-bit integer)";
            return false;
        }
        overrides->has_offset_y = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0F", "offset_rotation")) {
        if (!lgfx_parse_rotation_value(value, &overrides->offset_rotation)) {
            *error_detail = "bad value for offset_rotation (expected integer in 0..7)";
            return false;
        }
        overrides->has_offset_rotation = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x08", "readable")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->readable)) {
            *error_detail = "bad value for readable (expected boolean)";
            return false;
        }
        overrides->has_readable = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x06", "invert")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->invert)) {
            *error_detail = "bad value for invert (expected boolean)";
            return false;
        }
        overrides->has_invert = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x09", "rgb_order")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->rgb_order)) {
            *error_detail = "bad value for rgb_order (expected boolean)";
            return false;
        }
        overrides->has_rgb_order = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0A", "dlen_16bit")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->dlen_16bit)) {
            *error_detail = "bad value for dlen_16bit (expected boolean)";
            return false;
        }
        overrides->has_dlen_16bit = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0C", "lcd_spi_mode")) {
        if (!lgfx_parse_spi_mode_value(value, &overrides->lcd_spi_mode)) {
            *error_detail = "bad value for lcd_spi_mode (expected integer in 0..3)";
            return false;
        }
        overrides->has_lcd_spi_mode = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x11", "lcd_freq_write_hz")) {
        if (!lgfx_parse_positive_u32_value(value, &overrides->lcd_freq_write_hz)) {
            *error_detail = "bad value for lcd_freq_write_hz (expected positive integer in 1..2147483647)";
            return false;
        }
        overrides->has_lcd_freq_write_hz = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x10", "lcd_freq_read_hz")) {
        if (!lgfx_parse_positive_u32_value(value, &overrides->lcd_freq_read_hz)) {
            *error_detail = "bad value for lcd_freq_read_hz (expected positive integer in 1..2147483647)";
            return false;
        }
        overrides->has_lcd_freq_read_hz = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0F", "lcd_dma_channel")) {
        if (!lgfx_parse_dma_channel_value(global, value, &overrides->lcd_dma_channel)) {
            *error_detail = "bad value for lcd_dma_channel (expected spi_dma_ch_auto, 1, or 2)";
            return false;
        }
        overrides->has_lcd_dma_channel = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0D", "lcd_spi_3wire")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->lcd_spi_3wire)) {
            *error_detail = "bad value for lcd_spi_3wire (expected boolean)";
            return false;
        }
        overrides->has_lcd_spi_3wire = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0C", "lcd_use_lock")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->lcd_use_lock)) {
            *error_detail = "bad value for lcd_use_lock (expected boolean)";
            return false;
        }
        overrides->has_lcd_use_lock = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0E", "lcd_bus_shared")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->lcd_bus_shared)) {
            *error_detail = "bad value for lcd_bus_shared (expected boolean)";
            return false;
        }
        overrides->has_lcd_bus_shared = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0D", "spi_sclk_gpio")) {
        if (!lgfx_parse_gpio_value(value, &overrides->spi_sclk_gpio)) {
            *error_detail = "bad value for spi_sclk_gpio (expected GPIO integer in 0..255)";
            return false;
        }
        overrides->has_spi_sclk_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0D", "spi_mosi_gpio")) {
        if (!lgfx_parse_gpio_value(value, &overrides->spi_mosi_gpio)) {
            *error_detail = "bad value for spi_mosi_gpio (expected GPIO integer in 0..255)";
            return false;
        }
        overrides->has_spi_mosi_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0D", "spi_miso_gpio")) {
        if (!lgfx_parse_gpio_or_disabled_value(value, &overrides->spi_miso_gpio)) {
            *error_detail = "bad value for spi_miso_gpio (expected GPIO integer in -1..255; -1 disables)";
            return false;
        }
        overrides->has_spi_miso_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0C", "lcd_spi_host")) {
        if (!lgfx_parse_spi_host_value(global, value, &overrides->lcd_spi_host)) {
            *error_detail = "bad value for lcd_spi_host (expected spi2_host or spi3_host)";
            return false;
        }
        overrides->has_lcd_spi_host = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0B", "lcd_cs_gpio")) {
        if (!lgfx_parse_gpio_value(value, &overrides->lcd_cs_gpio)) {
            *error_detail = "bad value for lcd_cs_gpio (expected GPIO integer in 0..255)";
            return false;
        }
        overrides->has_lcd_cs_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0B", "lcd_dc_gpio")) {
        if (!lgfx_parse_gpio_value(value, &overrides->lcd_dc_gpio)) {
            *error_detail = "bad value for lcd_dc_gpio (expected GPIO integer in 0..255)";
            return false;
        }
        overrides->has_lcd_dc_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0C", "lcd_rst_gpio")) {
        if (!lgfx_parse_gpio_or_disabled_value(value, &overrides->lcd_rst_gpio)) {
            *error_detail = "bad value for lcd_rst_gpio (expected GPIO integer in -1..255; -1 disables)";
            return false;
        }
        overrides->has_lcd_rst_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0C", "lcd_pin_busy")) {
        if (!lgfx_parse_gpio_or_disabled_value(value, &overrides->lcd_pin_busy)) {
            *error_detail = "bad value for lcd_pin_busy (expected GPIO integer in -1..255; -1 disables)";
            return false;
        }
        overrides->has_lcd_pin_busy = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0D", "touch_cs_gpio")) {
        if (!lgfx_parse_gpio_or_disabled_value(value, &overrides->touch_cs_gpio)) {
            *error_detail = "bad value for touch_cs_gpio (expected GPIO integer in -1..255; -1 disables)";
            return false;
        }
        overrides->has_touch_cs_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0E", "touch_irq_gpio")) {
        if (!lgfx_parse_gpio_or_disabled_value(value, &overrides->touch_irq_gpio)) {
            *error_detail = "bad value for touch_irq_gpio (expected GPIO integer in -1..255; -1 disables)";
            return false;
        }
        overrides->has_touch_irq_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0E", "touch_spi_host")) {
        if (!lgfx_parse_spi_host_value(global, value, &overrides->touch_spi_host)) {
            *error_detail = "bad value for touch_spi_host (expected spi2_host or spi3_host)";
            return false;
        }
        overrides->has_touch_spi_host = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x11", "touch_spi_freq_hz")) {
        if (!lgfx_parse_positive_u32_value(value, &overrides->touch_spi_freq_hz)) {
            *error_detail = "bad value for touch_spi_freq_hz (expected positive integer in 1..2147483647)";
            return false;
        }
        overrides->has_touch_spi_freq_hz = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0E", "touch_i2c_port")) {
        if (!lgfx_parse_i2c_port_value(value, &overrides->touch_i2c_port)) {
            *error_detail = "bad value for touch_i2c_port (expected integer 0 or 1)";
            return false;
        }
        overrides->has_touch_i2c_port = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0E", "touch_sda_gpio")) {
        if (!lgfx_parse_gpio_value(value, &overrides->touch_sda_gpio)) {
            *error_detail = "bad value for touch_sda_gpio (expected GPIO integer in 0..255)";
            return false;
        }
        overrides->has_touch_sda_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0E", "touch_scl_gpio")) {
        if (!lgfx_parse_gpio_value(value, &overrides->touch_scl_gpio)) {
            *error_detail = "bad value for touch_scl_gpio (expected GPIO integer in 0..255)";
            return false;
        }
        overrides->has_touch_scl_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0E", "touch_i2c_addr")) {
        if (!lgfx_parse_i2c_addr_value(value, &overrides->touch_i2c_addr)) {
            *error_detail = "bad value for touch_i2c_addr (expected integer in 0..127)";
            return false;
        }
        overrides->has_touch_i2c_addr = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x0E", "touch_rst_gpio")) {
        if (!lgfx_parse_gpio_or_disabled_value(value, &overrides->touch_rst_gpio)) {
            *error_detail = "bad value for touch_rst_gpio (expected GPIO integer in -1..255; -1 disables)";
            return false;
        }
        overrides->has_touch_rst_gpio = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x15", "touch_offset_rotation")) {
        if (!lgfx_parse_rotation_value(value, &overrides->touch_offset_rotation)) {
            *error_detail = "bad value for touch_offset_rotation (expected integer in 0..7)";
            return false;
        }
        overrides->has_touch_offset_rotation = 1u;
        return true;
    }

    if (key == LGFX_ATOM(global, "\x10", "touch_bus_shared")) {
        if (!lgfx_parse_bool_value(global, value, &overrides->touch_bus_shared)) {
            *error_detail = "bad value for touch_bus_shared (expected boolean)";
            return false;
        }
        overrides->has_touch_bus_shared = 1u;
        return true;
    }

    *error_detail = "unknown open_port option key";
    return false;
}

bool lgfx_parse_open_config_opts(
    GlobalContext *global,
    term opts,
    lgfx_open_config_overrides_t *out_overrides,
    const char **error_detail)
{
    *out_overrides = (lgfx_open_config_overrides_t) { 0 };
    *error_detail = NULL;

    if (term_is_nil(opts)) {
        return true;
    }

    term cursor = opts;
    while (!term_is_nil(cursor)) {
        if (!term_is_list(cursor)) {
            *error_detail = "open_port opts must be a proper list";
            return false;
        }

        term entry = term_get_list_head(cursor);
        cursor = term_get_list_tail(cursor);

        if (!lgfx_parse_open_option_tuple(global, entry, out_overrides, error_detail)) {
            return false;
        }
    }

    return true;
}
