# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.DrawStringStress do
  @moduledoc false

  import SampleApp.AtomVMCompat, only: [yield: 0]

  @bg 0x101010
  @hud_bg 0x181818
  @hud_fg 0xFFFFFF
  @hud_dim 0xB0B0B0
  @hud_ok 0xC8C8C8
  @hud_err 0x707070
  @divider 0x505050

  @console_bg 0x0C0C0C
  @row_bg_a 0x141414
  @row_bg_b 0x1C1C1C
  @row_marker 0x606060

  @progress_bg 0x2A2A2A

  @hud_h 52
  @hud_title_y 2
  @hud_meta_y 16
  @hud_bar_y 34
  @hud_bar_h 8

  @main_text_scale 1
  @hud_title_scale 1
  @hud_meta_scale 1

  @line_h 18
  @x 4

  @progress_every 25
  @gc_every 32

  # Periodically emit a UTF-8 line to stress multi-byte transport under churn.
  @utf8_every 17

  # Exercise println occasionally while keeping the demo mostly deterministic.
  @println_every 19

  def run(port, rounds \\ 500) when is_integer(rounds) and rounds > 0 do
    with {:ok, w} <- LGFXPort.width(port, 0),
         {:ok, h} <- LGFXPort.height(port, 0) do
      hud_h = min_i(@hud_h, h)
      text_y0 = hud_h
      text_h = max_i(1, h - hud_h)
      rows = rows_for_height(text_h)

      IO.puts("draw_string_stress start w=#{w} h=#{h} rows=#{rows} rounds=#{rounds}")

      _ = draw_chrome(port, w, h, text_y0)

      # Keep state deterministic from this module's point of view.
      # Note: reset_text_state resets host cache only; device state remains whatever it was.
      _ = LGFXPort.reset_text_state(port, 0)
      _ = LGFXPort.set_text_wrap(port, false, 0)
      _ = LGFXPort.set_text_color(port, 0xFFFFFF, nil, 0)

      case choose_main_preset(port) do
        {:ok, main_preset} ->
          _ = LGFXPort.set_text_size(port, @main_text_scale, 0)
          _ = draw_status(port, w, rounds, -1, 0, 0, rows, main_preset)

          _ = probe_cursor_roundtrip(port, text_y0)
          _ = probe_invalid_text_guard(port)
          _ = probe_empty_prints(port, text_y0)

          do_run(port, w, rows, rounds, text_y0, main_preset, 0, 0, 0)

        {:error, {:failed_to_select_text_preset, jp_reason, ascii_reason}} = err ->
          IO.puts(
            "draw_string_stress failed to select text preset: " <>
              "jp=#{LGFXPort.format_error(jp_reason)} " <>
              "ascii=#{LGFXPort.format_error(ascii_reason)}"
          )

          err
      end
    else
      {:error, reason} = err ->
        IO.puts("draw_string_stress failed to start: #{LGFXPort.format_error(reason)}")
        err
    end
  end

  defp choose_main_preset(port) do
    case LGFXPort.set_text_font_preset(port, :jp, 0) do
      :ok ->
        IO.puts("draw_string_stress preset: :jp")
        {:ok, :jp}

      {:error, jp_reason} ->
        IO.puts("draw_string_stress preset :jp unavailable: #{LGFXPort.format_error(jp_reason)}")

        case LGFXPort.set_text_font_preset(port, :ascii, 0) do
          :ok ->
            IO.puts("draw_string_stress preset: :ascii")
            {:ok, :ascii}

          {:error, ascii_reason} ->
            IO.puts(
              "draw_string_stress preset :ascii unavailable: #{LGFXPort.format_error(ascii_reason)}"
            )

            {:error, {:failed_to_select_text_preset, jp_reason, ascii_reason}}
        end
    end
  end

  defp probe_cursor_roundtrip(port, text_y0) do
    probe_x = @x
    probe_y = text_y0

    with :ok <- LGFXPort.set_cursor(port, probe_x, probe_y, 0),
         {:ok, {^probe_x, ^probe_y}} <- LGFXPort.get_cursor(port, 0) do
      IO.puts("cursor_probe ok x=#{probe_x} y=#{probe_y}")
      :ok
    else
      {:ok, {x, y}} ->
        IO.puts("cursor_probe mismatch got=#{inspect({x, y})}")
        :ok

      {:error, reason} ->
        IO.puts("cursor_probe failed: #{LGFXPort.format_error(reason)}")
        :ok
    end
  end

  defp probe_invalid_text_guard(port) do
    # Client-side validation probe: empty binaries must be rejected for draw_string.
    case LGFXPort.draw_string(port, @x, 0, <<>>, 0) do
      {:error, :empty_text} ->
        IO.puts("invalid_text_probe ok (empty_text)")

      {:error, reason} ->
        IO.puts("invalid_text_probe ok (got #{LGFXPort.format_error(reason)})")

      :ok ->
        IO.puts("invalid_text_probe unexpected_ok")

      other ->
        IO.puts("invalid_text_probe unexpected_reply=#{inspect(other)}")
    end

    :ok
  end

  defp probe_empty_prints(port, text_y0) do
    _ = LGFXPort.set_cursor(port, @x, text_y0, 0)

    case LGFXPort.print(port, <<>>, 0) do
      :ok ->
        IO.puts("empty_print_probe ok")

      {:error, reason} ->
        IO.puts("empty_print_probe failed: #{LGFXPort.format_error(reason)}")

      other ->
        IO.puts("empty_print_probe unexpected_reply=#{inspect(other)}")
    end

    case LGFXPort.println(port, <<>>, 0) do
      :ok ->
        IO.puts("empty_println_probe ok")

      {:error, reason} ->
        IO.puts("empty_println_probe failed: #{LGFXPort.format_error(reason)}")

      other ->
        IO.puts("empty_println_probe unexpected_reply=#{inspect(other)}")
    end

    :ok
  end

  defp do_run(_port, _w, _rows, rounds, _text_y0, _main_preset, i, ok_count, err_count)
       when i >= rounds do
    IO.puts("draw_string_stress done rounds=#{rounds} ok=#{ok_count} err=#{err_count}")

    if err_count == 0 do
      :ok
    else
      {:error, {:draw_string_stress_failed, ok_count, err_count}}
    end
  end

  defp do_run(port, w, rows, rounds, text_y0, main_preset, i, ok_count, err_count) do
    row = rem(i, rows)
    y = text_y0 + row * @line_h

    row_bg = row_bg_color(row)
    fg = row_fg_color(i)

    # Clear row and draw a small marker strip.
    _ = LGFXPort.fill_rect(port, 0, y, w, @line_h, row_bg)
    _ = LGFXPort.fill_rect(port, 0, y, 2, @line_h, @row_marker)
    _ = restore_main_text_style(port, main_preset)
    _ = LGFXPort.set_text_color(port, fg, nil, 0)

    # Fresh runtime binary each iteration (important for lifetime testing).
    text = make_line(i)

    {ok_count2, err_count2} =
      case draw_line_with_cursor(port, w, @x, y, text, i) do
        :ok ->
          {ok_count + 1, err_count}

        {:error, reason} ->
          IO.puts("draw_string_stress error i=#{i}: #{LGFXPort.format_error(reason)}")
          {ok_count, err_count + 1}
      end

    # Churn another fresh binary after draw to help expose lifetime bugs.
    _trash = make_line(i + 10_000)

    if should_log_progress?(i, rounds) do
      IO.puts("draw_string_stress progress i=#{i}/#{rounds} ok=#{ok_count2} err=#{err_count2}")
      _ = draw_status(port, w, rounds, i, ok_count2, err_count2, rows, main_preset)
    end

    if rem(i, @gc_every) == 0 do
      :erlang.garbage_collect()
    end

    yield()
    do_run(port, w, rows, rounds, text_y0, main_preset, i + 1, ok_count2, err_count2)
  end

  defp draw_line_with_cursor(port, screen_w, x, y, text, i) do
    result =
      with :ok <- LGFXPort.set_clip_rect(port, 0, y, screen_w, @line_h, 0),
           :ok <- LGFXPort.set_cursor(port, x, y, 0) do
        if use_println?(i) do
          LGFXPort.println(port, text, 0)
        else
          LGFXPort.print(port, text, 0)
        end
      end

    _ = LGFXPort.clear_clip_rect(port, 0)
    result
  end

  defp restore_main_text_style(port, main_preset) do
    _ = LGFXPort.set_text_font_preset(port, main_preset, 0)
    _ = LGFXPort.set_text_size(port, @main_text_scale, 0)
    :ok
  end

  defp set_hud_text_style(port) do
    _ = LGFXPort.set_text_font_preset(port, :ascii, 0)
    _ = LGFXPort.set_text_size(port, 1, 0)
    :ok
  end

  defp use_println?(i), do: rem(i, @println_every) == 0

  defp should_log_progress?(i, rounds) do
    i == 0 or i == rounds - 1 or rem(i, @progress_every) == 0
  end

  # ---------------------------------------------------------------------------
  # HUD / UI
  # ---------------------------------------------------------------------------

  defp draw_chrome(port, w, h, text_y0) do
    _ = LGFXPort.fill_screen(port, @bg)

    if text_y0 > 0 do
      _ = LGFXPort.fill_rect(port, 0, 0, w, text_y0, @hud_bg)
      _ = LGFXPort.draw_fast_hline(port, 0, text_y0 - 1, w, @divider)
    end

    if h > text_y0 do
      _ = LGFXPort.fill_rect(port, 0, text_y0, w, h - text_y0, @console_bg)
    end

    :ok
  end

  defp draw_status(port, screen_w, rounds, i, ok_count, err_count, rows, main_preset) do
    bar_x = 4
    bar_y = @hud_bar_y
    bar_h = @hud_bar_h
    bar_w = max_i(8, screen_w - 8)

    _ = LGFXPort.fill_rect(port, 0, 0, screen_w, @hud_h, @hud_bg)
    _ = set_hud_text_style(port)

    line1 =
      <<"TXT cursor  ", progress_label(i, rounds)::binary, "  ok:", i2b(ok_count)::binary,
        "  err:", i2b(err_count)::binary>>

    line2 =
      <<"rows:", i2b(rows)::binary, "  lh:", i2b(@line_h)::binary, "  ",
        preset_label(main_preset)::binary, "  pln:", i2b(@println_every)::binary>>

    _ = LGFXPort.draw_string_bg(port, 4, @hud_title_y, @hud_fg, @hud_bg, @hud_title_scale, line1)
    _ = LGFXPort.draw_string_bg(port, 4, @hud_meta_y, @hud_dim, @hud_bg, @hud_meta_scale, line2)

    _ = LGFXPort.fill_rect(port, bar_x, bar_y, bar_w, bar_h, @progress_bg)

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
      _ = LGFXPort.fill_rect(port, bar_x, bar_y, fill_w, bar_h, bar_color)
    end

    _ = restore_main_text_style(port, main_preset)
    :ok
  end

  defp preset_label(:jp), do: "preset:jp"
  defp preset_label(_), do: "preset:ascii"

  defp progress_label(i, rounds) do
    if i < 0 do
      <<"0/", i2b(rounds)::binary>>
    else
      <<i2b(i + 1)::binary, "/", i2b(rounds)::binary>>
    end
  end

  defp row_bg_color(row) do
    if rem(row, 2) == 0 do
      @row_bg_a
    else
      @row_bg_b
    end
  end

  defp row_fg_color(i) do
    case rem(i, 6) do
      0 -> 0xFFFFFF
      1 -> 0xE8E8E8
      2 -> 0xD8D8D8
      3 -> 0xC8C8C8
      4 -> 0xB8B8B8
      _ -> 0xF0F0F0
    end
  end

  # ---------------------------------------------------------------------------
  # Text generation (fresh runtime binaries; periodic UTF-8)
  # ---------------------------------------------------------------------------

  defp rows_for_height(h) when is_integer(h) and h > 0 do
    r = div(h, @line_h)
    if r > 0, do: r, else: 1
  end

  defp make_line(i) when is_integer(i) do
    if rem(i, @utf8_every) == 0 do
      make_line_utf8(i)
    else
      make_line_ascii(i)
    end
  end

  defp make_line_utf8(i) do
    <<"[utf8 i:", i2b(i)::binary, "] 日本語テスト 設定 戻る 次へ 。、！？">>
  end

  defp make_line_ascii(i) do
    prefix = make_prefix(i)
    prefix_len = byte_size(prefix)
    max_payload_len = max_i(1, 96 - prefix_len)
    payload_len = rem(i * 7 + 13, max_payload_len) + 1
    payload = make_payload(i + 1, payload_len)
    <<prefix::binary, payload::binary>>
  end

  defp make_prefix(i) do
    mode =
      case rem(i, 3) do
        0 -> "tele"
        1 -> "log "
        _ -> "txt "
      end

    <<"[", mode::binary, " i:", i2b(i)::binary, "] ">>
  end

  defp make_payload(seed, len) when is_integer(seed) and is_integer(len) and len > 0 do
    make_payload_bytes(seed, len, 0, [])
    |> :lists.reverse()
    |> :erlang.list_to_binary()
  end

  defp make_payload_bytes(_seed, 0, _pos, acc), do: acc

  defp make_payload_bytes(seed, n, pos, acc) do
    seed2 = rem(seed * 1_664_525 + 1_013_904_223, 4_294_967_296)

    ch =
      if pos > 0 and rem(pos, 5) == 0 do
        ?-
      else
        alnum_char(rem(seed2, 62))
      end

    make_payload_bytes(seed2, n - 1, pos + 1, [ch | acc])
  end

  defp alnum_char(v) when v < 10, do: ?0 + v
  defp alnum_char(v) when v < 36, do: ?A + (v - 10)
  defp alnum_char(v), do: ?a + (v - 36)

  # ---------------------------------------------------------------------------
  # Tiny helpers (AtomVM-safe)
  # ---------------------------------------------------------------------------

  defp i2b(i), do: :erlang.integer_to_binary(i)

  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
