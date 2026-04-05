/*
 * SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// lgfx_port/include_internal/lgfx_port/batch_decode_internal.h
#pragma once

#include "context.h"
#include "term.h"

#include "lgfx_port/lgfx_port_internal.h"
#include "lgfx_runtime/batch_command.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    term tuple;
    term op_atom;
    uint32_t target;
    uint32_t flags;
    int arity;
} lgfx_batch_wire_command_t;

/* Primitive / shape builders */
term lgfx_batch_build_fill_screen(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_clear_command(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_fill_rect(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_draw_pixel(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_draw_rect(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_draw_round_rect(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_fill_round_rect(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_draw_circle(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_fill_circle(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_draw_ellipse(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_fill_ellipse(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_draw_arc(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_fill_arc(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_draw_bezier(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_draw_triangle(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_fill_triangle(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_draw_fast_vline(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_draw_fast_hline(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_draw_line(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_set_clip_rect(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_clear_clip_rect(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

/* Text builders */
term lgfx_batch_build_set_text_size(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_set_text_datum(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_set_text_wrap(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_set_text_font_preset(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_set_text_color(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_set_cursor(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

/* Sprite builders */
term lgfx_batch_build_push_sprite(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

term lgfx_batch_build_push_rotate_zoom(
    Context *ctx,
    lgfx_port_t *port,
    const lgfx_request_t *submit_req,
    const lgfx_batch_wire_command_t *wire,
    lgfx_batch_command_t *out_command);

#ifdef __cplusplus
}
#endif
