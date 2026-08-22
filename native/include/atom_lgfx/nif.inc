/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// native/include/atom_lgfx/nif.inc
//
// AtomVM NIF bridge for the direct LGFX Elixir API.
//
// Keep this layer intentionally thin:
// - decode AtomVM terms
// - call the existing lgfx_device_* C ABI
// - encode a small result
//
// Both the direct `LGFX` functions and the generic `AtomLGFX` dispatcher use
// the same LovyanGFX device implementation.

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "context.h"
#include "defaultatoms.h"
#include "memory.h"
#include "nifs.h"
#include "portnifloader.h"
#include "term.h"

#include "device.h"
#include "atom_lgfx/render_batch.h"


static bool lgfx_term_to_u32(term value, uint32_t *out)
{
    if (!out || !term_is_integer(value)) {
        return false;
    }

    avm_int_t decoded = term_to_int(value);
    if (decoded < 0 || (uint64_t) decoded > UINT32_MAX) {
        return false;
    }

    *out = (uint32_t) decoded;
    return true;
}

static bool lgfx_term_to_i32(term value, int32_t *out)
{
    if (!out || !term_is_integer(value)) {
        return false;
    }

    avm_int_t decoded = term_to_int(value);
    if (decoded < (avm_int_t) INT32_MIN || decoded > (avm_int_t) INT32_MAX) {
        return false;
    }

    *out = (int32_t) decoded;
    return true;
}

static bool lgfx_term_to_i16(term value, int16_t *out)
{
    int32_t decoded = 0;
    if (!out || !lgfx_term_to_i32(value, &decoded) || decoded < INT16_MIN || decoded > INT16_MAX) {
        return false;
    }

    *out = (int16_t) decoded;
    return true;
}

static bool lgfx_term_to_u16(term value, uint16_t *out)
{
    uint32_t decoded = 0;
    if (!out || !lgfx_term_to_u32(value, &decoded) || decoded > UINT16_MAX) {
        return false;
    }

    *out = (uint16_t) decoded;
    return true;
}

static bool lgfx_term_to_f32(term value, float *out)
{
    if (!out) {
        return false;
    }

    avm_float_t decoded = 0;

    if (term_is_float(value)) {
        decoded = term_to_float(value);
    } else if (term_is_integer(value)) {
        decoded = (avm_float_t) term_to_int(value);
    } else {
        return false;
    }

    if (!isfinite((double) decoded)
        || decoded < -(avm_float_t) FLT_MAX
        || decoded > (avm_float_t) FLT_MAX) {
        return false;
    }

    *out = (float) decoded;
    return true;
}

static bool lgfx_term_to_color565(term value, uint16_t *out)
{
    return lgfx_term_to_u16(value, out);
}

static uint8_t lgfx_nif_owner_token;
static bool lgfx_nif_initialized = false;
static const lgfx_open_config_overrides_t lgfx_nif_default_open_config = { 0 };

static term lgfx_nif_error(Context *ctx, term reason)
{
    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }

    term tuple = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(tuple, 0, term_from_atom_index(ERROR_ATOM_INDEX));
    term_put_tuple_element(tuple, 1, reason);
    return tuple;
}

static term lgfx_nif_badarg(Context *ctx)
{
    return lgfx_nif_error(ctx, term_from_atom_index(BADARG_ATOM_INDEX));
}

static term lgfx_nif_esp_result(Context *ctx, esp_err_t err)
{
    if (err == ESP_OK) {
        return term_from_atom_index(OK_ATOM_INDEX);
    }

    return lgfx_nif_error(ctx, term_from_int((avm_int_t) err));
}

static bool lgfx_nif_term_to_u8(term value, uint8_t *out)
{
    uint32_t decoded = 0;

    if (!out || !lgfx_term_to_u32(value, &decoded) || decoded > UINT8_MAX) {
        return false;
    }

    *out = (uint8_t) decoded;
    return true;
}

