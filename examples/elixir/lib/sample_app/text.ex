# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.Text do
  @moduledoc false

  def run(port, width, height)
      when is_integer(width) and width > 0 and is_integer(height) and height > 0 do
    margin = 10
    baseline_y = min(height - margin, 116)

    with :ok <- AtomLGFX.reset_text_state(port, 0) do
      AtomLGFX.render_lcd(
        port,
        [
          {:fill_screen, :black},
          {:set_text_font_preset, :ascii},
          {:set_text_wrap, false},
          {:set_text_color, :dark_grey},
          {:draw_string, "Text rendering", margin, margin},
          {:set_text_color, :white},
          {:set_cursor, margin, margin + 32},
          {:println, "println uses cursor state"},
          {:set_text_size, 2},
          {:set_text_color, :cyan},
          {:draw_string, "size 2", margin, margin + 62},
          {:set_text_size, 1},
          {:set_text_color, :yellow},
          {:draw_fast_hline, margin, baseline_y, max(16, width - margin * 2), :yellow},
          {:draw_string, "render_lcd/3", margin, baseline_y + 8},
          :display
        ],
        validate: true
      )
    end
  end
end
