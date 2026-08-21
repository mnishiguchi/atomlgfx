# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.NifSmoke do
  @moduledoc false

  @rounds 120

  def run do
    IO.puts("nif_smoke start")

    try do
      with :ok <- LGFX.init(),
           :ok <- LGFX.set_rotation(1),
           :ok <- LGFX.set_brightness(255),
           w when is_integer(w) and w > 0 <- LGFX.width(),
           h when is_integer(h) and h > 0 <- LGFX.height(),
           :ok <- smoke(w, h),
           :ok <- error_path_smoke(),
           :ok <- perf_smoke(w, h) do
        IO.puts("nif_smoke ok viewport=#{w}x#{h}")
        :ok
      else
        other ->
          IO.puts("nif_smoke failed result=#{inspect(other)}")
          {:error, {:nif_smoke_failed, other}}
      end
    after
      IO.puts("nif_smoke close=#{inspect(LGFX.close())}")
    end
  end

  defp smoke(w, h) do
    cx = div(w, 2)
    cy = div(h, 2)

    with :ok <- write_session(fn -> draw_direct(w, h, cx, cy) end),
         :ok <- LGFX.display(),
         :ok <- LGFX.push_image(8, h - 16, 8, 8, checker_pixels()),
         :ok <-
           LGFX.batch(
             [
               {:fill_rect, w - 92, 8, 84, 28, 0x000F},
               {:draw_rect, w - 92, 8, 84, 28, :white},
               {:set_text_color, :white},
               {:draw_string, "NIF BATCH", w - 86, 17}
             ],
             display: true
           ) do
      IO.puts("nif_api_smoke ok")
      :ok
    end
  end

  defp draw_direct(w, h, cx, cy) do
    with :ok <- LGFX.clear(:black),
         :ok <- LGFX.fill_screen(:black),
         :ok <- LGFX.draw_pixel(cx, cy, :white),
         :ok <- LGFX.draw_fast_hline(8, 42, w - 16, :cyan),
         :ok <- LGFX.draw_fast_vline(cx, 48, max(8, h - 96), :cyan),
         :ok <- LGFX.draw_line(8, 48, w - 9, h - 24, :yellow),
         :ok <- LGFX.draw_rect(12, 52, 72, 42, :red),
         :ok <- LGFX.fill_rect(18, 58, 60, 30, 0x7800),
         :ok <- LGFX.draw_round_rect(92, 52, 72, 42, 8, :green),
         :ok <- LGFX.fill_round_rect(98, 58, 60, 30, 6, 0x03E0),
         :ok <- LGFX.draw_circle(cx, cy, 28, :blue),
         :ok <- LGFX.fill_circle(cx, cy, 18, 0x000F),
         :ok <- LGFX.draw_ellipse(w - 64, cy, 32, 20, :magenta),
         :ok <- LGFX.fill_ellipse(w - 64, cy, 20, 12, 0x780F),
         :ok <- LGFX.draw_arc(cx, h - 64, 22, 32, -45, 225, 0xFD20),
         :ok <- LGFX.fill_arc(cx, h - 64, 10, 18, 30, 300, 0x7BE0),
         :ok <- LGFX.draw_triangle(16, h - 56, 48, h - 24, 80, h - 56, :white),
         :ok <- LGFX.fill_triangle(24, h - 52, 48, h - 30, 72, h - 52, 0x03EF),
         :ok <- LGFX.set_cursor(8, 8),
         :ok <- LGFX.set_text_size(1),
         :ok <- LGFX.set_text_datum(:top_left),
         :ok <- LGFX.set_text_color(:white, :black),
         :ok <- LGFX.print("LGFX NIF"),
         :ok <- LGFX.println(" direct"),
         :ok <- LGFX.draw_string("simple + fast", 8, 24) do
      :ok
    end
  end

  defp write_session(fun) when is_function(fun, 0) do
    with :ok <- LGFX.start_write() do
      try do
        fun.()
      after
        _ = LGFX.end_write()
      end
    end
  end

  defp error_path_smoke do
    case LGFX.fill_rect(0, 0, -1, 1, :white) do
      {:error, :badarg} ->
        IO.puts("nif_error_path ok")
        :ok

      other ->
        {:error, {:unexpected_nif_error_result, other}}
    end
  end

  defp perf_smoke(w, h) do
    commands = build_fill_rect_commands(@rounds, w, h, [])

    with {:ok, batch, encode_us} <- encode_timed(commands),
         {:ok, direct_us} <- timed(fn -> draw_direct_rects(commands) end),
         :ok <- LGFX.fill_screen(:black),
         {:ok, batch_us} <- timed(fn -> LGFX.submit_batch(batch) end) do
      report_perf("encode_fill_rect_batch", @rounds, encode_us)
      report_perf("direct_fill_rect", @rounds, direct_us)
      report_perf("submit_fill_rect_batch", @rounds, batch_us)
      IO.puts("nif_perf speedup_x100=#{div(direct_us * 100, max(1, batch_us))}")
      :ok
    end
  end

  defp build_fill_rect_commands(0, _w, _h, acc), do: :lists.reverse(acc)

  defp build_fill_rect_commands(remaining, w, h, acc) do
    index = remaining - 1
    rect_w = max(4, div(w, 12))
    rect_h = max(4, div(h, 12))
    x = rem(index * 7, max(1, w - rect_w))
    y = rem(index * 11, max(1, h - rect_h))
    color = :lists.nth(rem(index, 6) + 1, [:white, :red, :green, :blue, :yellow, :cyan])

    build_fill_rect_commands(
      remaining - 1,
      w,
      h,
      [{:fill_rect, x, y, rect_w, rect_h, color} | acc]
    )
  end

  defp draw_direct_rects([]), do: :ok

  defp draw_direct_rects([{:fill_rect, x, y, w, h, color} | rest]) do
    with :ok <- LGFX.fill_rect(x, y, w, h, color) do
      draw_direct_rects(rest)
    end
  end

  defp timed(fun) when is_function(fun, 0) do
    start_us = :erlang.monotonic_time(:microsecond)

    case fun.() do
      :ok -> {:ok, max(1, :erlang.monotonic_time(:microsecond) - start_us)}
      other -> {:error, {:timed_operation_failed, other}}
    end
  end

  defp encode_timed(commands) do
    start_us = :erlang.monotonic_time(:microsecond)

    case LGFX.encode_batch(commands) do
      {:ok, batch} ->
        {:ok, batch, max(1, :erlang.monotonic_time(:microsecond) - start_us)}

      other ->
        {:error, {:batch_encoding_failed, other}}
    end
  end

  defp report_perf(label, commands, elapsed_us) do
    per_command_us = div(elapsed_us, commands)
    commands_per_sec = div(commands * 1_000_000, elapsed_us)

    IO.puts(
      "NIF_PERF label=#{label} commands=#{commands} elapsed_us=#{elapsed_us} per_command_us=#{per_command_us} commands_per_sec=#{commands_per_sec}"
    )
  end

  defp checker_pixels do
    pixels = [
      0xF800,
      0x001F,
      0xF800,
      0x001F,
      0x001F,
      0xF800,
      0x001F,
      0xF800
    ]

    pixels
    |> List.duplicate(8)
    |> List.flatten()
    |> AtomLGFX.Color.pixels_le()
  end
end
