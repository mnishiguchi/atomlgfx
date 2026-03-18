# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.TouchProbe do
  @moduledoc false

  import SampleApp.AtomVMCompat, only: [yield: 0]

  @bg 0x000000

  @hud_h 32
  @hud_bg 0x101010
  @hud_fg 0xFFFFFF
  @hud_dim 0xA0A0A0

  @cross_color 0x00FF00
  @raw_color 0xFF00FF

  @cross_half 10
  @erase_pad 4

  @poll_ms 30

  # Exit gesture: hold top-left corner for 1.5s
  @exit_box 48
  @exit_hold_ms 1500

  # -----------------------------------------------------------------------------
  # Touch coordinate normalization
  # -----------------------------------------------------------------------------
  # If left/right is swapped: invert_x=true (mirror around screen center).
  # If up/down is swapped:    invert_y=true.
  # If X and Y are swapped:   swap_xy=true.
  @swap_xy false
  @invert_x true
  @invert_y false

  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    with {:ok, true} <- LGFXPort.supports_touch?(port),
         :ok <- draw_static_ui(port, w, h) do
      loop(port, w, h, nil, nil, nil)
    else
      {:ok, false} -> {:error, :cap_touch_missing}
      {:error, reason} -> {:error, reason}
    end
  end

  defp draw_static_ui(port, w, _h) do
    with :ok <- LGFXPort.fill_screen(port, @bg),
         :ok <- LGFXPort.fill_rect(port, 0, 0, w, @hud_h, @hud_bg),
         :ok <- LGFXPort.draw_fast_hline(port, 0, @hud_h - 1, w, 0x303030),
         :ok <- LGFXPort.draw_string_bg(port, 4, 0, @hud_fg, @hud_bg, 1, "TOUCH PROBE", 0),
         :ok <-
           LGFXPort.draw_string_bg(
             port,
             4,
             14,
             @hud_dim,
             @hud_bg,
             1,
             "touch=green raw=magenta hold TL to exit",
             0
           ),
         :ok <- LGFXPort.draw_string(port, 4, @hud_h + 6, "Tap / drag on the panel", 0) do
      :ok
    end
  end

  defp loop(port, w, h, last_touch_xy, last_raw_xy, hold_start_ms) do
    now_ms = :erlang.monotonic_time(:millisecond)

    touch =
      case LGFXPort.get_touch(port) do
        {:ok, :none} -> nil
        {:ok, {x, y, size}} -> normalize_touch({x, y, size}, w, h)
        {:error, reason} -> {:error, {:get_touch_failed, reason}}
      end

    raw =
      case LGFXPort.get_touch_raw(port) do
        {:ok, :none} -> nil
        {:ok, {x, y, size}} -> normalize_touch({x, y, size}, w, h)
        {:error, reason} -> {:error, {:get_touch_raw_failed, reason}}
      end

    case {touch, raw} do
      {{:error, reason}, _} ->
        {:error, reason}

      {_, {:error, reason}} ->
        {:error, reason}

      {touch1, raw1} ->
        {hold_start_ms2, should_exit} = update_exit_hold(touch1, hold_start_ms, now_ms)

        with :ok <- erase_cross(port, w, h, last_touch_xy, @erase_pad),
             :ok <- erase_cross(port, w, h, last_raw_xy, @erase_pad),
             :ok <- draw_cross(port, w, h, touch1, @cross_color),
             :ok <- draw_cross(port, w, h, raw1, @raw_color),
             :ok <- draw_hud(port, w, touch1, raw1) do
          if should_exit do
            :ok
          else
            sleep_ms(@poll_ms)
            yield()
            loop(port, w, h, xy_of(touch1), xy_of(raw1), hold_start_ms2)
          end
        end
    end
  end

  defp draw_hud(port, w, touch, raw) do
    line1 = <<"touch ", touch_label(touch)::binary>>
    line2 = <<"raw   ", touch_label(raw)::binary>>

    with :ok <- LGFXPort.fill_rect(port, 0, 0, w, @hud_h, @hud_bg),
         :ok <- LGFXPort.draw_fast_hline(port, 0, @hud_h - 1, w, 0x303030),
         :ok <- LGFXPort.draw_string_bg(port, 4, 0, @hud_fg, @hud_bg, 1, line1, 0),
         :ok <- LGFXPort.draw_string_bg(port, 4, 14, @hud_dim, @hud_bg, 1, line2, 0) do
      :ok
    end
  end

  defp touch_label(nil), do: "none"

  defp touch_label({x, y, size}) when is_integer(x) and is_integer(y) and is_integer(size) do
    <<"x=", i2b(x)::binary, " y=", i2b(y)::binary, " s=", i2b(size)::binary>>
  end

  defp xy_of(nil), do: nil
  defp xy_of({x, y, _size}), do: {x, y}

  defp erase_cross(_port, _w, _h, nil, _pad), do: :ok

  defp erase_cross(port, w, h, {x, y}, pad) do
    # Clear a small box around previous marker.
    x0 = clamp_i(x - (@cross_half + pad), 0, w - 1)
    y0 = clamp_i(y - (@cross_half + pad), @hud_h, h - 1)
    x1 = clamp_i(x + (@cross_half + pad), 0, w - 1)
    y1 = clamp_i(y + (@cross_half + pad), @hud_h, h - 1)

    rect_w = max_i(0, x1 - x0 + 1)
    rect_h = max_i(0, y1 - y0 + 1)

    if rect_w > 0 and rect_h > 0 do
      LGFXPort.fill_rect(port, x0, y0, rect_w, rect_h, @bg)
    else
      :ok
    end
  end

  defp draw_cross(_port, _w, _h, nil, _color), do: :ok

  defp draw_cross(port, w, h, {x0, y0, _size}, color) do
    x = clamp_i(x0, 0, w - 1)
    y = clamp_i(y0, @hud_h, h - 1)

    # Horizontal line
    {hx, hlen} = clip_span(x - @cross_half, @cross_half * 2 + 1, 0, w)
    # Vertical line
    {vy, vlen} = clip_span(y - @cross_half, @cross_half * 2 + 1, @hud_h, h)

    with :ok <- maybe_hline(port, hx, y, hlen, color),
         :ok <- maybe_vline(port, x, vy, vlen, color) do
      :ok
    end
  end

  defp maybe_hline(_port, _x, _y, len, _color) when len <= 0, do: :ok
  defp maybe_hline(port, x, y, len, color), do: LGFXPort.draw_fast_hline(port, x, y, len, color)

  defp maybe_vline(_port, _x, _y, len, _color) when len <= 0, do: :ok
  defp maybe_vline(port, x, y, len, color), do: LGFXPort.draw_fast_vline(port, x, y, len, color)

  # Clips a 1D span [pos, pos+len) to [min, max_excl)
  defp clip_span(pos, len, min, max_excl) do
    if len <= 0 do
      {min, 0}
    else
      start = max_i(min, pos)
      stop = min_i(max_excl, pos + len)
      {start, max_i(0, stop - start)}
    end
  end

  defp update_exit_hold(nil, _hold_start_ms, _now_ms), do: {nil, false}

  defp update_exit_hold({x, y, _size}, hold_start_ms, now_ms) do
    if x >= 0 and x < @exit_box and y >= @hud_h and y < @hud_h + @exit_box do
      hs = hold_start_ms || now_ms
      {hs, now_ms - hs >= @exit_hold_ms}
    else
      {nil, false}
    end
  end

  defp sleep_ms(ms) when is_integer(ms) and ms > 0 do
    receive do
    after
      ms -> :ok
    end
  end

  defp normalize_touch(nil, _w, _h), do: nil

  defp normalize_touch({x0, y0, size}, w, h)
       when is_integer(x0) and is_integer(y0) and is_integer(size) do
    {x1, y1} =
      if @swap_xy do
        {y0, x0}
      else
        {x0, y0}
      end

    x2 =
      if @invert_x do
        w - 1 - x1
      else
        x1
      end

    y2 =
      if @invert_y do
        h - 1 - y1
      else
        y1
      end

    {x2, y2, size}
  end

  defp i2b(i), do: :erlang.integer_to_binary(i)

  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b

  defp clamp_i(v, lo, hi) do
    cond do
      v < lo -> lo
      v > hi -> hi
      true -> v
    end
  end
end
