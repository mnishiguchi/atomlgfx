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
    batch0 = Batch.new()

    with {:ok, batch1} <- add_command(batch0, :fillScreen, 0, 0, [@bg]),
         {:ok, batch2} <-
           add_command(batch1, :setClipRect, 0, 0, [clip_x, clip_y, clip_w, clip_h]),
         {:ok, batch3} <- add_command(batch2, :fillRect, 0, 0, [0, 0, w, h, @panel_fill]),
         {:ok, batch4} <- add_command(batch3, :drawLine, 0, 0, [0, 0, w - 1, h - 1, @accent]),
         {:ok, batch5} <- add_command(batch4, :drawLine, 0, 0, [0, h - 1, w - 1, 0, @accent]),
         {:ok, batch6} <- add_command(batch5, :clearClipRect, 0, 0, []),
         {:ok, batch7} <- add_command(batch6, :setTextWrap, 0, 0, [false]),
         {:ok, batch8} <- add_command(batch7, :setTextFontPreset, 0, 0, [@ascii_preset]),
         {:ok, batch9} <- add_command(batch8, :setTextSize, 0, 0, [1]),
         {:ok, batch10} <- add_command(batch9, :setTextColor, 0, 0, [@fg]),
         {:ok, batch11} <-
           add_command(batch10, :fillRect, 0, 0, [inner_x, inner_y, inner_w, inner_h, @bg]),
         {:ok, batch12} <-
           add_command(
             batch11,
             :drawLine,
             0,
             0,
             [inner_x, inner_y, inner_x + inner_w - 1, inner_y + inner_h - 1, @fg]
           ),
         {:ok, batch13} <-
           add_command(
             batch12,
             :drawLine,
             0,
             0,
             [inner_x, inner_y + inner_h - 1, inner_x + inner_w - 1, inner_y, @fg]
           ) do
      {:ok, batch13}
    end
  end

  defp add_command(batch, op, target, flags, args) do
    case Command.new(op, target, flags, args) do
      {:ok, command} ->
        case Batch.add(batch, command) do
          %Batch{} = next_batch -> {:ok, next_batch}
          {:error, _reason} = error -> error
        end

      {:error, _reason} = error ->
        error
    end
  end

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

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
