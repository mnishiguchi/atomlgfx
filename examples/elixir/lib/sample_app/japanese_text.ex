# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.JapaneseText do
  @moduledoc false

  def run(port, width, height)
      when is_integer(width) and width > 0 and is_integer(height) and height > 0 do
    with :ok <- AtomLGFX.reset_text_state(port, 0),
         :ok <-
           AtomLGFX.render_lcd(
             port,
             [
               {:fill_screen, :black},
               {:set_text_wrap, false},
               {:set_text_font_preset, :ascii},
               {:set_text_color, :dark_grey},
               {:draw_string, "Japanese text", 8, 8},
               {:set_text_font_preset, :jp},
               {:set_text_color, :white},
               {:draw_string, "こんにちは", 8, 36},
               {:draw_string, "日本語テキスト", 8, 58},
               {:draw_rect, 4, 28, max(8, width - 8), min(84, max(8, height - 36)), :cyan},
               :display
             ],
             validate: true
           ) do
      IO.puts("japanese_text ok")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("japanese_text failed: #{AtomLGFX.format_error(reason)}")
        err
    end
  end
end
