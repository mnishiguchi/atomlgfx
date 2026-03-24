# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.ShapeSmoke do
  @moduledoc false

  @bg 0x101418
  @outline 0xF5F7FA
  @round_rect_fill 0x2E86DE
  @ellipse_fill 0xF39C12

  def run(port, w, h) when is_integer(w) and is_integer(h) and w > 0 and h > 0 do
    margin = 12
    gap = 16
    shape_width = max(w - margin * 2, 24)
    shape_height = max(div(h - margin * 2 - gap, 2), 24)

    round_rect_x = margin
    round_rect_y = margin

    ellipse_center_x = margin + div(shape_width, 2)
    ellipse_center_y = margin + shape_height + gap + div(shape_height, 2)

    round_rect_radius =
      min(
        max(div(min(shape_width, shape_height), 6), 4),
        24
      )

    ellipse_radius_x = max(div(shape_width, 2) - 4, 4)
    ellipse_radius_y = max(div(shape_height, 2) - 4, 4)

    with :ok <- AtomLGFX.fill_screen(port, @bg),
         :ok <-
           AtomLGFX.fill_round_rect(
             port,
             round_rect_x,
             round_rect_y,
             shape_width,
             shape_height,
             round_rect_radius,
             @round_rect_fill
           ),
         :ok <-
           AtomLGFX.draw_round_rect(
             port,
             round_rect_x,
             round_rect_y,
             shape_width,
             shape_height,
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
           ) do
      :ok
    end
  end
end
