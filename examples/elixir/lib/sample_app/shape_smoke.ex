# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.ShapeSmoke do
  @moduledoc false

  @bg 0x101418
  @outline 0xF5F7FA
  @round_rect_fill 0x2E86DE
  @ellipse_fill 0xF39C12
  @arc_fill 0x27AE60
  @bezier3_color 0xE91E63
  @bezier4_color 0x00BCD4

  def run(port, w, h) when is_integer(w) and is_integer(h) and w > 0 and h > 0 do
    margin = 12
    gap = 12

    top_height = max(div(h - margin * 2 - gap * 2, 3), 20)
    mid_height = top_height
    bottom_height = max(h - margin * 2 - gap * 2 - top_height - mid_height, 20)

    shape_width = max(w - margin * 2, 24)

    round_rect_x = margin
    round_rect_y = margin

    ellipse_center_x = margin + div(shape_width, 2)
    ellipse_center_y = margin + top_height + gap + div(mid_height, 2)

    arc_center_x = margin + div(shape_width, 2)
    arc_center_y = margin + top_height + gap + mid_height + gap + div(bottom_height, 2)

    round_rect_radius =
      min(
        max(div(min(shape_width, top_height), 6), 4),
        24
      )

    ellipse_radius_x = max(div(shape_width, 2) - 4, 4)
    ellipse_radius_y = max(div(mid_height, 2) - 4, 4)

    arc_outer_radius = max(div(min(shape_width, bottom_height), 2) - 4, 8)
    arc_inner_radius = max(arc_outer_radius - 18, 4)

    bottom_y = margin + top_height + gap + mid_height + gap
    bottom_x0 = margin + 8
    bottom_x3 = margin + shape_width - 8
    bottom_mid_x = margin + div(shape_width, 2)

    bezier3_y0 = bottom_y + bottom_height - 8
    bezier3_ctrl_y = bottom_y + 8

    bezier4_y0 = bottom_y + 10
    bezier4_y3 = bottom_y + bottom_height - 10
    bezier4_x1 = margin + div(shape_width, 3)
    bezier4_x2 = margin + div(shape_width * 2, 3)

    with :ok <- AtomLGFX.fill_screen(port, @bg),
         :ok <-
           AtomLGFX.fill_round_rect(
             port,
             round_rect_x,
             round_rect_y,
             shape_width,
             top_height,
             round_rect_radius,
             @round_rect_fill
           ),
         :ok <-
           AtomLGFX.draw_round_rect(
             port,
             round_rect_x,
             round_rect_y,
             shape_width,
             top_height,
             round_rect_radius,
             @outline
           ),
         :ok <-
           AtomLGFX.fill_ellipse(
             port,
             ellipse_center_x,
             ellipse_center_y,
             ellipse_radius_x,
             ellipse_radius_y,
             @ellipse_fill
           ),
         :ok <-
           AtomLGFX.draw_ellipse(
             port,
             ellipse_center_x,
             ellipse_center_y,
             ellipse_radius_x,
             ellipse_radius_y,
             @outline
           ),
         :ok <-
           AtomLGFX.fill_arc(
             port,
             arc_center_x,
             arc_center_y,
             arc_inner_radius,
             arc_outer_radius,
             -30,
             210,
             @arc_fill
           ),
         :ok <-
           AtomLGFX.draw_arc(
             port,
             arc_center_x,
             arc_center_y,
             arc_inner_radius,
             arc_outer_radius,
             -30,
             210,
             @outline
           ),
         :ok <-
           AtomLGFX.draw_bezier(
             port,
             bottom_x0,
             bezier3_y0,
             bottom_mid_x,
             bezier3_ctrl_y,
             bottom_x3,
             bezier3_y0,
             @bezier3_color
           ),
         :ok <-
           AtomLGFX.draw_bezier(
             port,
             bottom_x0,
             bezier4_y0,
             bezier4_x1,
             bezier4_y3,
             bezier4_x2,
             bezier4_y0,
             bottom_x3,
             bezier4_y3,
             @bezier4_color
           ) do
      :ok
    end
  end
end
