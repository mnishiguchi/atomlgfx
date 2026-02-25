# /examples/elixir/lib/sample_app/text_smoke.ex
defmodule SampleApp.TextSmoke do
  @moduledoc false

  alias SampleApp.Port

  @bg 0x000000
  @fg 0xFFFFFF
  @accent 0x00D0FF
  @warn 0xFFD040
  @ok_bg 0x103020

  def run(port, w, h) when is_integer(w) and is_integer(h) and w > 0 and h > 0 do
    line_h = 20
    usable_w = max_i(0, w - 8)

    with :ok <- Port.fill_screen(port, @bg),
         :ok <- Port.reset_text_state(port, 0),
         :ok <- Port.set_text_wrap(port, false, 0),
         :ok <- Port.set_text_datum(port, 0, 0),
         :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.draw_rect(port, 0, 0, w, h, 0x202020),
         :ok <- Port.draw_string_bg(port, 4, 2, @fg, @ok_bg, 1, "TEXT SMOKE", 0),
         :ok <- Port.draw_string(port, 4, 22, "font=1 size=1 datum=0", 0),
         :ok <- Port.set_text_color(port, @accent, nil, 0),
         :ok <- Port.set_text_size(port, 2, 0),
         :ok <- Port.draw_string(port, 4, 40, "HELLO", 0),
         :ok <- Port.set_text_color(port, @warn, nil, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.draw_string(port, 4, 64, "ASCII 123", 0),
         :ok <- maybe_use_japanese_font_preset(port),
         :ok <- Port.set_text_color(port, @fg, nil, 0),
         # Do not reset text size after preset: preset owns it (single-font strategy).
         :ok <- Port.draw_string(port, 4, 64 + line_h, "日本語テスト", 0),
         :ok <- Port.draw_fast_hline(port, 4, 64 + line_h * 2, usable_w, 0x404040),
         :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.draw_string(port, 4, 64 + line_h * 2 + 4, "draw_string ok", 0) do
      IO.puts("text_smoke ok w=#{w} h=#{h}")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("text_smoke failed: #{Port.format_error(reason)}")
        err
    end
  end

  defp maybe_use_japanese_font_preset(port) do
    try_japanese_font_presets(port, [:jp_medium, :jp_small, :jp, :jp_large])
  end

  defp try_japanese_font_presets(_port, []) do
    IO.puts("text_smoke warning: no Japanese font preset available, tofu is expected")
    :ok
  end

  defp try_japanese_font_presets(port, [preset | rest]) do
    case Port.set_font_preset(port, preset, 0) do
      :ok ->
        IO.puts("text_smoke using font preset #{inspect(preset)}")
        :ok

      {:error, reason} ->
        IO.puts("text_smoke preset #{inspect(preset)} unavailable: #{Port.format_error(reason)}")
        try_japanese_font_presets(port, rest)
    end
  end

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
