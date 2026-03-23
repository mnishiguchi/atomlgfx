# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.PushImageStress do
  @moduledoc false

  import SampleApp.AtomVMCompat, only: [yield: 0]

  @bg 0x101010
  @hud_bg 0x181818
  @hud_fg 0xFFFFFF
  @hud_dim 0xB0B0B0
  @hud_accent 0x909090
  @hud_ok 0xD0D0D0
  @hud_err 0x707070
  @divider 0x505050
  @stage_bg 0x080808
  @progress_bg 0x2A2A2A

  @hud_h 40
  @hud_x 4
  @hud_line1_y 2
  @hud_line2_y 12
  @hud_text_scale 1
  @hud_bar_y 26
  @hud_bar_h 6

  @poison565 0xF81F

  # Progress log interval (also forces GC on this cadence)
  @progress_every 25
  @gc_every 16

  # 3 simple patterns are enough to catch obvious corruption/stride issues.
  @pattern_solid 0
  @pattern_stripes 1
  @pattern_checker 2

  def run(port, rounds \\ 300) when is_integer(rounds) and rounds > 0 do
    with {:ok, w} <- AtomLGFX.width(port, 0),
         {:ok, h} <- AtomLGFX.height(port, 0) do
      hud_h = min_i(@hud_h, h)
      stage_y = hud_h
      stage_h = max_i(1, h - hud_h)

      IO.puts("push_image_stress start w=#{w} h=#{h} rounds=#{rounds}")

      draw_chrome(port, w, h, stage_y)
      draw_status(port, w, rounds, -1, 0, 0, {0, 0}, 0, @pattern_solid)

      :ok = invalid_stride_probe(port)

      cases = build_cases(w, stage_h)

      {ok_count, err_count} =
        run_i(port, rounds, 0, w, stage_y, stage_h, cases, 0, 0)

      draw_status(
        port,
        w,
        rounds,
        rounds - 1,
        ok_count,
        err_count,
        pick_case(cases, max_i(0, rounds - 1)),
        choose_stride_pixels(
          max_i(0, rounds - 1),
          elem(pick_case(cases, max_i(0, rounds - 1)), 0)
        ),
        rem(max_i(0, rounds - 1), 3)
      )

      IO.puts("push_image_stress done rounds=#{rounds} ok=#{ok_count} err=#{err_count}")

      if err_count == 0 do
        :ok
      else
        {:error, {:push_image_stress_failed, err_count}}
      end
    end
  end

  # ---------------------------------------------------------------------------
  # Main loop
  # ---------------------------------------------------------------------------

  defp run_i(_port, rounds, i, _screen_w, _stage_y, _stage_h, _cases, ok_count, err_count)
       when i >= rounds do
    {ok_count, err_count}
  end

  defp run_i(port, rounds, i, screen_w, stage_y, stage_h, cases, ok_count, err_count) do
    {w, h} = pick_case(cases, i)
    x = pick_x(i, screen_w, w)
    y = stage_y + pick_y(i, stage_h, h)

    stride_pixels = choose_stride_pixels(i, w)
    pattern_id = rem(i, 3)

    pixels = make_pixels_rgb565(w, h, stride_pixels, pattern_id, i)

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

      draw_status(
        port,
        screen_w,
        rounds,
        i,
        ok_count2,
        err_count2,
        {w, h},
        stride_pixels,
        pattern_id
      )
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

  # ---------------------------------------------------------------------------
  # HUD / UI
  # ---------------------------------------------------------------------------

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

  defp draw_status(
         port,
         screen_w,
         rounds,
         i,
         ok_count,
         err_count,
         {case_w, case_h},
         stride_pixels,
         pattern_id
       ) do
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
        stride_label(stride_pixels, case_w)::binary, "  ", pattern_label(pattern_id)::binary>>

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

    if fill_w < bar_w do
      tick_x = bar_x + fill_w
      _ = AtomLGFX.draw_fast_vline(port, tick_x, bar_y, bar_h, @hud_accent)
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

  defp pattern_label(@pattern_solid), do: "solid"
  defp pattern_label(@pattern_stripes), do: "stripe"
  defp pattern_label(@pattern_checker), do: "check"
  defp pattern_label(_), do: "?"

  defp stride_label(0, _w), do: "tight"
  defp stride_label(stride, w) when stride == w, do: "w"
  defp stride_label(stride, w), do: <<i2b(stride)::binary, "(+", i2b(stride - w)::binary, ")">>

  # ---------------------------------------------------------------------------
  # Case matrix
  # ---------------------------------------------------------------------------

  defp build_cases(screen_w, stage_h) do
    # Keep coverage broad, but avoid giant allocations on AtomVM.
    c1 = clamp_case(1, 1, screen_w, stage_h)
    c2 = clamp_case(2, 2, screen_w, stage_h)
    c3 = clamp_case(3, 5, screen_w, stage_h)
    c4 = clamp_case(7, 9, screen_w, stage_h)
    c5 = clamp_case(16, 16, screen_w, stage_h)
    c6 = clamp_case(32, 32, screen_w, stage_h)
    c7 = clamp_case(100, 37, screen_w, stage_h)
    c8 = clamp_case(screen_w, 1, screen_w, stage_h)
    c9 = clamp_case(1, min_i(288, stage_h), screen_w, stage_h)

    # Previously these were too large for the VM heap when repeatedly generated.
    c10 = clamp_case(screen_w, 8, screen_w, stage_h)
    c11 = clamp_case(screen_w, 32, screen_w, stage_h)

    # One moderate rectangle to keep area coverage
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

  # ---------------------------------------------------------------------------
  # Position / stride selection
  # ---------------------------------------------------------------------------

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

  # ---------------------------------------------------------------------------
  # Pixel generation (AtomVM-friendly)
  # ---------------------------------------------------------------------------

  # Returns a binary laid out with the requested stride.
  # If stride_pixels0 == 0, the returned binary is tightly packed (stride = w).
  defp make_pixels_rgb565(w, h, stride_pixels0, pattern_id, iter) do
    stride =
      case stride_pixels0 do
        0 -> w
        s -> s
      end

    pad_pixels = stride - w

    visible_row =
      case pattern_id do
        @pattern_solid ->
          solid_row(w, solid_color(iter))

        @pattern_stripes ->
          stripes_row(w, stripes_colors(iter))

        _ ->
          checker_row(w, 0)
      end

    pad_bin =
      if pad_pixels > 0 do
        :binary.copy(<<@poison565::16-big>>, pad_pixels)
      else
        <<>>
      end

    case pattern_id do
      @pattern_checker ->
        row_a = <<checker_row(w, rem(iter, 2))::binary, pad_bin::binary>>
        row_b = <<checker_row(w, rem(iter + 1, 2))::binary, pad_bin::binary>>
        alternating_rows_binary(h, row_a, row_b)

      _ ->
        row = <<visible_row::binary, pad_bin::binary>>
        :binary.copy(row, h)
    end
  end

  defp solid_color(iter) do
    case rem(iter, 6) do
      0 -> 0xF800
      1 -> 0x07E0
      2 -> 0x001F
      3 -> 0xFFE0
      4 -> 0xF81F
      _ -> 0x07FF
    end
  end

  defp stripes_colors(iter) do
    case rem(iter, 3) do
      0 -> {0xFFFF, 0x0000}
      1 -> {0xFFE0, 0x001F}
      _ -> {0x07FF, 0x780F}
    end
  end

  defp solid_row(w, color565) do
    :binary.copy(<<color565::16-big>>, w)
  end

  # 2-pixel vertical stripes: A A B B A A B B ...
  defp stripes_row(w, {c1, c2}) do
    group = <<c1::16-big, c1::16-big, c2::16-big, c2::16-big>>
    groups = div(w, 4)
    rem_pixels = rem(w, 4)
    base = :binary.copy(group, groups)

    tail =
      case rem_pixels do
        0 -> <<>>
        1 -> <<c1::16-big>>
        2 -> <<c1::16-big, c1::16-big>>
        _ -> <<c1::16-big, c1::16-big, c2::16-big>>
      end

    <<base::binary, tail::binary>>
  end

  # 1-pixel checker row: A B A B ... (phase flips starting color)
  defp checker_row(w, phase) do
    {c_first, c_second} =
      if rem(phase, 2) == 0 do
        {0xFFFF, 0x0000}
      else
        {0x0000, 0xFFFF}
      end

    pair = <<c_first::16-big, c_second::16-big>>
    pairs = div(w, 2)
    rem_pixels = rem(w, 2)
    base = :binary.copy(pair, pairs)

    tail =
      if rem_pixels == 1 do
        <<c_first::16-big>>
      else
        <<>>
      end

    <<base::binary, tail::binary>>
  end

  defp alternating_rows_binary(h, row_a, row_b) do
    alternating_rows_iolist(h, row_a, row_b, 0, [])
    |> :lists.reverse()
    |> :erlang.iolist_to_binary()
  end

  defp alternating_rows_iolist(h, _row_a, _row_b, y, acc) when y >= h do
    acc
  end

  defp alternating_rows_iolist(h, row_a, row_b, y, acc) do
    row =
      if rem(y, 2) == 0 do
        row_a
      else
        row_b
      end

    alternating_rows_iolist(h, row_a, row_b, y + 1, [row | acc])
  end

  # ---------------------------------------------------------------------------
  # Probe cases
  # ---------------------------------------------------------------------------

  defp invalid_stride_probe(port) do
    # stride < w must be rejected by the Elixir client-side validation
    w = 4
    h = 2
    stride = 3
    pixels = solid_row(w, 0x07E0) |> :binary.copy(h)

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

  # ---------------------------------------------------------------------------
  # Tiny helpers (AtomVM-safe)
  # ---------------------------------------------------------------------------

  defp i2b(i), do: :erlang.integer_to_binary(i)

  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
