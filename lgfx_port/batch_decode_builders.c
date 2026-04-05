/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/batch_decode_builders.c

#include <string.h>

#include "lgfx_port/batch_decode_internal.h"
#include "lgfx_port/handler_decode.h"
#include "lgfx_port/proto_term.h"

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

term lgfx_batch_build_fill_screen(
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

term lgfx_batch_build_clear_command(
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
    out_command->op = LGFX_OP_clear;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_fill_rect(
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

term lgfx_batch_build_draw_pixel(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_drawPixel;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_draw_rect(
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
    out_command->op = LGFX_OP_drawRect;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) w;
    out_command->inline_args.words[3] = (uint32_t) h;
    out_command->inline_args.words[4] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_draw_round_rect(
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
    uint16_t r = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &w) || w == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &h) || h == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 7), &r)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 8), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_drawRoundRect;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) w;
    out_command->inline_args.words[3] = (uint32_t) h;
    out_command->inline_args.words[4] = (uint32_t) r;
    out_command->inline_args.words[5] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_fill_round_rect(
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
    uint16_t r = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &w) || w == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &h) || h == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 7), &r)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 8), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_fillRoundRect;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) w;
    out_command->inline_args.words[3] = (uint32_t) h;
    out_command->inline_args.words[4] = (uint32_t) r;
    out_command->inline_args.words[5] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_draw_circle(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t r = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &r)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_drawCircle;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) r;
    out_command->inline_args.words[3] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_fill_circle(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t r = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &r)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_fillCircle;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) r;
    out_command->inline_args.words[3] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_draw_ellipse(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t rx = 0;
    uint16_t ry = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &rx) || rx == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &ry) || ry == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 7), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_drawEllipse;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) rx;
    out_command->inline_args.words[3] = (uint32_t) ry;
    out_command->inline_args.words[4] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_fill_ellipse(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t rx = 0;
    uint16_t ry = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &rx) || rx == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &ry) || ry == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 7), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_fillEllipse;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) rx;
    out_command->inline_args.words[3] = (uint32_t) ry;
    out_command->inline_args.words[4] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_draw_arc(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t r0 = 0;
    uint16_t r1 = 0;
    float angle0 = 0.0f;
    float angle1 = 0.0f;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &r0) || r0 == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &r1) || r1 == 0u
        || !lgfx_batch_term_to_f32_checked(lgfx_batch_tuple_elem(wire, 7), &angle0)
        || !lgfx_batch_term_to_f32_checked(lgfx_batch_tuple_elem(wire, 8), &angle1)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 9), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_drawArc;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) r0;
    out_command->inline_args.words[3] = (uint32_t) r1;
    out_command->inline_args.words[4] = lgfx_batch_pack_f32(angle0);
    out_command->inline_args.words[5] = lgfx_batch_pack_f32(angle1);
    out_command->inline_args.words[6] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_fill_arc(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t r0 = 0;
    uint16_t r1 = 0;
    float angle0 = 0.0f;
    float angle1 = 0.0f;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &r0) || r0 == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &r1) || r1 == 0u
        || !lgfx_batch_term_to_f32_checked(lgfx_batch_tuple_elem(wire, 7), &angle0)
        || !lgfx_batch_term_to_f32_checked(lgfx_batch_tuple_elem(wire, 8), &angle1)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 9), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_fillArc;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) r0;
    out_command->inline_args.words[3] = (uint32_t) r1;
    out_command->inline_args.words[4] = lgfx_batch_pack_f32(angle0);
    out_command->inline_args.words[5] = lgfx_batch_pack_f32(angle1);
    out_command->inline_args.words[6] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_draw_bezier(
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
    int16_t x2 = 0;
    int16_t y2 = 0;
    int16_t x3 = 0;
    int16_t y3 = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x0)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y0)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 5), &x1)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 6), &y1)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 7), &x2)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 8), &y2)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_drawBezier;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x0;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y0;
    out_command->inline_args.words[2] = (uint32_t) (int32_t) x1;
    out_command->inline_args.words[3] = (uint32_t) (int32_t) y1;
    out_command->inline_args.words[4] = (uint32_t) (int32_t) x2;
    out_command->inline_args.words[5] = (uint32_t) (int32_t) y2;

    if (wire->arity == 10) {
        if (!lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 9), &color)) {
            return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
        }

        out_command->inline_args.words[6] = 0u;
        out_command->inline_args.words[7] = 0u;
        out_command->inline_args.words[8] = 0u;
        out_command->inline_args.words[9] = (uint32_t) color;
        return term_invalid_term();
    }

    if (wire->arity == 12) {
        if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 9), &x3)
            || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 10), &y3)
            || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 11), &color)) {
            return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
        }

        out_command->inline_args.words[6] = 1u;
        out_command->inline_args.words[7] = (uint32_t) (int32_t) x3;
        out_command->inline_args.words[8] = (uint32_t) (int32_t) y3;
        out_command->inline_args.words[9] = (uint32_t) color;
        return term_invalid_term();
    }

    return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
}

