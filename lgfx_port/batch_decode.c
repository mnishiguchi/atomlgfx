/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/batch_decode.c

#include "lgfx_port/batch_decode.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"

#include "lgfx_port/handler_decode.h"
#include "lgfx_port/proto_term.h"

typedef struct
{
    term tuple;
    term op_atom;
    uint32_t target;
    uint32_t flags;
    int arity;
} lgfx_batch_wire_command_t;

static bool lgfx_batch_term_to_int32(term value, int32_t *out_value)
{
    if (out_value == NULL || !term_is_integer(value)) {
        return false;
    }

    avm_int_t parsed = term_to_int(value);
    if (parsed < (avm_int_t) INT32_MIN || parsed > (avm_int_t) INT32_MAX) {
        return false;
    }

    *out_value = (int32_t) parsed;
    return true;
}

static bool lgfx_batch_term_to_i16(term value, int16_t *out_value)
{
    int32_t parsed = 0;
    if (!lgfx_batch_term_to_int32(value, &parsed)) {
        return false;
    }

    if (parsed < INT16_MIN || parsed > INT16_MAX) {
        return false;
    }

    *out_value = (int16_t) parsed;
    return true;
}

static bool lgfx_batch_term_to_u8(term value, uint8_t *out_value)
{
    uint32_t parsed = 0;
    if (out_value == NULL || !lgfx_term_to_u32(value, &parsed) || parsed > UINT8_MAX) {
        return false;
    }

    *out_value = (uint8_t) parsed;
    return true;
}

static bool lgfx_batch_term_to_u16(term value, uint16_t *out_value)
{
    uint32_t parsed = 0;
    if (out_value == NULL || !lgfx_term_to_u32(value, &parsed) || parsed > UINT16_MAX) {
        return false;
    }

    *out_value = (uint16_t) parsed;
    return true;
}

static bool lgfx_batch_term_to_f32_checked(term value, float *out_value)
{
    return out_value != NULL && lgfx_term_to_f32(value, out_value);
}

static uint32_t lgfx_batch_pack_f32(float value)
{
    uint32_t bits = 0u;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static term lgfx_batch_tuple_elem(const lgfx_batch_wire_command_t *wire, int index)
{
    return term_get_tuple_element(wire->tuple, index);
}

static bool lgfx_batch_flag_is_set(const lgfx_batch_wire_command_t *wire, uint32_t flag)
{
    return wire != NULL && ((wire->flags & flag) != 0u);
}

static bool lgfx_batch_count_proper_list(term list, size_t *out_count)
{
    if (out_count == NULL) {
        return false;
    }

    *out_count = 0u;

    term cursor = list;
    while (!term_is_nil(cursor)) {
        if (!term_is_list(cursor)) {
            return false;
        }

        (*out_count)++;
        cursor = term_get_list_tail(cursor);
    }

    return true;
}

static bool lgfx_batch_decode_wire_command(term command_t, lgfx_batch_wire_command_t *out)
{
    if (out == NULL || !term_is_tuple(command_t)) {
        return false;
    }

    int arity = term_get_tuple_arity(command_t);
    if (arity < 3) {
        return false;
    }

    term op_atom = term_get_tuple_element(command_t, 0);
    if (!term_is_atom(op_atom)) {
        return false;
    }

    uint32_t target = 0;
    if (!lgfx_term_to_u32(term_get_tuple_element(command_t, 1), &target)) {
        return false;
    }

    uint32_t flags = 0;
    if (!lgfx_term_to_u32(term_get_tuple_element(command_t, 2), &flags)) {
        return false;
    }

    out->tuple = command_t;
    out->op_atom = op_atom;
    out->target = target;
    out->flags = flags;
    out->arity = arity;

    return true;
}

static term lgfx_batch_validate_target_policy(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    uint8_t policy)
{
    if (wire->target > 254u) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_target, 0);
    }

    switch ((lgfx_op_target_policy_t) policy) {
        case LGFX_OP_TARGET_ANY:
            return term_invalid_term();

        case LGFX_OP_TARGET_BAD_TARGET:
            if (wire->target == 0u) {
                return term_invalid_term();
            }
            return reply_error(ctx, port, submit_req, port->atoms.bad_target, 0);

        case LGFX_OP_TARGET_UNSUPPORTED:
            if (wire->target == 0u) {
                return term_invalid_term();
            }
            return reply_error(ctx, port, submit_req, port->atoms.unsupported, 0);

        case LGFX_OP_TARGET_SPRITE_ONLY:
            if (wire->target >= 1u && wire->target <= 254u) {
                return term_invalid_term();
            }
            return reply_error(ctx, port, submit_req, port->atoms.bad_target, 0);

        default:
            return reply_error(ctx, port, submit_req, port->atoms.internal, 0);
    }
}

