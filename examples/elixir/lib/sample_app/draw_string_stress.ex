defmodule SampleApp.DrawStringStress do
  @moduledoc false

  alias SampleApp.Port
  import SampleApp.AtomVMCompat, only: [yield: 0]

  @bg 0x0B1020
  @hud_bg 0x111827
  @hud_fg 0xE5E7EB
  @hud_dim 0x94A3B8
  @hud_ok 0x10B981
  @hud_err 0xEF4444
  @divider 0x334155

  @console_bg 0x05070E
  @row_bg_a 0x0F172A
  @row_bg_b 0x111827
  @row_marker 0x334155

  @hud_h 44
  @line_h 20
  @x 4

  @progress_every 25
  @gc_every 32

  def run(port, rounds \\ 500) when is_integer(rounds) and rounds > 0 do
    {:ok, w} = Port.width(port, 0)
    {:ok, h} = Port.height(port, 0)

    hud_h = min_i(@hud_h, h)
    text_y0 = hud_h
    text_h = max_i(1, h - hud_h)
    rows = rows_for_height(text_h)

    IO.puts("draw_string_stress start w=#{w} h=#{h} rows=#{rows} rounds=#{rounds}")

    draw_chrome(port, w, h, text_y0)
    draw_status(port, w, rounds, -1, 0, 0, rows)

    _ = Port.reset_text_state(port, 0)
    _ = Port.set_text_wrap(port, false, 0)
    _ = Port.set_text_size(port, 2, 0)
    _ = Port.set_text_color(port, 0xFFFFFF, nil, 0)

    # Client-side validation probe: Port.draw_string/5 rejects empty binary by guard.
    probe_invalid_text_guard(port)

    do_run(port, w, rows, rounds, text_y0, 0, 0, 0)
  end

  defp probe_invalid_text_guard(port) do
    try do
      _ = Port.draw_string(port, @x, 0, <<>>, 0)
      IO.puts("invalid_text_probe unexpected: empty binary accepted")
    catch
      :error, :function_clause ->
        IO.puts("invalid_text_probe ok (client guard)")
    end

    :ok
  end

  defp do_run(_port, _w, _rows, rounds, _text_y0, i, ok_count, err_count) when i >= rounds do
    IO.puts("draw_string_stress done rounds=#{rounds} ok=#{ok_count} err=#{err_count}")

    if err_count == 0 do
      :ok
    else
      {:error, {:draw_string_stress_failed, ok_count, err_count}}
    end
  end

  defp do_run(port, w, rows, rounds, text_y0, i, ok_count, err_count) do
    row = rem(i, rows)
    y = text_y0 + row * @line_h

    row_bg = row_bg_color(row)
    fg = row_fg_color(i)

    # Clear row and draw a small marker strip
    _ = Port.fill_rect(port, 0, y, w, @line_h, row_bg)
    _ = Port.fill_rect(port, 0, y, 2, @line_h, @row_marker)
    _ = Port.set_text_color(port, fg, nil, 0)

    # Fresh runtime binary each iteration (important for lifetime testing)
    text = make_line(i)

    {ok_count2, err_count2} =
      case Port.draw_string(port, @x, y, text, 0) do
        :ok ->
          {ok_count + 1, err_count}

        {:error, reason} ->
          IO.puts("draw_string_stress error i=#{i}: #{Port.format_error(reason)}")
          {ok_count, err_count + 1}
      end

    # Churn another fresh binary after draw to help expose lifetime bugs
    _trash = make_line(i + 10_000)

    if should_log_progress?(i, rounds) do
      IO.puts("draw_string_stress progress i=#{i}/#{rounds} ok=#{ok_count2} err=#{err_count2}")
      draw_status(port, w, rounds, i, ok_count2, err_count2, rows)
    end

    if rem(i, @gc_every) == 0 do
      :erlang.garbage_collect()
    end

    yield()
    do_run(port, w, rows, rounds, text_y0, i + 1, ok_count2, err_count2)
  end

  defp should_log_progress?(i, rounds) do
    i == 0 or i == rounds - 1 or rem(i, @progress_every) == 0
  end

  # ---------------------------------------------------------------------------
  # HUD / UI
  # ---------------------------------------------------------------------------

  defp draw_chrome(port, w, h, text_y0) do
    _ = Port.fill_screen(port, @bg)

    if text_y0 > 0 do
      _ = Port.fill_rect(port, 0, 0, w, text_y0, @hud_bg)
      _ = Port.draw_fast_hline(port, 0, text_y0 - 1, w, @divider)
    end

    if h > text_y0 do
      _ = Port.fill_rect(port, 0, text_y0, w, h - text_y0, @console_bg)
    end

    :ok
  end

  defp draw_status(port, screen_w, rounds, i, ok_count, err_count, rows) do
    bar_x = 4
    bar_y = 24
    bar_h = 8
    bar_w = max_i(8, screen_w - 8)

    _ = Port.fill_rect(port, 0, 0, screen_w, @hud_h, @hud_bg)

    line1 =
      <<"TXT stress  ", progress_label(i, rounds)::binary, "  ", "ok:", i2b(ok_count)::binary,
        "  err:", i2b(err_count)::binary>>

    line2 =
      <<"rows:", i2b(rows)::binary, "  line_h:", i2b(@line_h)::binary,
        "  mode:drawString fresh-binary">>

    _ = Port.draw_string_bg(port, 4, 0, @hud_fg, @hud_bg, 2, line1)
    _ = Port.draw_string_bg(port, 4, 12, @hud_dim, @hud_bg, 1, line2)

    _ = Port.fill_rect(port, bar_x, bar_y, bar_w, bar_h, 0x1F2937)

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
      _ = Port.fill_rect(port, bar_x, bar_y, fill_w, bar_h, bar_color)
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

  defp row_bg_color(row) do
    if rem(row, 2) == 0 do
      @row_bg_a
    else
      @row_bg_b
    end
  end

  defp row_fg_color(i) do
    case rem(i, 6) do
      0 -> 0xE5E7EB
      1 -> 0x93C5FD
      2 -> 0x86EFAC
      3 -> 0xFDE68A
      4 -> 0xFCA5A5
      _ -> 0xC4B5FD
    end
  end

  # ---------------------------------------------------------------------------
  # Text generation (fresh runtime binaries, but more readable)
  # ---------------------------------------------------------------------------

  defp rows_for_height(h) when is_integer(h) and h > 0 do
    r = div(h, @line_h)

    if r > 0 do
      r
    else
      1
    end
  end

  defp make_line(i) when is_integer(i) do
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

  # Alnum payload with periodic separators for readability.
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
