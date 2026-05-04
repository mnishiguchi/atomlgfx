# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.PerfSmoke do
  @moduledoc false

  alias AtomLGFX.BinaryBatch

  @default_rounds 120

  @bg 0x0000
  @fg 0xFFFF
  @muted 0x8410
  @ok 0x07E0
  @warmup_color 0x2104

  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    rounds = rounds()

    IO.puts("perf_smoke start viewport=#{w}x#{h} rounds=#{rounds}")

    {fill_rect_batch, fill_rect_build_us} =
      timed_value(fn -> build_fill_rect_binary_batch(rounds, w, h) end)

    {draw_line_batch, draw_line_build_us} =
      timed_value(fn -> build_draw_line_binary_batch(rounds, w, h) end)

    report_perf(
      "build_fill_rect_binary_batch",
      rounds,
      byte_size(fill_rect_batch),
      fill_rect_build_us
    )

    report_perf(
      "build_draw_line_binary_batch",
      rounds,
      byte_size(draw_line_batch),
      draw_line_build_us
    )

    with :ok <- warmup(port),
         :ok <-
           bench("direct_fill_rect", rounds, 0, fn ->
             direct_fill_rect_loop(port, rounds, w, h)
           end),
         :ok <-
           bench("binary_batch_fill_rect", rounds, byte_size(fill_rect_batch), fn ->
             AtomLGFX.submit_binary_batch(port, fill_rect_batch)
           end),
         :ok <-
           bench("direct_draw_line", rounds, 0, fn ->
             direct_draw_line_loop(port, rounds, w, h)
           end),
         :ok <-
           bench("binary_batch_draw_line", rounds, byte_size(draw_line_batch), fn ->
             AtomLGFX.submit_binary_batch(port, draw_line_batch)
           end),
         :ok <- draw_done(port, w, h, rounds) do
      IO.puts("perf_smoke done")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("perf_smoke failed: #{format_reason(reason)}")
        err

      other ->
        reason = {:unexpected_perf_result, :perf_smoke, other}
        IO.puts("perf_smoke failed: #{inspect(reason)}")
        {:error, reason}
    end
  end

  defp rounds do
    case :erlang.get(:sample_app_perf_rounds) do
      value when is_integer(value) and value > 0 -> value
      _other -> @default_rounds
    end
  end

  defp warmup(port) do
    warmup_batch =
      BinaryBatch.batch([
        BinaryBatch.target(0),
        BinaryBatch.fill_screen(@bg),
        BinaryBatch.fill_rect(0, 0, 8, 8, @warmup_color)
      ])

    with :ok <- run_step("warmup_direct_fill_screen", fn -> AtomLGFX.fill_screen(port, @bg) end),
         :ok <-
           run_step("warmup_binary_batch", fn ->
             AtomLGFX.submit_binary_batch(port, warmup_batch)
           end),
         :ok <- run_step("warmup_display", fn -> AtomLGFX.display(port) end) do
      :ok
    end
  end

  defp direct_fill_rect_loop(_port, 0, _w, _h), do: :ok

  defp direct_fill_rect_loop(port, remaining, w, h) do
    index = remaining - 1
    {x, y, rect_w, rect_h, color} = fill_rect_command(index, w, h)

    with :ok <-
           normalize_result(
             "direct_fill_rect_command",
             AtomLGFX.fill_rect(port, x, y, rect_w, rect_h, color)
           ) do
      direct_fill_rect_loop(port, remaining - 1, w, h)
    end
  end

  defp direct_draw_line_loop(_port, 0, _w, _h), do: :ok

  defp direct_draw_line_loop(port, remaining, w, h) do
    index = remaining - 1
    {x0, y0, x1, y1, color} = draw_line_command(index, w, h)

    with :ok <-
           normalize_result(
             "direct_draw_line_command",
             AtomLGFX.draw_line(port, x0, y0, x1, y1, color)
           ) do
      direct_draw_line_loop(port, remaining - 1, w, h)
    end
  end

  defp build_fill_rect_binary_batch(rounds, w, h) do
    rounds
    |> build_fill_rect_iodata(w, h, [])
    |> then(fn commands -> BinaryBatch.batch([BinaryBatch.target(0), commands]) end)
  end

  defp build_fill_rect_iodata(0, _w, _h, acc), do: :lists.reverse(acc)

  defp build_fill_rect_iodata(remaining, w, h, acc) do
    index = remaining - 1
    {x, y, rect_w, rect_h, color} = fill_rect_command(index, w, h)
    command = BinaryBatch.fill_rect(x, y, rect_w, rect_h, color)
    build_fill_rect_iodata(remaining - 1, w, h, [command | acc])
  end

  defp build_draw_line_binary_batch(rounds, w, h) do
    rounds
    |> build_draw_line_iodata(w, h, [])
    |> then(fn commands -> BinaryBatch.batch([BinaryBatch.target(0), commands]) end)
  end

  defp build_draw_line_iodata(0, _w, _h, acc), do: :lists.reverse(acc)

  defp build_draw_line_iodata(remaining, w, h, acc) do
    index = remaining - 1
    {x0, y0, x1, y1, color} = draw_line_command(index, w, h)
    command = BinaryBatch.draw_line(x0, y0, x1, y1, color)
    build_draw_line_iodata(remaining - 1, w, h, [command | acc])
  end

  defp fill_rect_command(index, w, h) do
    rect_w = max_i(4, div(max_i(w, 1), 12))
    rect_h = max_i(4, div(max_i(h, 1), 12))
    max_x = max_i(1, w - rect_w)
    max_y = max_i(1, h - rect_h)

    x = rem_i(index * 7, max_x)
    y = rem_i(index * 11, max_y)
    color = color_at(index)

    {x, y, rect_w, rect_h, color}
  end

  defp draw_line_command(index, w, h) do
    safe_w = max_i(w, 1)
    safe_h = max_i(h, 1)
    max_x = safe_w - 1
    max_y = safe_h - 1

    x0 = rem_i(index * 13, safe_w)
    y0 = rem_i(index * 5, safe_h)
    x1 = max_x - rem_i(index * 17, safe_w)
    y1 = max_y - rem_i(index * 3, safe_h)
    color = color_at(index)

    {x0, y0, x1, y1, color}
  end

  defp color_at(index) do
    case rem_i(index, 6) do
      0 -> 0xFFFF
      1 -> 0xF800
      2 -> 0x07E0
      3 -> 0x001F
      4 -> 0xFFE0
      _ -> 0x07FF
    end
  end

  defp bench(label, command_count, byte_count, fun) when is_function(fun, 0) do
    {result, elapsed_us} = timed_result(fun)

    case normalize_result(label, result) do
      :ok ->
        report_perf(label, command_count, byte_count, elapsed_us)

      {:error, reason} = err ->
        IO.puts("PERF_FAILED label=#{label} reason=#{format_reason(reason)}")
        err
    end
  end

  defp run_step(label, fun) when is_function(fun, 0) do
    case normalize_result(label, fun.()) do
      :ok ->
        IO.puts("PERF_STEP label=#{label} status=ok")
        :ok

      {:error, reason} = err ->
        IO.puts("PERF_STEP label=#{label} status=failed reason=#{format_reason(reason)}")
        err
    end
  end

  defp normalize_result(_label, :ok), do: :ok
  defp normalize_result(_label, {:ok, _result}), do: :ok
  defp normalize_result(_label, {:error, reason}), do: {:error, reason}
  defp normalize_result(label, other), do: {:error, {:unexpected_perf_result, label, other}}

  defp timed_value(fun) when is_function(fun, 0) do
    start_us = now_us()
    value = fun.()
    {value, elapsed_us(start_us)}
  end

  defp timed_result(fun) when is_function(fun, 0) do
    start_us = now_us()
    result = fun.()
    {result, elapsed_us(start_us)}
  end

  defp now_us do
    :erlang.monotonic_time(:microsecond)
  end

  defp elapsed_us(start_us) do
    max_i(1, now_us() - start_us)
  end

  defp report_perf(label, command_count, byte_count, elapsed_us) do
    elapsed = max_i(1, elapsed_us)
    safe_count = max_i(1, command_count)
    per_command_us = div(elapsed, safe_count)
    commands_per_sec = div(safe_count * 1_000_000, elapsed)

    IO.puts(
      "PERF label=#{label} commands=#{command_count} bytes=#{byte_count} elapsed_us=#{elapsed} per_command_us=#{per_command_us} commands_per_sec=#{commands_per_sec}"
    )

    :ok
  end

  defp draw_done(port, w, h, rounds) do
    with :ok <- AtomLGFX.reset_text_state(port, 0),
         :ok <- AtomLGFX.set_text_font_preset(port, :ascii, 0),
         :ok <- AtomLGFX.set_text_wrap(port, false, 0),
         :ok <- AtomLGFX.fill_rect(port, 0, 0, w, min_i(42, h), @bg, 0),
         :ok <- AtomLGFX.draw_string_bg(port, 8, 6, @fg, @bg, 2, "PERF", 0),
         :ok <-
           AtomLGFX.draw_string_bg(port, 8, 28, @ok, @bg, 1, "#{rounds} commands per path", 0),
         :ok <-
           AtomLGFX.draw_string_bg(
             port,
             max_i(96, div(w, 2)),
             28,
             @muted,
             @bg,
             1,
             "see serial log",
             0
           ) do
      :ok
    end
  end

  defp format_reason(reason) do
    AtomLGFX.format_error(reason)
  end

  defp rem_i(a, b) when b > 0 do
    rem(a, b)
  end

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b

  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b
end
