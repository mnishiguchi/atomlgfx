/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/batch_decode.c

#include <stdlib.h>

#include "esp_err.h"

#include "lgfx_port/batch_decode.h"
#include "lgfx_port/batch_decode_internal.h"
#include "lgfx_port/proto_term.h"

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
        case LGFX_OP_clear:
        case LGFX_OP_fillRect:
        case LGFX_OP_drawPixel:
        case LGFX_OP_drawRect:
        case LGFX_OP_drawRoundRect:
        case LGFX_OP_fillRoundRect:
        case LGFX_OP_drawCircle:
        case LGFX_OP_fillCircle:
        case LGFX_OP_drawEllipse:
        case LGFX_OP_fillEllipse:
        case LGFX_OP_drawArc:
        case LGFX_OP_fillArc:
        case LGFX_OP_drawBezier:
        case LGFX_OP_drawTriangle:
        case LGFX_OP_fillTriangle:
        case LGFX_OP_drawFastVLine:
        case LGFX_OP_drawFastHLine:
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

        case LGFX_OP_clear:
            return lgfx_batch_build_clear_command(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_fillRect:
            return lgfx_batch_build_fill_rect(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_drawPixel:
            return lgfx_batch_build_draw_pixel(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_drawRect:
            return lgfx_batch_build_draw_rect(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_drawRoundRect:
            return lgfx_batch_build_draw_round_rect(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_fillRoundRect:
            return lgfx_batch_build_fill_round_rect(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_drawCircle:
            return lgfx_batch_build_draw_circle(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_fillCircle:
            return lgfx_batch_build_fill_circle(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_drawEllipse:
            return lgfx_batch_build_draw_ellipse(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_fillEllipse:
            return lgfx_batch_build_fill_ellipse(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_drawArc:
            return lgfx_batch_build_draw_arc(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_fillArc:
            return lgfx_batch_build_fill_arc(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_drawBezier:
            return lgfx_batch_build_draw_bezier(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_drawTriangle:
            return lgfx_batch_build_draw_triangle(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_fillTriangle:
            return lgfx_batch_build_fill_triangle(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_drawFastVLine:
            return lgfx_batch_build_draw_fast_vline(ctx, port, submit_req, wire, out_command);

        case LGFX_OP_drawFastHLine:
            return lgfx_batch_build_draw_fast_hline(ctx, port, submit_req, wire, out_command);

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

    *out_commands_term = term_get_tuple_element(req->request_tuple, 5);
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
