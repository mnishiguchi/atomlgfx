# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.PushImageStress do
  @moduledoc false

  import SampleApp.AtomVMCompat, only: [yield: 0]

  alias AtomLGFX.Color

  @bg 0x1082
  @hud_bg 0x18C3
  @hud_fg 0xFFFF
  @hud_dim 0xB596
  @hud_ok 0xD69A
  @hud_err 0x738E
  @divider 0x528A
  @stage_bg 0x0841
  @progress_bg 0x2945

  @hud_h 40
  @hud_x 4
  @hud_line1_y 2
  @hud_line2_y 12
  @hud_text_scale 1
  @hud_bar_y 26
  @hud_bar_h 6

  @solid_a Color.white()
  @solid_b Color.yellow()

  @progress_every 25
  @gc_every 16

  def run(port, rounds \\ 300) when is_integer(rounds) and rounds > 0 do
    try do
      with {:ok, w} <- AtomLGFX.width(port, 0),
           {:ok, h} <- AtomLGFX.height(port, 0),
           :ok <- AtomLGFX.set_swap_bytes(port, true, 0) do
        hud_h = min_i(@hud_h, h)
        stage_y = hud_h
        stage_h = max_i(1, h - hud_h)

        IO.puts("push_image_stress start w=#{w} h=#{h} rounds=#{rounds}")

        draw_chrome(port, w, h, stage_y)
        draw_status(port, w, rounds, -1, 0, 0, {0, 0}, 0)

        :ok = invalid_stride_probe(port)

        cases = build_cases(w, stage_h)
        {ok_count, err_count} = run_i(port, rounds, 0, w, stage_y, stage_h, cases, 0, 0)

        draw_status(
          port,
          w,
          rounds,
          rounds - 1,
          ok_count,
          err_count,
          pick_case(cases, max_i(0, rounds - 1)),
          choose_stride_pixels(max_i(0, rounds - 1), elem(pick_case(cases, max_i(0, rounds - 1)), 0))
        )

        IO.puts("push_image_stress done rounds=#{rounds} ok=#{ok_count} err=#{err_count}")

        if err_count == 0 do
          :ok
        else
          {:error, {:push_image_stress_failed, err_count}}
        end
      end
    after
      _ = AtomLGFX.set_swap_bytes(port, false, 0)
    end
  end

  defp run_i(_port, rounds, i, _screen_w, _stage_y, _stage_h, _cases, ok_count, err_count)
       when i >= rounds do
    {ok_count, err_count}
  end

  defp run_i(port, rounds, i, screen_w, stage_y, stage_h, cases, ok_count, err_count) do
    {w, h} = pick_case(cases, i)
    x = pick_x(i, screen_w, w)
    y = stage_y + pick_y(i, stage_h, h)

    stride_pixels = choose_stride_pixels(i, w)
    pixels = make_pixels_rgb565(w, h, stride_pixels, i)

    result = AtomLGFX.push_image_rgb565(port, x, y, w, h, pixels, stride_pixels, 0)

    {ok_count2, err_count2} =
      case result do
        :ok ->
          {ok_count + 1, err_count}

        {:error, reason} ->
          IO.puts(
            "push_image_stress error i=#{i} x=#{x} y=#{y} w=#{w} h=#{h} stride=#{stride_pixels} reason=#{AtomLGFX.format_error(reason)}"
          )

          {ok_count, err_count + 1}
      end

    if should_log_progress?(i, rounds) do
      IO.puts("push_image_stress progress i=#{i}/#{rounds} ok=#{ok_count2} err=#{err_count2}")
      draw_status(port, screen_w, rounds, i, ok_count2, err_count2, {w, h}, stride_pixels)
    end

    if rem(i, @gc_every) == 0 do
      :erlang.garbage_collect()
    end

    yield()

    run_i(port, rounds, i + 1, screen_w, stage_y, stage_h, cases, ok_count2, err_count2)
  end

  defp should_log_progress?(i, rounds) do
    i == 0 or i == rounds - 1 or rem(i, @progress_every) == 0
  end

  defp draw_chrome(port, w, h, stage_y) do
    _ = AtomLGFX.fill_screen(port, @bg)

    if stage_y > 0 do
      _ = AtomLGFX.fill_rect(port, 0, 0, w, stage_y, @hud_bg)
      _ = AtomLGFX.draw_fast_hline(port, 0, stage_y - 1, w, @divider)
    end

    if h > stage_y do
      _ = AtomLGFX.fill_rect(port, 0, stage_y, w, h - stage_y, @stage_bg)
    end

    :ok
  end

  defp draw_status(port, screen_w, rounds, i, ok_count, err_count, {case_w, case_h}, stride_pixels) do
    hud_h = @hud_h
    bar_x = @hud_x
    bar_y = @hud_bar_y
    bar_h = @hud_bar_h
    bar_w = max_i(8, screen_w - 2 * @hud_x)

    _ = AtomLGFX.fill_rect(port, 0, 0, screen_w, hud_h, @hud_bg)

    line1 =
      <<"IMG ", progress_label(i, rounds)::binary, "  ok:", i2b(ok_count)::binary, "  err:",
        i2b(err_count)::binary>>

    line2 =
      <<i2b(case_w)::binary, "x", i2b(case_h)::binary, "  st:",
        stride_label(stride_pixels, case_w)::binary, "  solid">>

    _ =
      AtomLGFX.draw_string_bg(
        port,
        @hud_x,
        @hud_line1_y,
        @hud_fg,
        @hud_bg,
        @hud_text_scale,
        line1
      )

    _ =
      AtomLGFX.draw_string_bg(
        port,
        @hud_x,
        @hud_line2_y,
        @hud_dim,
        @hud_bg,
        @hud_text_scale,
        line2
      )

    _ = AtomLGFX.fill_rect(port, bar_x, bar_y, bar_w, bar_h, @progress_bg)

    fill_w =
      cond do
        rounds <= 0 -> 0
        i < 0 -> 0
        true -> min_i(bar_w, div((i + 1) * bar_w, rounds))
      end

    bar_color =
      if err_count == 0 do
        @hud_ok
      else
        @hud_err
      end

    if fill_w > 0 do
      _ = AtomLGFX.fill_rect(port, bar_x, bar_y, fill_w, bar_h, bar_color)
    end

    :ok
  end

  defp progress_label(i, rounds) do
    if i < 0 do
      <<"0/", i2b(rounds)::binary>>
    else
      <<i2b(i + 1)::binary, "/", i2b(rounds)::binary>>
    end
  end

  defp stride_label(0, _w), do: "tight"
  defp stride_label(stride, w) when stride == w, do: "w"
  defp stride_label(stride, w), do: <<i2b(stride)::binary, "(+", i2b(stride - w)::binary, ")">>

  defp build_cases(screen_w, stage_h) do
    c1 = clamp_case(1, 1, screen_w, stage_h)
    c2 = clamp_case(2, 2, screen_w, stage_h)
    c3 = clamp_case(3, 5, screen_w, stage_h)
    c4 = clamp_case(7, 9, screen_w, stage_h)
    c5 = clamp_case(16, 16, screen_w, stage_h)
    c6 = clamp_case(32, 32, screen_w, stage_h)
    c7 = clamp_case(100, 37, screen_w, stage_h)
    c8 = clamp_case(screen_w, 1, screen_w, stage_h)
    c9 = clamp_case(1, min_i(288, stage_h), screen_w, stage_h)
    c10 = clamp_case(screen_w, 8, screen_w, stage_h)
    c11 = clamp_case(screen_w, 32, screen_w, stage_h)
    c12 = clamp_case(160, 64, screen_w, stage_h)

    {c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12}
  end

  defp clamp_case(w, h, screen_w, screen_h) do
    {max_i(1, min_i(w, screen_w)), max_i(1, min_i(h, screen_h))}
  end

  defp pick_case(cases, i) do
    idx = rem(i, tuple_size(cases))
    elem(cases, idx)
  end

  defp pick_x(i, screen_w, w) do
    max_x = max_i(0, screen_w - w)

    if max_x == 0 do
      0
    else
      rem(i * 37 + 11, max_x + 1)
    end
  end

  defp pick_y(i, stage_h, h) do
    max_y = max_i(0, stage_h - h)

    if max_y == 0 do
      0
    else
      rem(i * 53 + 7, max_y + 1)
    end
  end

  defp choose_stride_pixels(i, w) do
    case rem(i, 4) do
      0 -> 0
      1 -> w
      2 -> w + 1
      _ -> w + 3
    end
  end

  # Raw pushImage payloads use ordinary RGB565 data encoded as little-endian
  # 16-bit words. This stress test keeps the payload simple on purpose:
  # one solid color, optional stride padding, repeated many times.
  defp make_pixels_rgb565(w, h, stride_pixels0, iter) do
    stride =
      case stride_pixels0 do
        0 -> w
        s -> s
      end

    color = solid_color(iter)
    row = solid_row(w, color)
    pad_pixels = stride - w

    pad_bin =
      if pad_pixels > 0 do
        :binary.copy(Color.rgb565_le(color), pad_pixels)
      else
        <<>>
      end

    full_row = <<row::binary, pad_bin::binary>>
    :binary.copy(full_row, h)
  end

  defp solid_color(iter) do
    if rem(iter, 2) == 0 do
      @solid_a
    else
      @solid_b
    end
  end

  defp solid_row(w, color565) do
    :binary.copy(Color.rgb565_le(color565), w)
  end

  defp invalid_stride_probe(port) do
    w = 4
    h = 2
    stride = 3
    pixels = solid_row(w, @solid_a) |> :binary.copy(h)

    case AtomLGFX.push_image_rgb565(port, 0, 0, w, h, pixels, stride, 0) do
      {:error, {:bad_stride, ^stride, ^w}} ->
        IO.puts("invalid_stride_probe ok")
        :ok

      {:error, reason} ->
        IO.puts("invalid_stride_probe unexpected_error=#{AtomLGFX.format_error(reason)}")
        :ok

      :ok ->
        IO.puts("invalid_stride_probe unexpected_ok")
        :ok
    end
  end

  defp i2b(i), do: :erlang.integer_to_binary(i)

  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
