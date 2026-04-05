# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.BatchSmoke do
  @moduledoc false

  alias AtomLGFX.Batch
  alias AtomLGFX.Batch.Command

  @bg 0x0000
  @panel_fill 0x1184
  @fg 0xFFFF
  @dim 0xA514
  @accent 0x07FF
  @frame 0x4208

  @ascii_preset 0

  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    clip_x = max_i(8, div(w, 8))
    clip_y = max_i(24, div(h, 6))
    clip_w = max_i(16, w - clip_x * 2)
    clip_h = max_i(16, h - clip_y * 2)

    inner_x = clip_x + 8
    inner_y = clip_y + 8
    inner_w = max_i(16, clip_w - 16)
    inner_h = max_i(16, clip_h - 16)

    status_y = max_i(0, h - 14)

    with :ok <- expect_empty_batch_rejected(port),
         {:ok, batch} <-
           build_lcd_batch(
             w,
             h,
             clip_x,
             clip_y,
             clip_w,
             clip_h,
             inner_x,
             inner_y,
             inner_w,
             inner_h
           ),
         :ok <- submit_batch_ok(port, batch),
         :ok <- draw_sync_status(port, w, status_y) do
      IO.puts("batch_smoke ok")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("batch_smoke failed: #{AtomLGFX.format_error(reason)}")
        err
    end
  end

  defp expect_empty_batch_rejected(port) do
    case AtomLGFX.submit_batch(port, []) do
      {:error, :empty_batch} ->
        :ok

      {:error, reason} ->
        {:error, {:unexpected_empty_batch_error, reason}}

      {:ok, payload} ->
        {:error, {:empty_batch_unexpected_success, payload}}
    end
  end

  defp build_lcd_batch(
         w,
         h,
         clip_x,
         clip_y,
         clip_w,
         clip_h,
         inner_x,
         inner_y,
         inner_w,
         inner_h
       ) do
    center_x = div(w - 1, 2)
    center_y = div(h - 1, 2)

    inner_radius = max_i(2, div(min_i(inner_w, inner_h), 8))
    circle_radius = max_i(3, div(min_i(inner_w, inner_h), 6))
    circle_fill_radius = max_i(2, div(circle_radius, 2))

    tri_inset = max_i(2, div(min_i(inner_w, inner_h), 10))
    bezier_lift = max_i(2, div(inner_h, 5))
    inner_right = inner_x + inner_w - 1
    inner_bottom = inner_y + inner_h - 1

    tri_x0 = center_x
    tri_y0 = inner_y + tri_inset
    tri_x1 = inner_x + tri_inset
    tri_y1 = inner_bottom - tri_inset
    tri_x2 = inner_right - tri_inset
    tri_y2 = inner_bottom - tri_inset

    batch0 = Batch.new()

    with {:ok, batch1} <- add_batch_entry(batch0, Command.clear(@bg)),
         {:ok, batch2} <-
           add_batch_entry(
             batch1,
             Command.new(:setClipRect, 0, 0, [clip_x, clip_y, clip_w, clip_h])
           ),
         {:ok, batch3} <- add_batch_entry(batch2, Command.fill_rect(0, 0, w, h, @panel_fill)),
         {:ok, batch4} <-
           add_batch_entry(batch3, Command.line(0, center_y, w - 1, center_y, @accent)),
         {:ok, batch5} <-
           add_batch_entry(batch4, Command.line(center_x, 0, center_x, h - 1, @accent)),
         {:ok, batch6} <- add_batch_entry(batch5, Command.line(0, 0, w - 1, h - 1, @accent)),
         {:ok, batch7} <- add_batch_entry(batch6, Command.line(0, h - 1, w - 1, 0, @accent)),
         {:ok, batch8} <-
           add_batch_entry(
             batch7,
             Command.fill_circle(center_x, center_y, circle_fill_radius, @bg)
           ),
         {:ok, batch9} <-
           add_batch_entry(batch8, Command.draw_circle(center_x, center_y, circle_radius, @fg)),
         {:ok, batch10} <- add_batch_entry(batch9, Command.new(:clearClipRect, 0, 0, [])),
         {:ok, batch11} <-
           add_batch_entry(batch10, Command.draw_rect(clip_x, clip_y, clip_w, clip_h, @frame)),
         {:ok, batch12} <- add_batch_entry(batch11, Command.new(:setTextWrap, 0, 0, [false])),
         {:ok, batch13} <-
           add_batch_entry(batch12, Command.new(:setTextFontPreset, 0, 0, [@ascii_preset])),
         {:ok, batch14} <- add_batch_entry(batch13, Command.new(:setTextSize, 0, 0, [1])),
         {:ok, batch15} <- add_batch_entry(batch14, Command.new(:setTextColor, 0, 0, [@fg])),
         {:ok, batch16} <-
           add_batch_entry(
             batch15,
             Command.fill_round_rect(inner_x, inner_y, inner_w, inner_h, inner_radius, @bg)
           ),
         {:ok, batch17} <-
           add_batch_entry(
             batch16,
             Command.line(inner_x, inner_y, inner_right, inner_bottom, @fg)
           ),
         {:ok, batch18} <-
           add_batch_entry(
             batch17,
             Command.line(inner_x, inner_bottom, inner_right, inner_y, @fg)
           ),
         {:ok, batch19} <-
           add_batch_entry(
             batch18,
             Command.draw_round_rect(inner_x, inner_y, inner_w, inner_h, inner_radius, @frame)
           ),
         {:ok, batch20} <-
           add_batch_entry(
             batch19,
             Command.fill_triangle(tri_x0, tri_y0, tri_x1, tri_y1, tri_x2, tri_y2, @dim)
           ),
         {:ok, batch21} <-
           add_batch_entry(
             batch20,
             Command.draw_triangle(tri_x0, tri_y0, tri_x1, tri_y1, tri_x2, tri_y2, @accent)
           ),
         {:ok, batch22} <-
           add_batch_entry(
             batch21,
             Command.draw_bezier3(
               inner_x + tri_inset,
               center_y,
               center_x,
               inner_y + tri_inset,
               inner_right - tri_inset,
               center_y,
               @fg
             )
           ),
         {:ok, batch23} <-
           add_batch_entry(
             batch22,
             Command.draw_bezier4(
               inner_x + tri_inset,
               center_y + bezier_lift,
               inner_x + div(inner_w, 3),
               inner_bottom - tri_inset,
               inner_x + div(inner_w * 2, 3),
               inner_y + tri_inset,
               inner_right - tri_inset,
               center_y + bezier_lift,
               @accent
             )
           ),
         {:ok, batch24} <- add_batch_entry(batch23, Command.draw_pixel(center_x, center_y, @fg)) do
      {:ok, batch24}
    end
  end

  defp add_batch_entry(%Batch{} = batch, {:ok, command}) do
    add_batch_entry(batch, command)
  end

  defp add_batch_entry(%Batch{} = batch, {:cmd, _, _, _, _} = command) do
    case Batch.add(batch, command) do
      %Batch{} = next_batch -> {:ok, next_batch}
      {:error, _reason} = error -> error
    end
  end

  defp add_batch_entry(%Batch{}, {:error, _reason} = error), do: error

  defp submit_batch_ok(port, %Batch{} = batch) do
    case AtomLGFX.submit_batch(port, batch) do
      {:ok, _payload} ->
        :ok

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp draw_sync_status(port, w, status_y) do
    with :ok <- AtomLGFX.reset_text_state(port, 0),
         :ok <- AtomLGFX.set_text_wrap(port, false, 0),
         :ok <- AtomLGFX.set_text_font_preset(port, :ascii, 0),
         :ok <- AtomLGFX.set_text_size(port, 1, 0),
         :ok <- AtomLGFX.set_text_color(port, @fg, nil, 0),
         :ok <- AtomLGFX.draw_rect(port, 0, 0, w, status_y + 14, @frame, 0),
         :ok <- AtomLGFX.draw_string_bg(port, 4, 2, @fg, @bg, 1, "BATCH SMOKE", 0),
         :ok <- AtomLGFX.set_text_color(port, @dim, nil, 0),
         :ok <- AtomLGFX.draw_string(port, 4, 14, "submit_batch ok / lcd inline path ok", 0),
         :ok <-
           AtomLGFX.draw_string(port, 4, status_y, "batch is explicit; sync path still works", 0) do
      :ok
    end
  end

  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
