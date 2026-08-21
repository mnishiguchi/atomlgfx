/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Compatibility dispatcher for the full AtomLGFX surface.
//
// Elixir performs operation metadata validation before entering this NIF.  This
// layer still validates every native term before calling the device ABI so raw
// calls cannot pass unchecked values to LovyanGFX.

#include <math.h>

#include "lgfx_port/open_config.h"
#include "lgfx_port/ops.h"
#include "lgfx_port/protocol.h"

#define LGFX_NIF_MAX_ARGS 9

typedef enum
{
    LGFX_NIF_REASON_NONE = 0,
    LGFX_NIF_REASON_BAD_ARGS,
    LGFX_NIF_REASON_BAD_TARGET,
    LGFX_NIF_REASON_NOT_INITIALIZED,
    LGFX_NIF_REASON_NO_MEMORY,
    LGFX_NIF_REASON_INTERNAL,
    LGFX_NIF_REASON_UNSUPPORTED,
    LGFX_NIF_REASON_RESOURCE_BUSY,
} lgfx_nif_reason_t;

typedef struct
{
    bool present;
    uint32_t opcode;
    lgfx_nif_reason_t reason;
    uint32_t flags;
    uint8_t target;
    int32_t esp_err;
} lgfx_nif_last_error_t;

static lgfx_nif_last_error_t lgfx_nif_last_error;

typedef struct
{
    uint8_t min_args;
    uint8_t max_args;
    uint32_t allowed_flags;
    uint8_t target_policy;
    uint8_t state_policy;
} lgfx_nif_op_meta_t;

static const lgfx_nif_op_meta_t lgfx_nif_op_meta[LGFX_OP_COUNT] = {
#define X(name, min_args, max_args, flags, target, state, cap, batchable, owned, sync, boundary) \
    [LGFX_OP_##name] = { (min_args), (max_args), (flags), (target), (state) },
#include "lgfx_port/ops.def"
#undef X
};

static term lgfx_nif_atom(Context *ctx, AtomString atom)
{
    return globalcontext_make_atom(ctx->global, atom);
}

static term lgfx_nif_reason_atom(Context *ctx, lgfx_nif_reason_t reason)
{
    switch (reason) {
        case LGFX_NIF_REASON_BAD_ARGS:
            return lgfx_nif_atom(ctx, ATOM_STR("\x08", "bad_args"));
        case LGFX_NIF_REASON_BAD_TARGET:
            return lgfx_nif_atom(ctx, ATOM_STR("\x0A", "bad_target"));
        case LGFX_NIF_REASON_NOT_INITIALIZED:
            return lgfx_nif_atom(ctx, ATOM_STR("\x0F", "not_initialized"));
        case LGFX_NIF_REASON_NO_MEMORY:
            return lgfx_nif_atom(ctx, ATOM_STR("\x09", "no_memory"));
        case LGFX_NIF_REASON_UNSUPPORTED:
            return lgfx_nif_atom(ctx, ATOM_STR("\x0B", "unsupported"));
        case LGFX_NIF_REASON_RESOURCE_BUSY:
            return lgfx_nif_atom(ctx, ATOM_STR("\x0D", "resource_busy"));
        case LGFX_NIF_REASON_INTERNAL:
            return lgfx_nif_atom(ctx, ATOM_STR("\x08", "internal"));
        case LGFX_NIF_REASON_NONE:
        default:
            return lgfx_nif_atom(ctx, ATOM_STR("\x04", "none"));
    }
}

static term lgfx_nif_ok(Context *ctx, term payload)
{
    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }

    term tuple = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(tuple, 0, term_from_atom_index(OK_ATOM_INDEX));
    term_put_tuple_element(tuple, 1, payload);
    return tuple;
}

static void lgfx_nif_remember_error(
    uint32_t opcode,
    uint8_t target,
    uint32_t flags,
    lgfx_nif_reason_t reason,
    int32_t esp_err)
{
    lgfx_nif_last_error.present = true;
    lgfx_nif_last_error.opcode = opcode;
    lgfx_nif_last_error.reason = reason;
    lgfx_nif_last_error.flags = flags;
    lgfx_nif_last_error.target = target;
    lgfx_nif_last_error.esp_err = esp_err;
}

static term lgfx_nif_dispatch_error(
    Context *ctx,
    uint32_t opcode,
    uint8_t target,
    uint32_t flags,
    lgfx_nif_reason_t reason,
    int32_t esp_err)
{
    lgfx_nif_remember_error(opcode, target, flags, reason, esp_err);
    return lgfx_nif_error(ctx, lgfx_nif_reason_atom(ctx, reason));
}

static lgfx_nif_reason_t lgfx_nif_reason_from_esp(esp_err_t err)
{
    switch (err) {
        case ESP_ERR_INVALID_ARG:
        case ESP_ERR_INVALID_SIZE:
            return LGFX_NIF_REASON_BAD_ARGS;
        case ESP_ERR_NO_MEM:
            return LGFX_NIF_REASON_NO_MEMORY;
        case ESP_ERR_NOT_SUPPORTED:
            return LGFX_NIF_REASON_UNSUPPORTED;
        case ESP_ERR_NOT_FOUND:
            return LGFX_NIF_REASON_BAD_TARGET;
        case ESP_ERR_INVALID_STATE:
        default:
            return LGFX_NIF_REASON_INTERNAL;
    }
}

