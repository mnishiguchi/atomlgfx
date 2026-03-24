# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.TextProbe do
  @moduledoc false

  @bg 0x0000
  @fg 0xFFFF
  @dim 0xA514
  @accent 0x069F
  @warn 0xFE88
  @ok_bg 0x1184
  @frame 0x2104

  @ascii_preset :ascii
  @jp_preset :jp

  # Layout
  @pad 4
  @line_h 20
  @ascii_row_h 20
  @jp_row_h 28
  @ascii_sample_x 48
  @jp_sample_x 120

  # Public entry:
  # - Always does the "text smoke" part.
  # - If there is enough vertical space, also draws the preset matrix.
  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    run_probe(port, w, h, true)
  end

  # Smoke-only entry for callers that want it explicitly.
  def run_smoke(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    run_probe(port, w, h, false)
  end

  defp run_probe(port, w, h, draw_matrix?) do
    with :ok <- prepare_probe_canvas(port, w, h),
         {:ok, y_after_smoke} <- draw_text_smoke_block(port, w, h) do
      cond do
        draw_matrix? and should_draw_preset_matrix?(h, y_after_smoke) ->
          case draw_preset_matrix_block(port, h, y_after_smoke) do
            :ok ->
              IO.puts("text_probe ok (smoke + matrix)")
              :ok

            {:error, reason} = err ->
              IO.puts("text_probe matrix failed: #{AtomLGFX.format_error(reason)}")
              err
          end

        true ->
          IO.puts("text_probe ok (smoke only)")
          :ok
      end
    else
      {:error, reason} = err ->
        IO.puts("text_probe failed: #{AtomLGFX.format_error(reason)}")
        err
    end
  end

  defp prepare_probe_canvas(port, w, h) do
    with :ok <- AtomLGFX.fill_screen(port, @bg),
         :ok <- AtomLGFX.reset_text_state(port, 0),
         :ok <- AtomLGFX.set_text_wrap_xy(port, false, false, 0),
         :ok <- AtomLGFX.set_text_datum(port, 0, 0),
         :ok <- set_ascii_text_style(port, @fg, 1),
         :ok <- AtomLGFX.draw_rect(port, 0, 0, w, h, @frame) do
      :ok
    end
  end

  # -----------------------------------------------------------------------------
  # Text smoke block (top of screen)
  # -----------------------------------------------------------------------------
  #
  # Intentionally exercises:
  # - set_text_font_preset/3
  # - set_text_size/3
  # - set_text_size_xy/4
  # - draw_string/5
  defp draw_text_smoke_block(port, w, h) do
    y0 = 2
    meta_y = y0 + 14
    hello_y = y0 + 34
    wide_y = hello_y + @line_h + 4
    ascii_y = wide_y + @line_h + 4
    jp_y = ascii_y + @line_h + 4
    rule_y = jp_y + @line_h - 2
    status_y = rule_y + 6

    with :ok <- AtomLGFX.draw_string_bg(port, @pad, y0, @fg, @ok_bg, 1, "TEXT + PRESET PROBE", 0),
         :ok <- set_ascii_text_style(port, @dim, 1),
         :ok <-
           AtomLGFX.draw_string(port, @pad, meta_y, "preset=ascii size=1 datum=0 wrap=false", 0),
         :ok <- set_ascii_text_style(port, @accent, 2),
         :ok <- AtomLGFX.draw_string(port, @pad, hello_y, "HELLO", 0),
         :ok <- set_ascii_text_style_xy(port, @accent, 2, 1),
         :ok <- AtomLGFX.draw_string(port, @pad, wide_y, "WIDE XY", 0),
         :ok <- set_ascii_text_style(port, @warn, 1),
         :ok <- AtomLGFX.draw_string(port, @pad, ascii_y, "ASCII 123 !?", 0),
         :ok <- maybe_use_japanese_preset(port),
         :ok <- AtomLGFX.set_text_color(port, @fg, nil, 0),
         :ok <- AtomLGFX.draw_string(port, @pad, jp_y, "日本語テスト", 0),
         :ok <- AtomLGFX.draw_fast_hline(port, @pad, rule_y, max_i(0, w - @pad * 2), 0x4208),
         :ok <- set_ascii_text_style(port, @dim, 1),
         :ok <- AtomLGFX.draw_string(port, @pad, status_y, "draw_string ok / size_xy ok", 0) do
      {:ok, min_i(status_y + @line_h, h)}
    else
      {:error, _reason} = err ->
        err
    end
  end

  defp maybe_use_japanese_preset(port) do
    case AtomLGFX.set_text_font_preset(port, @jp_preset, 0) do
      :ok ->
        IO.puts("text_probe using preset #{@jp_preset}")
        :ok

      {:error, reason} ->
        IO.puts("text_probe preset #{@jp_preset} unavailable: #{AtomLGFX.format_error(reason)}")
        IO.puts("text_probe warning: no Japanese preset available, tofu is expected")
        :ok
    end
  end

  # -----------------------------------------------------------------------------
  # Preset matrix block (below the smoke block)
  # -----------------------------------------------------------------------------
  defp should_draw_preset_matrix?(h, y_after_smoke) do
    min_needed = 2 * @line_h + @ascii_row_h + @jp_row_h
    y_after_smoke + min_needed < h
  end

  defp draw_preset_matrix_block(port, h, y0) do
    ascii_y0 = y0 + 6
    jp_y0 = ascii_y0 + 14 + @ascii_row_h + 12

    with :ok <- draw_ascii_header(port, ascii_y0, h),
         :ok <- draw_ascii_row(port, ascii_y0 + 14, h),
         :ok <- draw_jp_header(port, jp_y0 - 2, h),
         :ok <- draw_jp_row(port, jp_y0 + 12, h) do
      :ok
    end
  end

  defp draw_ascii_header(_port, y, h) when y + 10 >= h, do: :ok

  defp draw_ascii_header(port, y, _h) do
    with :ok <- set_ascii_text_style(port, @dim, 1),
         :ok <- AtomLGFX.draw_string(port, @pad, y, "ASCII preset:", 0) do
      :ok
    end
  end

  defp draw_ascii_row(_port, y, h) when y + 16 >= h, do: :ok

  defp draw_ascii_row(port, y, _h) do
    label = Atom.to_string(@ascii_preset)

    with :ok <- set_ascii_text_style(port, @dim, 1),
         :ok <- AtomLGFX.draw_string(port, @pad, y, label, 0),
         :ok <- set_ascii_text_style(port, 0x87F0, 1),
         :ok <- AtomLGFX.draw_string(port, @ascii_sample_x, y, "ABC abc 123 !?", 0) do
      :ok
    end
  end

  defp draw_jp_header(_port, y, h) when y + 10 >= h, do: :ok

  defp draw_jp_header(port, y, _h) do
    with :ok <- set_ascii_text_style(port, @dim, 1),
         :ok <- AtomLGFX.draw_fast_hline(port, @pad, y, 220, 0x4208),
         :ok <- AtomLGFX.draw_string(port, @pad, y + 2, "Japanese-capable preset:", 0) do
      :ok
    end
  end

  defp draw_jp_row(_port, y, h) when y + 24 >= h, do: :ok

  defp draw_jp_row(port, y, _h) do
    label = Atom.to_string(@jp_preset)

    with :ok <- set_ascii_text_style(port, @dim, 1),
         :ok <- AtomLGFX.draw_string(port, @pad, y, label, 0) do
      case AtomLGFX.set_text_font_preset(port, @jp_preset, 0) do
        :ok ->
          IO.puts("text_probe using preset #{@jp_preset}")

          with :ok <- AtomLGFX.set_text_color(port, @fg, nil, 0),
               :ok <- AtomLGFX.draw_string(port, @jp_sample_x, y, "日本語: 設定 戻る 次へ", 0) do
            :ok
          end

        {:error, reason} ->
          IO.puts("text_probe preset #{@jp_preset} unavailable: #{AtomLGFX.format_error(reason)}")

          with :ok <- set_ascii_text_style(port, @warn, 1),
               :ok <- AtomLGFX.draw_string(port, @jp_sample_x, y, "(unsupported)", 0) do
            :ok
          end
      end
    end
  end

  defp set_ascii_text_style(port, color, scale) do
    with :ok <- AtomLGFX.set_text_font_preset(port, @ascii_preset, 0),
         :ok <- AtomLGFX.set_text_size(port, scale, 0),
         :ok <- AtomLGFX.set_text_color(port, color, nil, 0) do
      :ok
    end
  end

  defp set_ascii_text_style_xy(port, color, scale_x, scale_y) do
    with :ok <- AtomLGFX.set_text_font_preset(port, @ascii_preset, 0),
         :ok <- AtomLGFX.set_text_size_xy(port, scale_x, scale_y, 0),
         :ok <- AtomLGFX.set_text_color(port, color, nil, 0) do
      :ok
    end
  end

  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
