# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.JapaneseText do
  @moduledoc false

  @bg 0x0000
  @fg 0xFFFF
  @muted 0x8410
  @accent 0x07FF

  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    with :ok <- AtomLGFX.fill_screen(port, @bg),
         :ok <- AtomLGFX.reset_text_state(port, 0),
         :ok <- AtomLGFX.set_text_wrap(port, false, 0),
         :ok <- AtomLGFX.set_text_font_preset(port, :ascii, 0),
         :ok <- AtomLGFX.set_text_color(port, @muted, nil, 0),
         :ok <- AtomLGFX.draw_string(port, 8, 8, "Japanese text", 0),
         :ok <- AtomLGFX.set_text_font_preset(port, :jp, 0),
         :ok <- AtomLGFX.set_text_color(port, @fg, nil, 0),
         :ok <- AtomLGFX.draw_string(port, 8, 36, "こんにちは", 0),
         :ok <- AtomLGFX.draw_string(port, 8, 58, "日本語テキスト", 0),
         :ok <-
           AtomLGFX.draw_rect(port, 4, 28, max(8, w - 8), min(84, max(8, h - 36)), @accent, 0),
         :ok <- AtomLGFX.display(port) do
      IO.puts("japanese_text ok")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("japanese_text failed: #{AtomLGFX.format_error(reason)}")
        err
    end
  end
end