static term lgfx_nif_dispatch_esp_result(
    Context *ctx,
    uint32_t opcode,
    uint8_t target,
    uint32_t flags,
    esp_err_t err)
{
    if (err == ESP_OK) {
        return lgfx_nif_ok(ctx, term_from_atom_index(OK_ATOM_INDEX));
    }

    return lgfx_nif_dispatch_error(
        ctx,
        opcode,
        target,
        flags,
        lgfx_nif_reason_from_esp(err),
        (int32_t) err);
}

static term lgfx_nif_dispatch_bad_args(Context *ctx, uint32_t opcode, uint8_t target, uint32_t flags)
{
    return lgfx_nif_dispatch_error(
        ctx,
        opcode,
        target,
        flags,
        LGFX_NIF_REASON_BAD_ARGS,
        0);
}

static term lgfx_nif_dispatch_bad_target(Context *ctx, uint32_t opcode, uint8_t target, uint32_t flags)
{
    return lgfx_nif_dispatch_error(
        ctx,
        opcode,
        target,
        flags,
        LGFX_NIF_REASON_BAD_TARGET,
        0);
}

static bool lgfx_nif_copy_list(term list, term out[LGFX_NIF_MAX_ARGS], int *out_count)
{
    bool proper = false;
    int count = term_list_length(list, &proper);
    if (!proper || count < 0 || count > LGFX_NIF_MAX_ARGS || !out_count) {
        return false;
    }

    term cursor = list;
    for (int i = 0; i < count; ++i) {
        out[i] = term_get_list_head(cursor);
        cursor = term_get_list_tail(cursor);
    }

    *out_count = count;
    return true;
}

static bool lgfx_nif_term_to_bool(term value, bool *out)
{
    if (!out) {
        return false;
    }
    if (value == TRUE_ATOM) {
        *out = true;
        return true;
    }
    if (value == FALSE_ATOM) {
        *out = false;
        return true;
    }

    uint32_t decoded = 0;
    if (!lgfx_term_to_u32(value, &decoded) || decoded > 1u) {
        return false;
    }
    *out = decoded == 1u;
    return true;
}

static bool lgfx_nif_term_to_color(term value, bool is_index, uint32_t *out)
{
    if (is_index) {
        uint8_t index = 0;
        if (!lgfx_nif_term_to_u8(value, &index)) {
            return false;
        }
        *out = (uint32_t) index;
        return true;
    }

    uint16_t color = 0;
    if (!lgfx_term_to_color565(value, &color)) {
        return false;
    }
    *out = (uint32_t) color;
    return true;
}

static bool lgfx_nif_term_to_text(term value, bool allow_empty, const uint8_t **out, size_t *out_len)
{
    if (!lgfx_nif_term_to_binary(value, out, out_len)
        || *out_len > (size_t) LGFX_PORT_MAX_BINARY_BYTES
        || (!allow_empty && *out_len == 0u)) {
        return false;
    }
    return *out_len == 0u || memchr(*out, 0, *out_len) == NULL;
}

static uint32_t lgfx_nif_allowed_flags(uint32_t opcode)
{
    return opcode < (uint32_t) LGFX_OP_COUNT
        ? lgfx_nif_op_meta[opcode].allowed_flags
        : 0u;
}

static term lgfx_nif_ok_pair(Context *ctx, int32_t first, int32_t second)
{
    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2) + TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }
    term payload = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(payload, 0, term_from_int((avm_int_t) first));
    term_put_tuple_element(payload, 1, term_from_int((avm_int_t) second));
    term reply = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(reply, 0, term_from_atom_index(OK_ATOM_INDEX));
    term_put_tuple_element(reply, 1, payload);
    return reply;
}

static term lgfx_nif_ok_touch(Context *ctx, bool touched, int16_t x, int16_t y, uint16_t size)
{
    if (!touched) {
        return lgfx_nif_ok(ctx, lgfx_nif_atom(ctx, ATOM_STR("\x04", "none")));
    }
    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(3) + TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }
    term payload = term_alloc_tuple(3, &ctx->heap);
    term_put_tuple_element(payload, 0, term_from_int((avm_int_t) x));
    term_put_tuple_element(payload, 1, term_from_int((avm_int_t) y));
    term_put_tuple_element(payload, 2, term_from_int((avm_int_t) size));
    term reply = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(reply, 0, term_from_atom_index(OK_ATOM_INDEX));
    term_put_tuple_element(reply, 1, payload);
    return reply;
}

static term lgfx_nif_ok_calibration(Context *ctx, const uint16_t values[8])
{
    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(8) + TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }
    term payload = term_alloc_tuple(8, &ctx->heap);
    for (int i = 0; i < 8; ++i) {
        term_put_tuple_element(payload, i, term_from_int((avm_int_t) values[i]));
    }
    term reply = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(reply, 0, term_from_atom_index(OK_ATOM_INDEX));
    term_put_tuple_element(reply, 1, payload);
    return reply;
}