static bool lgfx_batch_op_supported_in_first_inline_slice(lgfx_op_t op)
{
    switch (op) {
        case LGFX_OP_fillScreen:
        case LGFX_OP_fillRect:
        case LGFX_OP_drawLine:
        case LGFX_OP_setClipRect:
        case LGFX_OP_clearClipRect:
        case LGFX_OP_setTextSize:
        case LGFX_OP_setTextDatum:
        case LGFX_OP_setTextWrap:
        case LGFX_OP_setTextFontPreset:
        case LGFX_OP_setTextColor:
        case LGFX_OP_setCursor:
        case LGFX_OP_pushSprite:
        case LGFX_OP_pushRotateZoom:
            return true;

        default:
            return false;
    }
}

static term lgfx_batch_validate_common_command(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_op_t *out_op)
{
    const lgfx_op_meta_t *meta = lgfx_op_meta_lookup(port, wire->op_atom);
    if (meta == NULL) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_op, 0);
    }

    lgfx_op_t op = LGFX_OP_COUNT;
    if (!lgfx_op_try_from_atom(port, wire->op_atom, &op)) {
        return reply_error(ctx, port, submit_req, port->atoms.internal, 0);
    }

    if (lgfx_dispatch_lookup(port, wire->op_atom) == NULL) {
        return reply_error(ctx, port, submit_req, port->atoms.unsupported, 0);
    }

    if (!meta->batchable || meta->sync_only) {
        return reply_error(ctx, port, submit_req, port->atoms.unsupported, 0);
    }

    if (meta->needs_owned_payload || meta->batch_boundary_sensitive) {
        return reply_error(ctx, port, submit_req, port->atoms.unsupported, 0);
    }

    /*
     * Outer ordinary request shape:
     *   {lgfx, ProtoVer, Op, Target, Flags, ...}
     *
     * Inner batch command shape:
     *   {Op, Target, Flags, ...}
     *
     * So the inner tuple arity is the ordinary protocol arity minus 2.
     */
    int inner_min_arity = (int) meta->min_arity - 2;
    int inner_max_arity = (int) meta->max_arity - 2;
    if (wire->arity < inner_min_arity || wire->arity > inner_max_arity) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    if ((wire->flags & ~meta->allowed_flags_mask) != 0u) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_flags, 0);
    }

    term target_error = lgfx_batch_validate_target_policy(ctx, port, submit_req, wire, meta->target_policy);
    if (!term_is_invalid_term(target_error)) {
        return target_error;
    }

    if (!lgfx_batch_op_supported_in_first_inline_slice(op)) {
        return reply_error(ctx, port, submit_req, port->atoms.unsupported, 0);
    }

    if (out_op != NULL) {
        *out_op = op;
    }

    return term_invalid_term();
}

static term lgfx_batch_build_fill_screen(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    uint16_t color = 0;
    if (!lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 3), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_fillScreen;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) color;

    return term_invalid_term();
}

static term lgfx_batch_build_fill_rect(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &w) || w == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &h) || h == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 7), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_fillRect;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) w;
    out_command->inline_args.words[3] = (uint32_t) h;
    out_command->inline_args.words[4] = (uint32_t) color;

    return term_invalid_term();
}

static term lgfx_batch_build_draw_line(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x0 = 0;
    int16_t y0 = 0;
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x0)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y0)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 5), &x1)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 6), &y1)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 7), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_drawLine;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x0;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y0;
    out_command->inline_args.words[2] = (uint32_t) (int32_t) x1;
    out_command->inline_args.words[3] = (uint32_t) (int32_t) y1;
    out_command->inline_args.words[4] = (uint32_t) color;

    return term_invalid_term();
}

