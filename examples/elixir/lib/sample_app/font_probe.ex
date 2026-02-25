# /examples/elixir/lib/sample_app/font_probe.ex
defmodule SampleApp.FontProbe do
  @moduledoc false

  alias SampleApp.Port

  @bg 0x000000
  @fg 0xFFFFFF
  @dim 0xA0A0A0
  @accent 0x80FF80
  @warn 0xFFD040

  # Built-in numeric fonts (usually ASCII-oriented)
  @font_ids [1, 2, 4, 6, 7, 8]

  # Driver-defined presets (may be unsupported depending on build)
  @jp_presets [:jp_small, :jp_medium, :jp_large, :jp]

  def run(port, w, h) when is_integer(w) and is_integer(h) and w > 0 and h > 0 do
    ascii_row_h = 20
    jp_row_h = 28

    ascii_y0 = 30
    jp_y0 = ascii_y0 + length(@font_ids) * ascii_row_h + 10

    with :ok <- Port.fill_screen(port, @bg),
         :ok <- Port.reset_text_state(port, 0),
         :ok <- Port.set_text_wrap(port, false, 0),
         :ok <- Port.set_text_datum(port, 0, 0),
         :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.set_text_color(port, @fg, nil, 0),
         :ok <- Port.draw_string(port, 4, 2, "FONT PROBE", 0),
         :ok <- Port.set_text_color(port, @dim, nil, 0),
         :ok <- Port.draw_string(port, 4, 14, "ASCII fonts + JP preset check", 0),
         :ok <- draw_ascii_rows(port, @font_ids, ascii_y0, ascii_row_h, h),
         :ok <- draw_jp_header(port, jp_y0 - 2, h),
         :ok <- draw_jp_rows(port, @jp_presets, jp_y0 + 12, jp_row_h, h) do
      IO.puts("font_probe ok")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("font_probe failed: #{Port.format_error(reason)}")
        err
    end
  end

  defp draw_ascii_rows(_port, [], _y0, _row_h, _h), do: :ok

  defp draw_ascii_rows(port, [font_id | rest], y0, row_h, h) do
    y = y0

    if y + 16 >= h do
      :ok
    else
      label = <<"F", :erlang.integer_to_binary(font_id)::binary, ":">>

      with :ok <- Port.set_text_font(port, 1, 0),
           :ok <- Port.set_text_size(port, 1, 0),
           :ok <- Port.set_text_color(port, @dim, nil, 0),
           :ok <- Port.draw_string(port, 4, y, label, 0),
           :ok <- Port.set_text_font(port, font_id, 0),
           :ok <- Port.set_text_size(port, 1, 0),
           :ok <- Port.set_text_color(port, @accent, nil, 0),
           :ok <- Port.draw_string(port, 28, y, "ABC abc 123 !?", 0) do
        draw_ascii_rows(port, rest, y0 + row_h, row_h, h)
      end
    end
  end

  defp draw_jp_header(_port, y, h) when y + 10 >= h, do: :ok

  defp draw_jp_header(port, y, _h) do
    with :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.set_text_color(port, @dim, nil, 0),
         :ok <- Port.draw_fast_hline(port, 4, y, 220, 0x404040),
         :ok <- Port.draw_string(port, 4, y + 2, "JP presets (preset controls size):", 0) do
      :ok
    end
  end

  defp draw_jp_rows(_port, [], _y0, _row_h, _h), do: :ok

  defp draw_jp_rows(port, [preset | rest], y0, row_h, h) do
    y = y0

    if y + 24 >= h do
      :ok
    else
      label = Atom.to_string(preset)

      with :ok <- Port.set_text_font(port, 1, 0),
           :ok <- Port.set_text_size(port, 1, 0),
           :ok <- Port.set_text_color(port, @dim, nil, 0),
           :ok <- Port.draw_string(port, 4, y, label, 0) do
        case Port.set_font_preset(port, preset, 0) do
          :ok ->
            IO.puts("font_probe using font preset #{inspect(preset)}")

            with :ok <- Port.set_text_color(port, @fg, nil, 0),
                 # Do not call set_text_size here: preset owns it.
                 :ok <- Port.draw_string(port, 120, y, "日本語: 設定 戻る 次へ", 0) do
              draw_jp_rows(port, rest, y0 + row_h, row_h, h)
            end

          {:error, reason} ->
            IO.puts(
              "font_probe preset #{inspect(preset)} unavailable: #{Port.format_error(reason)}"
            )

            with :ok <- Port.set_text_font(port, 1, 0),
                 :ok <- Port.set_text_size(port, 1, 0),
                 :ok <- Port.set_text_color(port, @warn, nil, 0),
                 :ok <- Port.draw_string(port, 120, y, "(unsupported)", 0) do
              draw_jp_rows(port, rest, y0 + row_h, row_h, h)
            end
        end
      end
    end
  end
end