static bool lgfx_nif_term_to_binary(term value, const uint8_t **out_bytes, size_t *out_len)
{
    if (!out_bytes || !out_len || !term_is_binary(value)) {
        return false;
    }

    *out_bytes = (const uint8_t *) term_binary_data(value);
    *out_len = (size_t) term_binary_size(value);
    return true;
}

static term lgfx_nif_init_0(Context *ctx, int argc, term argv[])
{
    (void) argv;

    if (argc != 0) {
        return lgfx_nif_badarg(ctx);
    }

    esp_err_t err = lgfx_device_init_with_open_config(
        &lgfx_nif_default_open_config,
        (const void *) &lgfx_nif_owner_token);

    if (err == ESP_OK) {
        lgfx_nif_initialized = true;
    }

    return lgfx_nif_esp_result(ctx, err);
}

static term lgfx_nif_close_0(Context *ctx, int argc, term argv[])
{
    (void) argv;

    if (argc != 0) {
        return lgfx_nif_badarg(ctx);
    }

    esp_err_t err =
        lgfx_device_close_for_owner((const void *) &lgfx_nif_owner_token);

    if (err == ESP_OK) {
        lgfx_nif_initialized = false;
    }

    return lgfx_nif_esp_result(ctx, err);
}

static term lgfx_nif_width_0(Context *ctx, int argc, term argv[])
{
    (void) argv;

    if (argc != 0) {
        return lgfx_nif_badarg(ctx);
    }

    uint16_t width = 0;
    uint16_t height = 0;
    esp_err_t err = lgfx_device_get_target_dims(LGFX_DEVICE_TARGET_LCD, &width, &height);

    if (err != ESP_OK) {
        return lgfx_nif_esp_result(ctx, err);
    }

    return term_from_int((avm_int_t) width);
}

static term lgfx_nif_height_0(Context *ctx, int argc, term argv[])
{
    (void) argv;

    if (argc != 0) {
        return lgfx_nif_badarg(ctx);
    }

    uint16_t width = 0;
    uint16_t height = 0;
    esp_err_t err = lgfx_device_get_target_dims(LGFX_DEVICE_TARGET_LCD, &width, &height);

    if (err != ESP_OK) {
        return lgfx_nif_esp_result(ctx, err);
    }

    return term_from_int((avm_int_t) height);
}

static term lgfx_nif_start_write_0(Context *ctx, int argc, term argv[])
{
    (void) argv;

    if (argc != 0) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(ctx, lgfx_device_start_write());
}

static term lgfx_nif_end_write_0(Context *ctx, int argc, term argv[])
{
    (void) argv;

    if (argc != 0) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(ctx, lgfx_device_end_write());
}

static term lgfx_nif_display_0(Context *ctx, int argc, term argv[])
{
    (void) argv;

    if (argc != 0) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(ctx, lgfx_device_display());
}

static term lgfx_nif_set_rotation_1(Context *ctx, int argc, term argv[])
{
    uint8_t rotation = 0;

    if (argc != 1 || !lgfx_nif_term_to_u8(argv[0], &rotation) || rotation > 7u) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(ctx, lgfx_device_set_rotation(rotation));
}