static term lgfx_batch_build_set_clip_rect(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &w) || w == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &h) || h == 0u) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_setClipRect;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) w;
    out_command->inline_args.words[3] = (uint32_t) h;

    return term_invalid_term();
}

static term lgfx_batch_build_clear_clip_rect(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    (void) ctx;
    (void) port;
    (void) submit_req;
    (void) wire;

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_clearClipRect;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;

    return term_invalid_term();
}

static term lgfx_batch_build_set_text_size(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    float scale_x = 0.0f;
    float scale_y = 0.0f;
    bool use_xy = false;

    if (!lgfx_batch_term_to_f32_checked(lgfx_batch_tuple_elem(wire, 3), &scale_x)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    if (wire->arity == 5) {
        if (!lgfx_batch_term_to_f32_checked(lgfx_batch_tuple_elem(wire, 4), &scale_y)) {
            return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
        }
        use_xy = true;
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_setTextSize;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = use_xy ? 1u : 0u;
    out_command->inline_args.words[1] = lgfx_batch_pack_f32(scale_x);
    out_command->inline_args.words[2] = lgfx_batch_pack_f32(scale_y);

    return term_invalid_term();
}

static term lgfx_batch_build_set_text_datum(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    uint8_t datum = 0u;
    if (!lgfx_batch_term_to_u8(lgfx_batch_tuple_elem(wire, 3), &datum)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_setTextDatum;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) datum;

    return term_invalid_term();
}

static term lgfx_batch_build_set_text_wrap(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    bool wrap_x = false;
    bool wrap_y = false;

    if (!lgfx_decode_bool_term(port, lgfx_batch_tuple_elem(wire, 3), &wrap_x)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    if (wire->arity == 5) {
        if (!lgfx_decode_bool_term(port, lgfx_batch_tuple_elem(wire, 4), &wrap_y)) {
            return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
        }
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_setTextWrap;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = wrap_x ? 1u : 0u;
    out_command->inline_args.words[1] = wrap_y ? 1u : 0u;

    return term_invalid_term();
}

static term lgfx_batch_build_set_text_font_preset(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    uint8_t preset = 0u;
    if (!lgfx_batch_term_to_u8(lgfx_batch_tuple_elem(wire, 3), &preset)
        || preset > (uint8_t) LGFX_FONT_PRESET_JP) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_setTextFontPreset;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) preset;

    return term_invalid_term();
}

static term lgfx_batch_decode_text_color_value(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    term value_t,
    bool is_index,
    uint32_t *out_value)
{
    if (is_index) {
        uint8_t palette_index = 0u;
        if (!lgfx_batch_term_to_u8(value_t, &palette_index)) {
            return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
        }

        *out_value = (uint32_t) palette_index;
        return term_invalid_term();
    }

    uint16_t color565 = 0u;
    if (!lgfx_batch_term_to_u16(value_t, &color565)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    *out_value = (uint32_t) color565;
    return term_invalid_term();
}

static term lgfx_batch_build_set_text_color(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    const bool has_bg = lgfx_batch_flag_is_set(wire, LGFX_F_TEXT_HAS_BG);
    const bool fg_is_index = lgfx_batch_flag_is_set(wire, LGFX_F_TEXT_FG_INDEX);
    const bool bg_is_index = lgfx_batch_flag_is_set(wire, LGFX_F_TEXT_BG_INDEX);

    if (bg_is_index && !has_bg) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    uint32_t fg_value = 0u;
    term fg_error = lgfx_batch_decode_text_color_value(
        ctx,
        port,
        submit_req,
        lgfx_batch_tuple_elem(wire, 3),
        fg_is_index,
        &fg_value);
    if (!term_is_invalid_term(fg_error)) {
        return fg_error;
    }

    uint32_t bg_value = 0u;
    if (has_bg) {
        term bg_error = lgfx_batch_decode_text_color_value(
            ctx,
            port,
            submit_req,
            lgfx_batch_tuple_elem(wire, 4),
            bg_is_index,
            &bg_value);
        if (!term_is_invalid_term(bg_error)) {
            return bg_error;
        }
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_setTextColor;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = fg_value;
    out_command->inline_args.words[1] = bg_value;

    return term_invalid_term();
}

static term lgfx_batch_build_set_cursor(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_setCursor;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;

    return term_invalid_term();
}

static term lgfx_batch_decode_dst_target_any(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    term value,
    uint8_t *out_dst_target)
{
    uint8_t dst_target = 0u;
    if (!lgfx_batch_term_to_u8(value, &dst_target) || dst_target > 254u) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_target, 0);
    }

    *out_dst_target = dst_target;
    return term_invalid_term();
}

static term lgfx_batch_decode_optional_transparent(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    int index,
    bool *out_has_transparent,
    uint32_t *out_transparent_value)
{
    const bool transparent_is_index = (wire->flags & (uint32_t) LGFX_F_TRANSPARENT_INDEX) != 0u;

    *out_has_transparent = false;
    *out_transparent_value = 0u;

    if (wire->arity <= index) {
        return term_invalid_term();
    }

    if (transparent_is_index) {
        uint8_t palette_index = 0u;
        if (!lgfx_batch_term_to_u8(lgfx_batch_tuple_elem(wire, index), &palette_index)) {
            return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
        }

        *out_has_transparent = true;
        *out_transparent_value = (uint32_t) palette_index;
        return term_invalid_term();
    }

    uint16_t color565 = 0u;
    if (!lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, index), &color565)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    *out_has_transparent = true;
    *out_transparent_value = (uint32_t) color565;
    return term_invalid_term();
}

static term lgfx_batch_build_push_sprite(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    uint8_t dst_target = 0u;
    int16_t x = 0;
    int16_t y = 0;
    bool has_transparent = false;
    uint32_t transparent_value = 0u;

    term dst_target_error = lgfx_batch_decode_dst_target_any(ctx, port, submit_req, lgfx_batch_tuple_elem(wire, 3), &dst_target);
    if (!term_is_invalid_term(dst_target_error)) {
        return dst_target_error;
    }

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 5), &y)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    term transparent_error = lgfx_batch_decode_optional_transparent(
        ctx,
        port,
        submit_req,
        wire,
        6,
        &has_transparent,
        &transparent_value);
    if (!term_is_invalid_term(transparent_error)) {
        return transparent_error;
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_pushSprite;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) dst_target;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[2] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[3] = has_transparent ? 1u : 0u;
    out_command->inline_args.words[4] = transparent_value;

    return term_invalid_term();
}

