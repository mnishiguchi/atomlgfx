defmodule SampleApp.TextProbe do
  @moduledoc false

  alias LGFXPort, as: Port

  @bg 0x000000
  @fg 0xFFFFFF
  @dim 0xA0A0A0
  @accent 0x00D0FF
  @warn 0xFFD040
  @ok_bg 0x103020
  @frame 0x202020

  # Built-in numeric fonts (usually ASCII-oriented)
  @font_ids [1, 2, 4, 6, 7, 8]

  # Driver-defined presets (may be unsupported depending on build)
  @jp_presets [:jp_small, :jp_medium, :jp_large, :jp]

  # Layout
  @pad 4
  @line_h 20
  @ascii_row_h 20
  @jp_row_h 28

  # Public entry:
  # - Always does the "text smoke" part.
  # - If there is enough vertical space, also draws the font matrix.
  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    with :ok <- Port.fill_screen(port, @bg),
         :ok <- Port.reset_text_state(port, 0),
         :ok <- Port.set_text_wrap_xy(port, false, false, 0),
         :ok <- Port.set_text_datum(port, 0, 0),
         :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.set_text_size_xy(port, 2, 0, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.set_text_color(port, @fg, nil, 0),
         :ok <- Port.draw_rect(port, 0, 0, w, h, @frame),
         {:ok, y_after_smoke} <- draw_text_smoke_block(port, w, h) do
      if should_draw_font_matrix?(h, y_after_smoke) do
        case draw_font_matrix_block(port, w, h, y_after_smoke) do
          :ok ->
            IO.puts("text_probe ok (smoke + matrix)")
            :ok

          {:error, reason} = err ->
            IO.puts("text_probe matrix failed: #{Port.format_error(reason)}")
            err
        end
      else
        IO.puts("text_probe ok (smoke only)")
        :ok
      end
    else
      {:error, reason} = err ->
        IO.puts("text_probe failed: #{Port.format_error(reason)}")
        err
    end
  end

  # Smoke-only entry for callers that want it explicitly.
  def run_smoke(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    with :ok <- Port.fill_screen(port, @bg),
         :ok <- Port.reset_text_state(port, 0),
         :ok <- Port.set_text_wrap_xy(port, false, false, 0),
         :ok <- Port.set_text_datum(port, 0, 0),
         :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.set_text_size_xy(port, 2, 0, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.set_text_color(port, @fg, nil, 0),
         :ok <- Port.draw_rect(port, 0, 0, w, h, @frame),
         {:ok, _y} <- draw_text_smoke_block(port, w, h) do
      IO.puts("text_probe ok (smoke only)")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("text_probe smoke failed: #{Port.format_error(reason)}")
        err
    end
  end

  # -----------------------------------------------------------------------------
  # Text smoke block (top of screen)
  # -----------------------------------------------------------------------------
  defp draw_text_smoke_block(port, w, h) do
    y0 = 2

    with :ok <- Port.draw_string_bg(port, @pad, y0, @fg, @ok_bg, 1, "TEXT + FONT PROBE", 0),
         :ok <- Port.set_text_color(port, @dim, nil, 0),
         :ok <- Port.draw_string(port, @pad, y0 + 14, "font=1 size=1 datum=0 wrap=false", 0),
         :ok <- Port.set_text_color(port, @accent, nil, 0),
         :ok <- Port.set_text_size(port, 2, 0),
         :ok <- Port.draw_string(port, @pad, y0 + 34, "HELLO", 0),
         :ok <- Port.set_text_color(port, @warn, nil, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.draw_string(port, @pad, y0 + 34 + @line_h + 4, "ASCII 123 !?", 0),
         :ok <- maybe_use_japanese_font_preset(port),
         :ok <- Port.set_text_color(port, @fg, nil, 0),
         :ok <- Port.draw_string(port, @pad, y0 + 34 + @line_h * 2 + 4, "日本語テスト", 0),
         :ok <-
           Port.draw_fast_hline(
             port,
             @pad,
             y0 + 34 + @line_h * 3 + 2,
             max_i(0, w - @pad * 2),
             0x404040
           ),
         :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.set_text_color(port, @dim, nil, 0),
         :ok <- Port.draw_string(port, @pad, y0 + 34 + @line_h * 3 + 8, "draw_string ok", 0) do
      y_next = y0 + 34 + @line_h * 3 + 8 + @line_h
      {:ok, min_i(y_next, h)}
    else
      {:error, _reason} = err ->
        err
    end
  end

  defp maybe_use_japanese_font_preset(port) do
    # Prefer medium first for readability; then small; then fallbacks.
    try_japanese_font_presets(port, [:jp_medium, :jp_small, :jp, :jp_large])
  end

  defp try_japanese_font_presets(_port, []) do
    IO.puts("text_probe warning: no Japanese font preset available, tofu is expected")
    :ok
  end

  defp try_japanese_font_presets(port, [preset | rest]) do
    case Port.set_text_font_preset(port, preset, 0) do
      :ok ->
        IO.puts("text_probe using font preset #{inspect(preset)}")
        :ok

      {:error, reason} ->
        IO.puts("text_probe preset #{inspect(preset)} unavailable: #{Port.format_error(reason)}")
        try_japanese_font_presets(port, rest)
    end
  end

  # -----------------------------------------------------------------------------
  # Font matrix block (below the smoke block)
  # -----------------------------------------------------------------------------
  defp should_draw_font_matrix?(h, y_after_smoke) do
    # Roughly needs:
    # - header + a few rows, otherwise it turns into half-rendered noise.
    min_needed = 2 * @line_h + 3 * @ascii_row_h + 2 * @jp_row_h
    y_after_smoke + min_needed < h
  end

  defp draw_font_matrix_block(port, _w, h, y0) do
    ascii_y0 = y0 + 6

    # Make sure our label style is predictable after JP preset.
    with :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.set_text_color(port, @dim, nil, 0),
         :ok <- Port.draw_string(port, @pad, ascii_y0, "ASCII fonts:", 0),
         :ok <- draw_ascii_rows(port, @font_ids, ascii_y0 + 14, @ascii_row_h, h),
         {:ok, jp_y0} <- compute_jp_y0(ascii_y0 + 14, h),
         :ok <- draw_jp_header(port, jp_y0 - 2, h),
         :ok <- draw_jp_rows(port, @jp_presets, jp_y0 + 12, @jp_row_h, h) do
      :ok
    end
  end

  defp compute_jp_y0(ascii_rows_y0, h) do
    # Place JP section after all ASCII rows, but clamp to screen.
    jp_y0 = ascii_rows_y0 + length(@font_ids) * @ascii_row_h + 12

    if jp_y0 + 10 >= h do
      # will effectively skip JP header/rows via guards
      {:ok, h}
    else
      {:ok, jp_y0}
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
           :ok <- Port.draw_string(port, @pad, y, label, 0),
           :ok <- Port.set_text_font(port, font_id, 0),
           :ok <- Port.set_text_size(port, 1, 0),
           :ok <- Port.set_text_color(port, 0x80FF80, nil, 0),
           :ok <- Port.draw_string(port, @pad + 24, y, "ABC abc 123 !?", 0) do
        draw_ascii_rows(port, rest, y0 + row_h, row_h, h)
      end
    end
  end

  defp draw_jp_header(_port, y, h) when y + 10 >= h, do: :ok

  defp draw_jp_header(port, y, _h) do
    with :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.set_text_color(port, @dim, nil, 0),
         :ok <- Port.draw_fast_hline(port, @pad, y, 220, 0x404040),
         :ok <- Port.draw_string(port, @pad, y + 2, "JP presets (preset controls size):", 0) do
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
           :ok <- Port.draw_string(port, @pad, y, label, 0) do
        case Port.set_text_font_preset(port, preset, 0) do
          :ok ->
            IO.puts("text_probe using font preset #{inspect(preset)}")

            with :ok <- Port.set_text_color(port, @fg, nil, 0),
                 # Do not call set_text_size here: preset owns it.
                 :ok <- Port.draw_string(port, 120, y, "日本語: 設定 戻る 次へ", 0) do
              draw_jp_rows(port, rest, y0 + row_h, row_h, h)
            end

          {:error, reason} ->
            IO.puts(
              "text_probe preset #{inspect(preset)} unavailable: #{Port.format_error(reason)}"
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

  # -----------------------------------------------------------------------------
  # Tiny helpers
  # -----------------------------------------------------------------------------
  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
