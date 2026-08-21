# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.BasicShapes do
  @moduledoc false

  def run(handle, width, height)
      when is_integer(width) and width > 0 and is_integer(height) and height > 0 do
    margin = 10
    center_x = div(width, 2)
    center_y = div(height, 2)
    box_width = max(32, width - margin * 2)
    box_height = max(32, height - margin * 2)
    circle_radius = max(8, div(min(width, height), 8))

    AtomLGFX.render_lcd(
      handle,
      [
        {:fill_screen, :black},
        {:set_text_color, :white},
        {:set_text_datum, :top_left},
        {:set_cursor, margin, margin},
        {:println, "AtomLGFX.render_lcd"},
        {:draw_round_rect, margin, margin + 24, box_width, box_height - 24, 8, :white},
        {:draw_line, margin, center_y, width - margin, center_y, {:rgb, 0, 255, 255}},
        {:draw_line, center_x, margin + 24, center_x, height - margin, {:rgb888, 0x00FFFF}},
        {:fill_circle, center_x, center_y, circle_radius, {:rgb, 255, 0, 0}},
        {:draw_circle, center_x, center_y, circle_radius + 6, :yellow},
        {:draw_arc, center_x, center_y, circle_radius + 18, circle_radius + 22, 30, 300, :orange},
        {:fill_triangle, center_x, max(margin + 34, center_y - circle_radius - 36),
         max(margin + 10, center_x - 30), center_y - 8, min(width - margin - 10, center_x + 30),
         center_y - 8, :green},
        :display
      ],
      validate: true
    )
  end
end