static term lgfx_batch_build_push_rotate_zoom(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    uint8_t dst_target = 0u;
    int16_t x = 0;
    int16_t y = 0;
    float angle = 0.0f;
    float zoom_x = 0.0f;
    float zoom_y = 0.0f;
    bool has_transparent = false;
    uint32_t transparent_value = 0u;

    term dst_target_error = lgfx_batch_decode_dst_target_any(ctx, port, submit_req, lgfx_batch_tuple_elem(wire, 3), &dst_target);
    if (!term_is_invalid_term(dst_target_error)) {
        return dst_target_error;
    }

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 5), &y)
        || !lgfx_batch_term_to_f32_checked(lgfx_batch_tuple_elem(wire, 6), &angle)
        || !lgfx_batch_term_to_f32_checked(lgfx_batch_tuple_elem(wire, 7), &zoom_x)
        || !lgfx_batch_term_to_f32_checked(lgfx_batch_tuple_elem(wire, 8), &zoom_y)
        || !lgfx_validate_positive_f32(zoom_x)
        || !lgfx_validate_positive_f32(zoom_y)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    term transparent_error = lgfx_batch_decode_optional_transparent(
        ctx,
        port,
        submit_req,
        wire,
        9,
        &has_transparent,
        &transparent_value);
    if (!term_is_invalid_term(transparent_error)) {
        return transparent_error;
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_pushRotateZoom;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) dst_target;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[2] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[3] = lgfx_batch_pack_f32(angle);
    out_command->inline_args.words[4] = lgfx_batch_pack_f32(zoom_x);
    out_command->inline_args.words[5] = lgfx_batch_pack_f32(zoom_y);
    out_command->inline_args.words[6] = has_transparent ? 1u : 0u;
    out_command->inline_args.words[7] = transparent_value;

    return term_invalid_term();
}