static term lgfx_nif_get_last_error(Context *ctx)
{
    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(6) + TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }

    term none = lgfx_nif_atom(ctx, ATOM_STR("\x04", "none"));
    term payload = term_alloc_tuple(6, &ctx->heap);
    term_put_tuple_element(payload, 0, lgfx_nif_atom(ctx, ATOM_STR("\x0A", "last_error")));
    term_put_tuple_element(
        payload,
        1,
        lgfx_nif_last_error.present ? term_from_int((avm_int_t) lgfx_nif_last_error.opcode) : none);
    term_put_tuple_element(
        payload,
        2,
        lgfx_nif_last_error.present
            ? lgfx_nif_reason_atom(ctx, lgfx_nif_last_error.reason)
            : none);
    term_put_tuple_element(payload, 3, term_from_int((avm_int_t) lgfx_nif_last_error.flags));
    term_put_tuple_element(payload, 4, term_from_int((avm_int_t) lgfx_nif_last_error.target));
    term_put_tuple_element(payload, 5, term_from_int((avm_int_t) lgfx_nif_last_error.esp_err));

    term reply = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(reply, 0, term_from_atom_index(OK_ATOM_INDEX));
    term_put_tuple_element(reply, 1, payload);
    lgfx_nif_last_error = (lgfx_nif_last_error_t) { 0 };
    return reply;
}

#define LGFX_NIF_PRZL_HEADER_SIZE 12u
#define LGFX_NIF_PRZL_RECORD_SIZE 12u
#define LGFX_NIF_PRZL_HAS_TRANSPARENT 0x01u
#define LGFX_NIF_PRZL_APPROX_CULL 0x02u

static uint16_t lgfx_nif_read_le_u16(const uint8_t *bytes)
{
    return (uint16_t) (((uint16_t) bytes[0]) | ((uint16_t) bytes[1] << 8));
}

static bool lgfx_nif_decode_przl(
    term value,
    uint32_t flags,
    const uint8_t **out_records,
    size_t *out_count,
    int16_t *out_y_offset,
    bool *out_has_transparent,
    bool *out_transparent_is_index,
    uint32_t *out_transparent,
    bool *out_approx_cull)
{
    const uint8_t *bytes = NULL;
    size_t len = 0;
    if (!lgfx_nif_term_to_binary(value, &bytes, &len)
        || len > (size_t) LGFX_PORT_MAX_BINARY_BYTES
        || len < LGFX_NIF_PRZL_HEADER_SIZE
        || memcmp(bytes, "PRZL", 4) != 0
        || bytes[4] != 1u) {
        return false;
    }

    uint8_t options = bytes[5];
    if ((options & ~(LGFX_NIF_PRZL_HAS_TRANSPARENT | LGFX_NIF_PRZL_APPROX_CULL)) != 0u) {
        return false;
    }
    uint16_t transparent = lgfx_nif_read_le_u16(bytes + 6);
    int16_t y_offset = (int16_t) lgfx_nif_read_le_u16(bytes + 8);
    uint16_t count = lgfx_nif_read_le_u16(bytes + 10);
    size_t expected = LGFX_NIF_PRZL_HEADER_SIZE + ((size_t) count * LGFX_NIF_PRZL_RECORD_SIZE);
    bool has_transparent = (options & LGFX_NIF_PRZL_HAS_TRANSPARENT) != 0u;
    bool transparent_is_index = (flags & LGFX_F_TRANSPARENT_INDEX) != 0u;
    if (count == 0u || len != expected
        || (!has_transparent && (transparent != 0u || transparent_is_index))
        || (has_transparent && transparent_is_index && transparent > UINT8_MAX)) {
        return false;
    }

    size_t offset = LGFX_NIF_PRZL_HEADER_SIZE;
    for (uint16_t i = 0; i < count; ++i, offset += LGFX_NIF_PRZL_RECORD_SIZE) {
        uint8_t src = bytes[offset];
        uint16_t angle = lgfx_nif_read_le_u16(bytes + offset + 6);
        uint16_t zoom_x = lgfx_nif_read_le_u16(bytes + offset + 8);
        uint16_t zoom_y = lgfx_nif_read_le_u16(bytes + offset + 10);
        if (bytes[offset + 1] != 0u
            || !lgfx_device_is_sprite_target(src)
            || angle >= 36000u
            || zoom_x == 0u
            || zoom_y == 0u) {
            return false;
        }
    }

    *out_records = bytes + LGFX_NIF_PRZL_HEADER_SIZE;
    *out_count = (size_t) count;
    *out_y_offset = y_offset;
    *out_has_transparent = has_transparent;
    *out_transparent_is_index = transparent_is_index;
    *out_transparent = transparent_is_index ? (uint32_t) (transparent & UINT8_MAX) : (uint32_t) transparent;
    *out_approx_cull = (options & LGFX_NIF_PRZL_APPROX_CULL) != 0u;
    return true;
}

#define LGFX_NIF_REQUIRE_COUNT(expected) \
    do { if (arg_count != (expected)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags); } while (0)