term lgfx_batch_build_draw_triangle(
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
    int16_t x2 = 0;
    int16_t y2 = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x0)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y0)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 5), &x1)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 6), &y1)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 7), &x2)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 8), &y2)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 9), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_drawTriangle;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x0;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y0;
    out_command->inline_args.words[2] = (uint32_t) (int32_t) x1;
    out_command->inline_args.words[3] = (uint32_t) (int32_t) y1;
    out_command->inline_args.words[4] = (uint32_t) (int32_t) x2;
    out_command->inline_args.words[5] = (uint32_t) (int32_t) y2;
    out_command->inline_args.words[6] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_fill_triangle(
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
    int16_t x2 = 0;
    int16_t y2 = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x0)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y0)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 5), &x1)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 6), &y1)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 7), &x2)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 8), &y2)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 9), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_fillTriangle;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x0;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y0;
    out_command->inline_args.words[2] = (uint32_t) (int32_t) x1;
    out_command->inline_args.words[3] = (uint32_t) (int32_t) y1;
    out_command->inline_args.words[4] = (uint32_t) (int32_t) x2;
    out_command->inline_args.words[5] = (uint32_t) (int32_t) y2;
    out_command->inline_args.words[6] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_draw_fast_vline(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t h = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &h) || h == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_drawFastVLine;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) h;
    out_command->inline_args.words[3] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_draw_fast_hline(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command)
{
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t color = 0;

    if (!lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 3), &x)
        || !lgfx_batch_term_to_i16(lgfx_batch_tuple_elem(wire, 4), &y)
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 5), &w) || w == 0u
        || !lgfx_batch_term_to_u16(lgfx_batch_tuple_elem(wire, 6), &color)) {
        return reply_error(ctx, port, submit_req, port->atoms.bad_args, 0);
    }

    lgfx_batch_command_clear(out_command);
    out_command->op = LGFX_OP_drawFastHLine;
    out_command->target = (uint8_t) wire->target;
    out_command->flags = wire->flags;
    out_command->inline_args.words[0] = (uint32_t) (int32_t) x;
    out_command->inline_args.words[1] = (uint32_t) (int32_t) y;
    out_command->inline_args.words[2] = (uint32_t) w;
    out_command->inline_args.words[3] = (uint32_t) color;

    return term_invalid_term();
}

term lgfx_batch_build_draw_line(
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

term lgfx_batch_build_set_clip_rect(
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

term lgfx_batch_build_clear_clip_rect(
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

term lgfx_batch_build_set_text_size(
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

term lgfx_batch_build_set_text_datum(
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

term lgfx_batch_build_set_text_wrap(
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

term lgfx_batch_build_set_text_font_preset(
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

term lgfx_batch_build_set_text_color(
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

term lgfx_batch_build_set_cursor(
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

term lgfx_batch_build_push_sprite(
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

term lgfx_batch_build_push_rotate_zoom(
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