static term lgfx_batch_build_one_inline_command(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_op_t op,
    lgfx_batch_command_t *out_command)
{
    switch (op) {
        case LGFX_OP_fillScreen:
            return lgfx_batch_build_fill_screen(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_fillRect:
            return lgfx_batch_build_fill_rect(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_drawLine:
            return lgfx_batch_build_draw_line(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_setClipRect:
            return lgfx_batch_build_set_clip_rect(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_clearClipRect:
            return lgfx_batch_build_clear_clip_rect(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_setTextSize:
            return lgfx_batch_build_set_text_size(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_setTextDatum:
            return lgfx_batch_build_set_text_datum(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_setTextWrap:
            return lgfx_batch_build_set_text_wrap(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_setTextFontPreset:
            return lgfx_batch_build_set_text_font_preset(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_setTextColor:
            return lgfx_batch_build_set_text_color(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_setCursor:
            return lgfx_batch_build_set_cursor(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_pushSprite:
            return lgfx_batch_build_push_sprite(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_pushRotateZoom:
            return lgfx_batch_build_push_rotate_zoom(ctx, port, submit_req, wire, out_command);

        default:
            return reply_error(ctx, port, submit_req, port->atoms.unsupported, 0);
    }
}

void lgfx_batch_build_clear(lgfx_batch_build_t *build)
{
    if (build == NULL) {
        return;
    }

    if (build->commands != NULL) {
        free(build->commands);
    }

    build->commands = NULL;
    build->command_count = 0u;
}

bool lgfx_batch_decode_submit_request(
    const lgfx_request_t *req,
    term *out_commands_term)
{
    if (out_commands_term != NULL) {
        *out_commands_term = term_invalid_term();
    }

    if (req == NULL || out_commands_term == NULL) {
        return false;
    }

    *out_commands_term = lgfx_req_elem(req, 5);
    return !term_is_invalid_term(*out_commands_term);
}

bool lgfx_batch_build_inline_submit(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    term commands_t,
    lgfx_batch_build_t *out_build,
    term *out_error_reply)
{
    if (out_error_reply != NULL) {
        *out_error_reply = term_invalid_term();
    }

    if (ctx == NULL || port == NULL || submit_req == NULL || out_build == NULL) {
        return false;
    }

    lgfx_batch_build_clear(out_build);

    size_t command_count = 0u;
    if (!lgfx_batch_count_proper_list(commands_t, &command_count) || command_count == 0u) {
        if (out_error_reply != NULL) {
            *out_error_reply = reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
        }
        return false;
    }

    lgfx_batch_command_t *commands = (lgfx_batch_command_t *) calloc(command_count, sizeof(lgfx_batch_command_t));
    if (commands == NULL) {
        if (out_error_reply != NULL) {
            *out_error_reply = reply_error(
                ctx,
                port,
                submit_req,
                port->atoms.no_memory,
                (int32_t) ESP_ERR_NO_MEM);
        }
        return false;
    }

    term cursor = commands_t;
    size_t index = 0u;

    while (!term_is_nil(cursor)) {
        term command_t = term_get_list_head(cursor);
        cursor = term_get_list_tail(cursor);

        lgfx_batch_wire_command_t wire = { 0 };
        if (!lgfx_batch_decode_wire_command(command_t, &wire)) {
            free(commands);

            if (out_error_reply != NULL) {
                *out_error_reply = reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
            }
            return false;
        }

        lgfx_op_t op = LGFX_OP_COUNT;
        term common_error = lgfx_batch_validate_common_command(ctx, port, submit_req, &wire, &op);

        if (!term_is_invalid_term(common_error)) {
            free(commands);

            if (out_error_reply != NULL) {
                *out_error_reply = common_error;
            }
            return false;
        }

        term build_error = lgfx_batch_build_one_inline_command(ctx, port, submit_req, &wire, op, &commands[index]);
        if (!term_is_invalid_term(build_error)) {
            free(commands);

            if (out_error_reply != NULL) {
                *out_error_reply = build_error;
            }
            return false;
        }

        index++;
    }

    out_build->commands = commands;
    out_build->command_count = command_count;
    return true;
}