#define LGFX_NIF_I16(index, name) \
    int16_t name = 0; if (!lgfx_term_to_i16(args[index], &name)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags)
#define LGFX_NIF_U16(index, name) \
    uint16_t name = 0; if (!lgfx_term_to_u16(args[index], &name)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags)
#define LGFX_NIF_U8(index, name) \
    uint8_t name = 0; if (!lgfx_nif_term_to_u8(args[index], &name)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags)
#define LGFX_NIF_F32(index, name) \
    float name = 0.0f; if (!lgfx_term_to_f32(args[index], &name)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags)

static term lgfx_nif_call_4(Context *ctx, int argc, term argv[])
{
    uint32_t opcode = 0;
    uint8_t target = 0;
    uint32_t flags = 0;
    term args[LGFX_NIF_MAX_ARGS] = { 0 };
    int arg_count = 0;

    if (argc != 4
        || !lgfx_term_to_u32(argv[0], &opcode)
        || opcode >= (uint32_t) LGFX_OP_COUNT
        || !lgfx_nif_term_to_u8(argv[1], &target)
        || !lgfx_term_to_u32(argv[2], &flags)
        || !lgfx_nif_copy_list(argv[3], args, &arg_count)) {
        return lgfx_nif_badarg(ctx);
    }
    const lgfx_nif_op_meta_t *meta = &lgfx_nif_op_meta[opcode];

    if (arg_count < (int) meta->min_args || arg_count > (int) meta->max_args) {
        return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
    }

    switch ((lgfx_op_target_policy_t) meta->target_policy) {
        case LGFX_OP_TARGET_BAD_TARGET:
        case LGFX_OP_TARGET_UNSUPPORTED:
            if (target != LGFX_DEVICE_TARGET_LCD) {
                return lgfx_nif_dispatch_bad_target(ctx, opcode, target, flags);
            }
            break;

        case LGFX_OP_TARGET_ANY:
            if (target > LGFX_DEVICE_TARGET_MAX_SPRITE) {
                return lgfx_nif_dispatch_bad_target(ctx, opcode, target, flags);
            }
            break;

        case LGFX_OP_TARGET_SPRITE_ONLY:
            if (!lgfx_device_is_sprite_target(target)) {
                return lgfx_nif_dispatch_bad_target(ctx, opcode, target, flags);
            }
            break;

        default:
            return lgfx_nif_dispatch_error(
                ctx,
                opcode,
                target,
                flags,
                LGFX_NIF_REASON_INTERNAL,
                0);
    }

    if ((flags & ~lgfx_nif_allowed_flags(opcode)) != 0u) {
        return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
    }

    if (meta->state_policy == LGFX_OP_STATE_REQUIRES_INIT && !lgfx_nif_initialized) {
        return lgfx_nif_dispatch_error(
            ctx,
            opcode,
            target,
            flags,
            LGFX_NIF_REASON_NOT_INITIALIZED,
            0);
    }

    esp_err_t err = ESP_OK;
    bool color_is_index = (flags & LGFX_F_COLOR_INDEX) != 0u;
    uint32_t color = 0;

    switch ((lgfx_op_t) opcode) {
        case LGFX_OP_ping:
            LGFX_NIF_REQUIRE_COUNT(0);
            return lgfx_nif_ok(ctx, lgfx_nif_atom(ctx, ATOM_STR("\x04", "pong")));
        case LGFX_OP_getCaps: {
            LGFX_NIF_REQUIRE_COUNT(0);
            return lgfx_nif_ok(ctx, term_from_int((avm_int_t) LGFX_BUILD_CAP_MASK));
        }
        case LGFX_OP_getLastError:
            LGFX_NIF_REQUIRE_COUNT(0);
            return lgfx_nif_get_last_error(ctx);
        case LGFX_OP_width:
        case LGFX_OP_height: {
            LGFX_NIF_REQUIRE_COUNT(0);
            uint16_t width = 0;
            uint16_t height = 0;
            err = lgfx_device_get_target_dims(target, &width, &height);
            if (err != ESP_OK) {
                return lgfx_nif_dispatch_esp_result(ctx, opcode, target, flags, err);
            }
            uint16_t value = opcode == (uint32_t) LGFX_OP_width ? width : height;
            return lgfx_nif_ok(ctx, term_from_int((avm_int_t) value));
        }
        case LGFX_OP_init:
            LGFX_NIF_REQUIRE_COUNT(0);
            err = lgfx_device_init_with_open_config(
                &lgfx_nif_default_open_config,
                (const void *) &lgfx_nif_owner_token);
            break;
        case LGFX_OP_close:
            LGFX_NIF_REQUIRE_COUNT(0);
            err = lgfx_device_close_for_owner((const void *) &lgfx_nif_owner_token);
            break;
        case LGFX_OP_startWrite:
            LGFX_NIF_REQUIRE_COUNT(0);
            err = lgfx_device_start_write();
            break;
        case LGFX_OP_endWrite:
            LGFX_NIF_REQUIRE_COUNT(0);
            err = lgfx_device_end_write();
            break;
        case LGFX_OP_setRotation: {
            LGFX_NIF_REQUIRE_COUNT(1);
            LGFX_NIF_U8(0, rotation);
            if (rotation > 7u) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_set_rotation(rotation);
            break;
        }
        case LGFX_OP_setBrightness: {
            LGFX_NIF_REQUIRE_COUNT(1);
            LGFX_NIF_U8(0, brightness);
            err = lgfx_device_set_brightness(brightness);
            break;
        }
        case LGFX_OP_setColorDepth: {
            LGFX_NIF_REQUIRE_COUNT(1);
            LGFX_NIF_U8(0, depth);
            if (!lgfx_validate_color_depth(depth)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_set_color_depth(target, depth);
            break;
        }
        case LGFX_OP_setSwapBytes: {
            LGFX_NIF_REQUIRE_COUNT(1);
            bool enabled = false;
            if (!lgfx_nif_term_to_bool(args[0], &enabled)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_set_swap_bytes(target, enabled);
            break;
        }
        case LGFX_OP_display:
            LGFX_NIF_REQUIRE_COUNT(0);
            err = lgfx_device_display();
            break;
        case LGFX_OP_fillScreen:
        case LGFX_OP_clear:
            LGFX_NIF_REQUIRE_COUNT(1);
            if (!lgfx_nif_term_to_color(args[0], color_is_index, &color)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = opcode == (uint32_t) LGFX_OP_fillScreen
                ? lgfx_device_fill_screen(target, color_is_index, color)
                : lgfx_device_clear(target, color_is_index, color);
            break;
        case LGFX_OP_drawPixel: {
            LGFX_NIF_REQUIRE_COUNT(3);
            LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y);
            if (!lgfx_nif_term_to_color(args[2], color_is_index, &color)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_draw_pixel(target, x, y, color_is_index, color);
            break;
        }
        case LGFX_OP_drawFastVLine:
        case LGFX_OP_drawFastHLine: {
            LGFX_NIF_REQUIRE_COUNT(4);
            LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y); LGFX_NIF_U16(2, length);
            if (length == 0u || !lgfx_nif_term_to_color(args[3], color_is_index, &color)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = opcode == (uint32_t) LGFX_OP_drawFastVLine
                ? lgfx_device_draw_fast_vline(target, x, y, length, color_is_index, color)
                : lgfx_device_draw_fast_hline(target, x, y, length, color_is_index, color);
            break;
        }
        case LGFX_OP_drawLine: {
            LGFX_NIF_REQUIRE_COUNT(5);
            LGFX_NIF_I16(0, x0); LGFX_NIF_I16(1, y0); LGFX_NIF_I16(2, x1); LGFX_NIF_I16(3, y1);
            if (!lgfx_nif_term_to_color(args[4], color_is_index, &color)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_draw_line(target, x0, y0, x1, y1, color_is_index, color);
            break;
        }
        case LGFX_OP_drawRect:
        case LGFX_OP_fillRect:
        case LGFX_OP_drawEllipse:
        case LGFX_OP_fillEllipse: {
            LGFX_NIF_REQUIRE_COUNT(5);
            LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y); LGFX_NIF_U16(2, u0); LGFX_NIF_U16(3, u1);
            if (u0 == 0u || u1 == 0u || !lgfx_nif_term_to_color(args[4], color_is_index, &color)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            if (opcode == (uint32_t) LGFX_OP_drawRect) err = lgfx_device_draw_rect(target, x, y, u0, u1, color_is_index, color);
            else if (opcode == (uint32_t) LGFX_OP_fillRect) err = lgfx_device_fill_rect(target, x, y, u0, u1, color_is_index, color);
            else if (opcode == (uint32_t) LGFX_OP_drawEllipse) err = lgfx_device_draw_ellipse(target, x, y, u0, u1, color_is_index, color);
            else err = lgfx_device_fill_ellipse(target, x, y, u0, u1, color_is_index, color);
            break;
        }
        case LGFX_OP_drawRoundRect:
        case LGFX_OP_fillRoundRect: {
            LGFX_NIF_REQUIRE_COUNT(6);
            LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y); LGFX_NIF_U16(2, width); LGFX_NIF_U16(3, height); LGFX_NIF_U16(4, radius);
            if (width == 0u || height == 0u || radius == 0u || !lgfx_nif_term_to_color(args[5], color_is_index, &color)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = opcode == (uint32_t) LGFX_OP_drawRoundRect
                ? lgfx_device_draw_round_rect(target, x, y, width, height, radius, color_is_index, color)
                : lgfx_device_fill_round_rect(target, x, y, width, height, radius, color_is_index, color);
            break;
        }
        case LGFX_OP_drawCircle:
        case LGFX_OP_fillCircle: {
            LGFX_NIF_REQUIRE_COUNT(4);
            LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y); LGFX_NIF_U16(2, radius);
            if (radius == 0u || !lgfx_nif_term_to_color(args[3], color_is_index, &color)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = opcode == (uint32_t) LGFX_OP_drawCircle
                ? lgfx_device_draw_circle(target, x, y, radius, color_is_index, color)
                : lgfx_device_fill_circle(target, x, y, radius, color_is_index, color);
            break;
        }
        case LGFX_OP_drawArc:
        case LGFX_OP_fillArc: {
            LGFX_NIF_REQUIRE_COUNT(7);
            LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y); LGFX_NIF_U16(2, r0); LGFX_NIF_U16(3, r1); LGFX_NIF_F32(4, angle0); LGFX_NIF_F32(5, angle1);
            if (r0 == 0u || r1 == 0u || !lgfx_nif_term_to_color(args[6], color_is_index, &color)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = opcode == (uint32_t) LGFX_OP_drawArc
                ? lgfx_device_draw_arc(target, x, y, r0, r1, angle0, angle1, color_is_index, color)
                : lgfx_device_fill_arc(target, x, y, r0, r1, angle0, angle1, color_is_index, color);
            break;
        }
        case LGFX_OP_drawBezier: {
            if (arg_count != 7 && arg_count != 9) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            LGFX_NIF_I16(0, x0); LGFX_NIF_I16(1, y0); LGFX_NIF_I16(2, x1); LGFX_NIF_I16(3, y1); LGFX_NIF_I16(4, x2); LGFX_NIF_I16(5, y2);
            int color_index = arg_count - 1;
            if (!lgfx_nif_term_to_color(args[color_index], color_is_index, &color)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            if (arg_count == 7) err = lgfx_device_draw_bezier3(target, x0, y0, x1, y1, x2, y2, color_is_index, color);
            else {
                LGFX_NIF_I16(6, x3); LGFX_NIF_I16(7, y3);
                err = lgfx_device_draw_bezier4(target, x0, y0, x1, y1, x2, y2, x3, y3, color_is_index, color);
            }
            break;
        }
        case LGFX_OP_drawTriangle:
        case LGFX_OP_fillTriangle: {
            LGFX_NIF_REQUIRE_COUNT(7);
            LGFX_NIF_I16(0, x0); LGFX_NIF_I16(1, y0); LGFX_NIF_I16(2, x1); LGFX_NIF_I16(3, y1); LGFX_NIF_I16(4, x2); LGFX_NIF_I16(5, y2);
            if (!lgfx_nif_term_to_color(args[6], color_is_index, &color)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = opcode == (uint32_t) LGFX_OP_drawTriangle
                ? lgfx_device_draw_triangle(target, x0, y0, x1, y1, x2, y2, color_is_index, color)
                : lgfx_device_fill_triangle(target, x0, y0, x1, y1, x2, y2, color_is_index, color);
            break;
        }
        case LGFX_OP_setTextSize: {
            if (arg_count != 1 && arg_count != 2) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            LGFX_NIF_F32(0, scale_x);
            if (arg_count == 1) err = lgfx_device_set_text_size(target, scale_x);
            else { LGFX_NIF_F32(1, scale_y); err = lgfx_device_set_text_size_xy(target, scale_x, scale_y); }
            break;
        }
        case LGFX_OP_setTextDatum: {
            LGFX_NIF_REQUIRE_COUNT(1); LGFX_NIF_U8(0, datum);
            err = lgfx_device_set_text_datum(target, datum); break;
        }
        case LGFX_OP_setTextWrap: {
            if (arg_count != 1 && arg_count != 2) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            bool wrap_x = false; bool wrap_y = false;
            if (!lgfx_nif_term_to_bool(args[0], &wrap_x) || (arg_count == 2 && !lgfx_nif_term_to_bool(args[1], &wrap_y))) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_set_text_wrap(target, wrap_x, wrap_y); break;
        }
        case LGFX_OP_setTextFontPreset: {
            LGFX_NIF_REQUIRE_COUNT(1); LGFX_NIF_U8(0, preset);
            if (preset > 1u) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_set_text_font_preset(target, preset); break;
        }
        case LGFX_OP_setTextColor: {
            bool has_bg = (flags & LGFX_F_TEXT_HAS_BG) != 0u;
            bool fg_index = (flags & LGFX_F_TEXT_FG_INDEX) != 0u;
            bool bg_index = (flags & LGFX_F_TEXT_BG_INDEX) != 0u;
            if (arg_count != (has_bg ? 2 : 1) || (!has_bg && bg_index)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            uint32_t fg = 0; uint32_t bg = 0;
            if (!lgfx_nif_term_to_color(args[0], fg_index, &fg) || (has_bg && !lgfx_nif_term_to_color(args[1], bg_index, &bg))) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_set_text_color(target, fg_index, fg, has_bg, bg_index, bg); break;
        }
        case LGFX_OP_setCursor: {
            LGFX_NIF_REQUIRE_COUNT(2); LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y);
            err = lgfx_device_set_cursor(target, x, y); break;
        }
        case LGFX_OP_getCursor: {
            LGFX_NIF_REQUIRE_COUNT(0); int32_t x = 0; int32_t y = 0;
            err = lgfx_device_get_cursor(target, &x, &y);
            if (err != ESP_OK) return lgfx_nif_dispatch_esp_result(ctx, opcode, target, flags, err);
            return lgfx_nif_ok_pair(ctx, x, y);
        }
        case LGFX_OP_drawString:
        case LGFX_OP_print:
        case LGFX_OP_println: {
            const uint8_t *text = NULL; size_t text_len = 0;
            if (opcode == (uint32_t) LGFX_OP_drawString) {
                LGFX_NIF_REQUIRE_COUNT(3); LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y);
                if (!lgfx_nif_term_to_text(args[2], false, &text, &text_len)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
                err = lgfx_device_draw_string(target, x, y, text, text_len);
            } else {
                LGFX_NIF_REQUIRE_COUNT(1);
                if (!lgfx_nif_term_to_text(args[0], true, &text, &text_len)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
                err = opcode == (uint32_t) LGFX_OP_print
                    ? lgfx_device_print(target, text, text_len)
                    : lgfx_device_println(target, text, text_len);
            }
            break;
        }
        case LGFX_OP_drawJpg: {
            if (arg_count != 3 && arg_count != 9) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y);
            uint16_t max_w = 0; uint16_t max_h = 0; int16_t off_x = 0; int16_t off_y = 0; float scale_x = 1.0f; float scale_y = 1.0f;
            int binary_index = 2;
            if (arg_count == 9) {
                if (!lgfx_term_to_u16(args[2], &max_w) || !lgfx_term_to_u16(args[3], &max_h) || !lgfx_term_to_i16(args[4], &off_x) || !lgfx_term_to_i16(args[5], &off_y) || !lgfx_term_to_f32(args[6], &scale_x) || !lgfx_term_to_f32(args[7], &scale_y)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
                binary_index = 8;
            }
            const uint8_t *bytes = NULL; size_t len = 0;
            if (!lgfx_nif_term_to_binary(args[binary_index], &bytes, &len) || len > (size_t) LGFX_PORT_MAX_BINARY_BYTES) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_draw_jpg(target, x, y, max_w, max_h, off_x, off_y, scale_x, scale_y, bytes, len); break;
        }
        case LGFX_OP_pushImage: {
            LGFX_NIF_REQUIRE_COUNT(6); LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y); LGFX_NIF_U16(2, width); LGFX_NIF_U16(3, height); LGFX_NIF_U16(4, stride);
            const uint8_t *bytes = NULL; size_t len = 0;
            if (!lgfx_nif_term_to_binary(args[5], &bytes, &len) || len > (size_t) LGFX_PORT_MAX_BINARY_BYTES) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_push_image_rgb565_strided(target, x, y, width, height, stride, bytes, len); break;
        }
        case LGFX_OP_setClipRect: {
            LGFX_NIF_REQUIRE_COUNT(4); LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y); LGFX_NIF_U16(2, width); LGFX_NIF_U16(3, height);
            if (width == 0u || height == 0u) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_set_clip_rect(target, x, y, width, height); break;
        }
        case LGFX_OP_clearClipRect:
            LGFX_NIF_REQUIRE_COUNT(0); err = lgfx_device_clear_clip_rect(target); break;
        case LGFX_OP_createSprite: {
            if (arg_count != 2 && arg_count != 3) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            LGFX_NIF_U16(0, width); LGFX_NIF_U16(1, height); uint8_t depth = (uint8_t) LGFX_PORT_SPRITE_DEFAULT_DEPTH;
            if (width == 0u || height == 0u || (arg_count == 3 && !lgfx_nif_term_to_u8(args[2], &depth)) || !lgfx_validate_color_depth(depth)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_sprite_create_at(target, width, height, depth); break;
        }
        case LGFX_OP_deleteSprite:
            LGFX_NIF_REQUIRE_COUNT(0); err = lgfx_device_sprite_delete(target);
            if (err == ESP_ERR_INVALID_STATE) return lgfx_nif_dispatch_error(ctx, opcode, target, flags, LGFX_NIF_REASON_RESOURCE_BUSY, (int32_t) err);
            break;
        case LGFX_OP_createPalette:
            LGFX_NIF_REQUIRE_COUNT(0); err = lgfx_device_sprite_create_palette(target); break;
        case LGFX_OP_setPaletteColor: {
            LGFX_NIF_REQUIRE_COUNT(2); LGFX_NIF_U8(0, index); uint32_t rgb888 = 0;
            if (!lgfx_term_to_u32(args[1], &rgb888) || rgb888 > 0xFFFFFFu) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_sprite_set_palette_color(target, index, rgb888); break;
        }
        case LGFX_OP_setPivot: {
            LGFX_NIF_REQUIRE_COUNT(2); LGFX_NIF_I16(0, x); LGFX_NIF_I16(1, y);
            err = lgfx_device_set_pivot(target, x, y); break;
        }
        case LGFX_OP_pushSprite: {
            if (arg_count != 3 && arg_count != 4) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            LGFX_NIF_U8(0, destination); LGFX_NIF_I16(1, x); LGFX_NIF_I16(2, y);
            bool has_transparent = arg_count == 4; bool transparent_is_index = (flags & LGFX_F_TRANSPARENT_INDEX) != 0u; uint32_t transparent = 0;
            if (destination > LGFX_DEVICE_TARGET_MAX_SPRITE || (!has_transparent && transparent_is_index) || (has_transparent && !lgfx_nif_term_to_color(args[3], transparent_is_index, &transparent))) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_sprite_push_sprite(target, destination, x, y, has_transparent, transparent_is_index, transparent); break;
        }
        case LGFX_OP_pushRotateZoom: {
            if (arg_count != 6 && arg_count != 7) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            LGFX_NIF_U8(0, destination); LGFX_NIF_I16(1, x); LGFX_NIF_I16(2, y); LGFX_NIF_F32(3, angle); LGFX_NIF_F32(4, zoom_x); LGFX_NIF_F32(5, zoom_y);
            bool has_transparent = arg_count == 7; bool transparent_is_index = (flags & LGFX_F_TRANSPARENT_INDEX) != 0u; uint32_t transparent = 0;
            if (destination > LGFX_DEVICE_TARGET_MAX_SPRITE || (!has_transparent && transparent_is_index) || (has_transparent && !lgfx_nif_term_to_color(args[6], transparent_is_index, &transparent))) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_sprite_push_rotate_zoom(target, destination, x, y, angle, zoom_x, zoom_y, has_transparent, transparent_is_index, transparent); break;
        }
        case LGFX_OP_getTouch:
        case LGFX_OP_getTouchRaw: {
            LGFX_NIF_REQUIRE_COUNT(0); bool touched = false; int16_t x = 0; int16_t y = 0; uint16_t size = 0;
            err = opcode == (uint32_t) LGFX_OP_getTouch
                ? lgfx_device_get_touch(&touched, &x, &y, &size)
                : lgfx_device_get_touch_raw(&touched, &x, &y, &size);
            if (err != ESP_OK) return lgfx_nif_dispatch_esp_result(ctx, opcode, target, flags, err);
            return lgfx_nif_ok_touch(ctx, touched, x, y, size);
        }
        case LGFX_OP_setTouchCalibrate: {
            LGFX_NIF_REQUIRE_COUNT(8); uint16_t values[8] = { 0 };
            for (int i = 0; i < 8; ++i) if (!lgfx_term_to_u16(args[i], &values[i])) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_set_touch_calibrate(values); break;
        }
        case LGFX_OP_calibrateTouch: {
            LGFX_NIF_REQUIRE_COUNT(0); uint16_t values[8] = { 0 };
            err = lgfx_device_calibrate_touch(values);
            if (err != ESP_OK) return lgfx_nif_dispatch_esp_result(ctx, opcode, target, flags, err);
            return lgfx_nif_ok_calibration(ctx, values);
        }
        case LGFX_OP_pushRotateZoomList: {
            LGFX_NIF_REQUIRE_COUNT(1); const uint8_t *records = NULL; size_t count = 0; int16_t y_offset = 0; bool has_transparent = false; bool transparent_is_index = false; uint32_t transparent = 0; bool approx_cull = false;
            if (!lgfx_nif_decode_przl(args[0], flags, &records, &count, &y_offset, &has_transparent, &transparent_is_index, &transparent, &approx_cull)) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            err = lgfx_device_sprite_push_rotate_zoom_list(target, records, count, y_offset, has_transparent, transparent_is_index, transparent, approx_cull); break;
        }
        case LGFX_OP_submitBinaryBatch: {
            LGFX_NIF_REQUIRE_COUNT(1); const uint8_t *bytes = NULL; size_t len = 0;
            if (!lgfx_nif_term_to_binary(args[0], &bytes, &len) || len == 0u || len > (size_t) LGFX_PORT_MAX_BINARY_BYTES) return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
            uint32_t failed_index = 0; uint8_t failed_opcode = 0; bool malformed = false;
            err = lgfx_render_batch_dispatch_run(bytes, len, target, &failed_index, &failed_opcode, &malformed);
            (void) failed_index; (void) failed_opcode; (void) malformed; break;
        }
        default:
            return lgfx_nif_dispatch_bad_args(ctx, opcode, target, flags);
    }

    if (err == ESP_OK) {
        if (opcode == (uint32_t) LGFX_OP_init) {
            lgfx_nif_initialized = true;
        } else if (opcode == (uint32_t) LGFX_OP_close) {
            lgfx_nif_initialized = false;
        }
    }

    return lgfx_nif_dispatch_esp_result(ctx, opcode, target, flags, err);
}

static term lgfx_nif_init_1(Context *ctx, int argc, term argv[])
{
    if (argc != 1) {
        return lgfx_nif_badarg(ctx);
    }
    lgfx_open_config_overrides_t config = { 0 };
    const char *error_detail = NULL;
    if (!lgfx_parse_open_config_opts(ctx->global, argv[0], &config, &error_detail)) {
        (void) error_detail;
        return lgfx_nif_badarg(ctx);
    }
    esp_err_t err =
        lgfx_device_init_with_open_config(&config, (const void *) &lgfx_nif_owner_token);

    if (err == ESP_OK) {
        lgfx_nif_initialized = true;
    }

    return lgfx_nif_esp_result(ctx, err);
}

#undef LGFX_NIF_F32
#undef LGFX_NIF_U8
#undef LGFX_NIF_U16
#undef LGFX_NIF_I16
#undef LGFX_NIF_REQUIRE_COUNT