static term lgfx_nif_set_brightness_1(Context *ctx, int argc, term argv[])
{
    uint8_t brightness = 0;

    if (argc != 1 || !lgfx_nif_term_to_u8(argv[0], &brightness)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(ctx, lgfx_device_set_brightness(brightness));
}

static term lgfx_nif_fill_screen_1(Context *ctx, int argc, term argv[])
{
    uint16_t color = 0;

    if (argc != 1 || !lgfx_term_to_color565(argv[0], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_fill_screen(LGFX_DEVICE_TARGET_LCD, false, (uint32_t) color));
}

static term lgfx_nif_clear_1(Context *ctx, int argc, term argv[])
{
    uint16_t color = 0;

    if (argc != 1 || !lgfx_term_to_color565(argv[0], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_clear(LGFX_DEVICE_TARGET_LCD, false, (uint32_t) color));
}

static term lgfx_nif_draw_pixel_3(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t color = 0;

    if (argc != 3
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_color565(argv[2], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_draw_pixel(LGFX_DEVICE_TARGET_LCD, x, y, false, (uint32_t) color));
}

static term lgfx_nif_draw_fast_vline_4(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t height = 0;
    uint16_t color = 0;

    if (argc != 4
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &height)
        || !lgfx_term_to_color565(argv[3], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_draw_fast_vline(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            height,
            false,
            (uint32_t) color));
}

static term lgfx_nif_draw_fast_hline_4(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t width = 0;
    uint16_t color = 0;

    if (argc != 4
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &width)
        || !lgfx_term_to_color565(argv[3], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_draw_fast_hline(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            width,
            false,
            (uint32_t) color));
}

static term lgfx_nif_draw_line_5(Context *ctx, int argc, term argv[])
{
    int16_t x0 = 0;
    int16_t y0 = 0;
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t color = 0;

    if (argc != 5
        || !lgfx_term_to_i16(argv[0], &x0)
        || !lgfx_term_to_i16(argv[1], &y0)
        || !lgfx_term_to_i16(argv[2], &x1)
        || !lgfx_term_to_i16(argv[3], &y1)
        || !lgfx_term_to_color565(argv[4], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_draw_line(
            LGFX_DEVICE_TARGET_LCD,
            x0,
            y0,
            x1,
            y1,
            false,
            (uint32_t) color));
}

static term lgfx_nif_draw_rect_5(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t color = 0;

    if (argc != 5
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &width)
        || !lgfx_term_to_u16(argv[3], &height)
        || !lgfx_term_to_color565(argv[4], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_draw_rect(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            width,
            height,
            false,
            (uint32_t) color));
}

static term lgfx_nif_fill_rect_5(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t color = 0;

    if (argc != 5
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &width)
        || !lgfx_term_to_u16(argv[3], &height)
        || !lgfx_term_to_color565(argv[4], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_fill_rect(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            width,
            height,
            false,
            (uint32_t) color));
}

static term lgfx_nif_draw_round_rect_6(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t radius = 0;
    uint16_t color = 0;

    if (argc != 6
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &width)
        || !lgfx_term_to_u16(argv[3], &height)
        || !lgfx_term_to_u16(argv[4], &radius)
        || !lgfx_term_to_color565(argv[5], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_draw_round_rect(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            width,
            height,
            radius,
            false,
            (uint32_t) color));
}

static term lgfx_nif_fill_round_rect_6(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t radius = 0;
    uint16_t color = 0;

    if (argc != 6
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &width)
        || !lgfx_term_to_u16(argv[3], &height)
        || !lgfx_term_to_u16(argv[4], &radius)
        || !lgfx_term_to_color565(argv[5], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_fill_round_rect(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            width,
            height,
            radius,
            false,
            (uint32_t) color));
}

static term lgfx_nif_draw_circle_4(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t radius = 0;
    uint16_t color = 0;

    if (argc != 4
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &radius)
        || !lgfx_term_to_color565(argv[3], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_draw_circle(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            radius,
            false,
            (uint32_t) color));
}

static term lgfx_nif_fill_circle_4(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t radius = 0;
    uint16_t color = 0;

    if (argc != 4
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &radius)
        || !lgfx_term_to_color565(argv[3], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_fill_circle(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            radius,
            false,
            (uint32_t) color));
}

static term lgfx_nif_draw_ellipse_5(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t radius_x = 0;
    uint16_t radius_y = 0;
    uint16_t color = 0;

    if (argc != 5
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &radius_x)
        || !lgfx_term_to_u16(argv[3], &radius_y)
        || !lgfx_term_to_color565(argv[4], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_draw_ellipse(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            radius_x,
            radius_y,
            false,
            (uint32_t) color));
}

static term lgfx_nif_fill_ellipse_5(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t radius_x = 0;
    uint16_t radius_y = 0;
    uint16_t color = 0;

    if (argc != 5
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &radius_x)
        || !lgfx_term_to_u16(argv[3], &radius_y)
        || !lgfx_term_to_color565(argv[4], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_fill_ellipse(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            radius_x,
            radius_y,
            false,
            (uint32_t) color));
}

static term lgfx_nif_draw_arc_7(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t radius0 = 0;
    uint16_t radius1 = 0;
    float angle0 = 0.0f;
    float angle1 = 0.0f;
    uint16_t color = 0;

    if (argc != 7
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &radius0)
        || !lgfx_term_to_u16(argv[3], &radius1)
        || !lgfx_term_to_f32(argv[4], &angle0)
        || !lgfx_term_to_f32(argv[5], &angle1)
        || !lgfx_term_to_color565(argv[6], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_draw_arc(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            radius0,
            radius1,
            angle0,
            angle1,
            false,
            (uint32_t) color));
}

static term lgfx_nif_fill_arc_7(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t radius0 = 0;
    uint16_t radius1 = 0;
    float angle0 = 0.0f;
    float angle1 = 0.0f;
    uint16_t color = 0;

    if (argc != 7
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &radius0)
        || !lgfx_term_to_u16(argv[3], &radius1)
        || !lgfx_term_to_f32(argv[4], &angle0)
        || !lgfx_term_to_f32(argv[5], &angle1)
        || !lgfx_term_to_color565(argv[6], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_fill_arc(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            radius0,
            radius1,
            angle0,
            angle1,
            false,
            (uint32_t) color));
}

static term lgfx_nif_draw_triangle_7(Context *ctx, int argc, term argv[])
{
    int16_t x0 = 0;
    int16_t y0 = 0;
    int16_t x1 = 0;
    int16_t y1 = 0;
    int16_t x2 = 0;
    int16_t y2 = 0;
    uint16_t color = 0;

    if (argc != 7
        || !lgfx_term_to_i16(argv[0], &x0)
        || !lgfx_term_to_i16(argv[1], &y0)
        || !lgfx_term_to_i16(argv[2], &x1)
        || !lgfx_term_to_i16(argv[3], &y1)
        || !lgfx_term_to_i16(argv[4], &x2)
        || !lgfx_term_to_i16(argv[5], &y2)
        || !lgfx_term_to_color565(argv[6], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_draw_triangle(
            LGFX_DEVICE_TARGET_LCD,
            x0,
            y0,
            x1,
            y1,
            x2,
            y2,
            false,
            (uint32_t) color));
}

static term lgfx_nif_fill_triangle_7(Context *ctx, int argc, term argv[])
{
    int16_t x0 = 0;
    int16_t y0 = 0;
    int16_t x1 = 0;
    int16_t y1 = 0;
    int16_t x2 = 0;
    int16_t y2 = 0;
    uint16_t color = 0;

    if (argc != 7
        || !lgfx_term_to_i16(argv[0], &x0)
        || !lgfx_term_to_i16(argv[1], &y0)
        || !lgfx_term_to_i16(argv[2], &x1)
        || !lgfx_term_to_i16(argv[3], &y1)
        || !lgfx_term_to_i16(argv[4], &x2)
        || !lgfx_term_to_i16(argv[5], &y2)
        || !lgfx_term_to_color565(argv[6], &color)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_fill_triangle(
            LGFX_DEVICE_TARGET_LCD,
            x0,
            y0,
            x1,
            y1,
            x2,
            y2,
            false,
            (uint32_t) color));
}

static term lgfx_nif_set_cursor_2(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;

    if (argc != 2
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_set_cursor(LGFX_DEVICE_TARGET_LCD, x, y));
}

static term lgfx_nif_set_text_size_1(Context *ctx, int argc, term argv[])
{
    float scale = 0.0f;

    if (argc != 1 || !lgfx_term_to_f32(argv[0], &scale) || scale <= 0.0f) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_set_text_size(LGFX_DEVICE_TARGET_LCD, scale));
}

static term lgfx_nif_set_text_datum_1(Context *ctx, int argc, term argv[])
{
    uint8_t datum = 0;

    if (argc != 1 || !lgfx_nif_term_to_u8(argv[0], &datum)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_set_text_datum(LGFX_DEVICE_TARGET_LCD, datum));
}

static term lgfx_nif_set_text_color_1(Context *ctx, int argc, term argv[])
{
    uint16_t foreground = 0;

    if (argc != 1 || !lgfx_term_to_color565(argv[0], &foreground)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_set_text_color(
            LGFX_DEVICE_TARGET_LCD,
            false,
            (uint32_t) foreground,
            false,
            false,
            0u));
}

static term lgfx_nif_set_text_color_2(Context *ctx, int argc, term argv[])
{
    uint16_t foreground = 0;
    uint16_t background = 0;

    if (argc != 2
        || !lgfx_term_to_color565(argv[0], &foreground)
        || !lgfx_term_to_color565(argv[1], &background)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_set_text_color(
            LGFX_DEVICE_TARGET_LCD,
            false,
            (uint32_t) foreground,
            true,
            false,
            (uint32_t) background));
}

static term lgfx_nif_draw_string_3(Context *ctx, int argc, term argv[])
{
    const uint8_t *text = NULL;
    size_t text_len = 0;
    int16_t x = 0;
    int16_t y = 0;

    if (argc != 3
        || !lgfx_nif_term_to_binary(argv[0], &text, &text_len)
        || !lgfx_term_to_i16(argv[1], &x)
        || !lgfx_term_to_i16(argv[2], &y)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_draw_string(LGFX_DEVICE_TARGET_LCD, x, y, text, text_len));
}

static term lgfx_nif_print_1(Context *ctx, int argc, term argv[])
{
    const uint8_t *text = NULL;
    size_t text_len = 0;

    if (argc != 1 || !lgfx_nif_term_to_binary(argv[0], &text, &text_len)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_print(LGFX_DEVICE_TARGET_LCD, text, text_len));
}

static term lgfx_nif_println_1(Context *ctx, int argc, term argv[])
{
    const uint8_t *text = NULL;
    size_t text_len = 0;

    if (argc != 1 || !lgfx_nif_term_to_binary(argv[0], &text, &text_len)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_println(LGFX_DEVICE_TARGET_LCD, text, text_len));
}

static term lgfx_nif_push_image_5(Context *ctx, int argc, term argv[])
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    const uint8_t *pixels = NULL;
    size_t pixels_len = 0;

    if (argc != 5
        || !lgfx_term_to_i16(argv[0], &x)
        || !lgfx_term_to_i16(argv[1], &y)
        || !lgfx_term_to_u16(argv[2], &width)
        || !lgfx_term_to_u16(argv[3], &height)
        || !lgfx_nif_term_to_binary(argv[4], &pixels, &pixels_len)) {
        return lgfx_nif_badarg(ctx);
    }

    return lgfx_nif_esp_result(
        ctx,
        lgfx_device_push_image_rgb565_strided(
            LGFX_DEVICE_TARGET_LCD,
            x,
            y,
            width,
            height,
            0u,
            pixels,
            pixels_len));
}

static term lgfx_nif_batch_2(Context *ctx, int argc, term argv[])
{
    uint8_t target = 0;
    const uint8_t *command_bytes = NULL;
    size_t command_len = 0;

    if (argc != 2
        || !lgfx_nif_term_to_u8(argv[0], &target)
        || !lgfx_nif_term_to_binary(argv[1], &command_bytes, &command_len)) {
        return lgfx_nif_badarg(ctx);
    }

    uint32_t failed_index = 0;
    uint8_t failed_opcode = 0;
    bool malformed_command = false;

    esp_err_t err = lgfx_render_batch_dispatch_run(
        command_bytes,
        command_len,
        target,
        &failed_index,
        &failed_opcode,
        &malformed_command);

    (void) failed_index;
    (void) failed_opcode;
    (void) malformed_command;

    return lgfx_nif_esp_result(ctx, err);
}

#include "atom_lgfx/nif_dispatch.inc"

#define LGFX_NIF_ENTRY(name, function_name)           \
    static const struct Nif name = {                  \
        .base.type = NIFFunctionType,                 \
        .nif_ptr = function_name                      \
    }

LGFX_NIF_ENTRY(lgfx_nif_init_0_entry, lgfx_nif_init_0);
LGFX_NIF_ENTRY(lgfx_nif_init_1_entry, lgfx_nif_init_1);
LGFX_NIF_ENTRY(lgfx_nif_call_4_entry, lgfx_nif_call_4);
LGFX_NIF_ENTRY(lgfx_nif_close_0_entry, lgfx_nif_close_0);
LGFX_NIF_ENTRY(lgfx_nif_width_0_entry, lgfx_nif_width_0);
LGFX_NIF_ENTRY(lgfx_nif_height_0_entry, lgfx_nif_height_0);
LGFX_NIF_ENTRY(lgfx_nif_start_write_0_entry, lgfx_nif_start_write_0);
LGFX_NIF_ENTRY(lgfx_nif_end_write_0_entry, lgfx_nif_end_write_0);
LGFX_NIF_ENTRY(lgfx_nif_display_0_entry, lgfx_nif_display_0);
LGFX_NIF_ENTRY(lgfx_nif_set_rotation_1_entry, lgfx_nif_set_rotation_1);
LGFX_NIF_ENTRY(lgfx_nif_set_brightness_1_entry, lgfx_nif_set_brightness_1);
LGFX_NIF_ENTRY(lgfx_nif_fill_screen_1_entry, lgfx_nif_fill_screen_1);
LGFX_NIF_ENTRY(lgfx_nif_clear_1_entry, lgfx_nif_clear_1);
LGFX_NIF_ENTRY(lgfx_nif_draw_pixel_3_entry, lgfx_nif_draw_pixel_3);
LGFX_NIF_ENTRY(lgfx_nif_draw_fast_vline_4_entry, lgfx_nif_draw_fast_vline_4);
LGFX_NIF_ENTRY(lgfx_nif_draw_fast_hline_4_entry, lgfx_nif_draw_fast_hline_4);
LGFX_NIF_ENTRY(lgfx_nif_draw_line_5_entry, lgfx_nif_draw_line_5);
LGFX_NIF_ENTRY(lgfx_nif_draw_rect_5_entry, lgfx_nif_draw_rect_5);
LGFX_NIF_ENTRY(lgfx_nif_fill_rect_5_entry, lgfx_nif_fill_rect_5);
LGFX_NIF_ENTRY(lgfx_nif_draw_round_rect_6_entry, lgfx_nif_draw_round_rect_6);
LGFX_NIF_ENTRY(lgfx_nif_fill_round_rect_6_entry, lgfx_nif_fill_round_rect_6);
LGFX_NIF_ENTRY(lgfx_nif_draw_circle_4_entry, lgfx_nif_draw_circle_4);
LGFX_NIF_ENTRY(lgfx_nif_fill_circle_4_entry, lgfx_nif_fill_circle_4);
LGFX_NIF_ENTRY(lgfx_nif_draw_ellipse_5_entry, lgfx_nif_draw_ellipse_5);
LGFX_NIF_ENTRY(lgfx_nif_fill_ellipse_5_entry, lgfx_nif_fill_ellipse_5);
LGFX_NIF_ENTRY(lgfx_nif_draw_arc_7_entry, lgfx_nif_draw_arc_7);
LGFX_NIF_ENTRY(lgfx_nif_fill_arc_7_entry, lgfx_nif_fill_arc_7);
LGFX_NIF_ENTRY(lgfx_nif_draw_triangle_7_entry, lgfx_nif_draw_triangle_7);
LGFX_NIF_ENTRY(lgfx_nif_fill_triangle_7_entry, lgfx_nif_fill_triangle_7);
LGFX_NIF_ENTRY(lgfx_nif_set_cursor_2_entry, lgfx_nif_set_cursor_2);
LGFX_NIF_ENTRY(lgfx_nif_set_text_size_1_entry, lgfx_nif_set_text_size_1);
LGFX_NIF_ENTRY(lgfx_nif_set_text_datum_1_entry, lgfx_nif_set_text_datum_1);
LGFX_NIF_ENTRY(lgfx_nif_set_text_color_1_entry, lgfx_nif_set_text_color_1);
LGFX_NIF_ENTRY(lgfx_nif_set_text_color_2_entry, lgfx_nif_set_text_color_2);
LGFX_NIF_ENTRY(lgfx_nif_draw_string_3_entry, lgfx_nif_draw_string_3);
LGFX_NIF_ENTRY(lgfx_nif_print_1_entry, lgfx_nif_print_1);
LGFX_NIF_ENTRY(lgfx_nif_println_1_entry, lgfx_nif_println_1);
LGFX_NIF_ENTRY(lgfx_nif_push_image_5_entry, lgfx_nif_push_image_5);
LGFX_NIF_ENTRY(lgfx_nif_batch_2_entry, lgfx_nif_batch_2);

#undef LGFX_NIF_ENTRY

#define LGFX_NIF_LOOKUP(nif_name, entry)     \
    if (strcmp((nif_name), nifname) == 0) {  \
        return &(entry);                     \
    }

const struct Nif *atom_lgfx_native_get_nif(const char *nifname)
{
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:init/0", lgfx_nif_init_0_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:init/1", lgfx_nif_init_1_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:call/4", lgfx_nif_call_4_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:close/0", lgfx_nif_close_0_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:width/0", lgfx_nif_width_0_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:height/0", lgfx_nif_height_0_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:start_write/0", lgfx_nif_start_write_0_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:end_write/0", lgfx_nif_end_write_0_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:display/0", lgfx_nif_display_0_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:set_rotation/1", lgfx_nif_set_rotation_1_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:set_brightness/1", lgfx_nif_set_brightness_1_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:fill_screen/1", lgfx_nif_fill_screen_1_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:clear/1", lgfx_nif_clear_1_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:draw_pixel/3", lgfx_nif_draw_pixel_3_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:draw_fast_vline/4", lgfx_nif_draw_fast_vline_4_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:draw_fast_hline/4", lgfx_nif_draw_fast_hline_4_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:draw_line/5", lgfx_nif_draw_line_5_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:draw_rect/5", lgfx_nif_draw_rect_5_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:fill_rect/5", lgfx_nif_fill_rect_5_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:draw_round_rect/6", lgfx_nif_draw_round_rect_6_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:fill_round_rect/6", lgfx_nif_fill_round_rect_6_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:draw_circle/4", lgfx_nif_draw_circle_4_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:fill_circle/4", lgfx_nif_fill_circle_4_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:draw_ellipse/5", lgfx_nif_draw_ellipse_5_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:fill_ellipse/5", lgfx_nif_fill_ellipse_5_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:draw_arc/7", lgfx_nif_draw_arc_7_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:fill_arc/7", lgfx_nif_fill_arc_7_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:draw_triangle/7", lgfx_nif_draw_triangle_7_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:fill_triangle/7", lgfx_nif_fill_triangle_7_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:set_cursor/2", lgfx_nif_set_cursor_2_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:set_text_size/1", lgfx_nif_set_text_size_1_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:set_text_datum/1", lgfx_nif_set_text_datum_1_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:set_text_color/1", lgfx_nif_set_text_color_1_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:set_text_color/2", lgfx_nif_set_text_color_2_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:draw_string/3", lgfx_nif_draw_string_3_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:print/1", lgfx_nif_print_1_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:println/1", lgfx_nif_println_1_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:push_image/5", lgfx_nif_push_image_5_entry);
    LGFX_NIF_LOOKUP("Elixir.AtomLGFX.Native:batch/2", lgfx_nif_batch_2_entry);

    return NULL;
}

#undef LGFX_NIF_LOOKUP

static void atom_lgfx_native_init(GlobalContext *global)
{
    (void) global;
}

static void atom_lgfx_native_destroy(GlobalContext *global)
{
    (void) global;
}

REGISTER_NIF_COLLECTION(
    atom_lgfx_native,
    atom_lgfx_native_init,
    atom_lgfx_native_destroy,
    atom_lgfx_native_get_nif)
