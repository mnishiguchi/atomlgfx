# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.Meter do
  @moduledoc false

  import SampleApp.AtomVMCompat, only: [yield: 0]

  alias AtomLGFX.Color

  @meter_size 239
  @half div(@meter_size, 2)

  @canvas_target 50
  @base_target 51
  @needle_target 52

  @palette_depth 2

  @lcd_bg 0x0000
  @outer_alert 0x07FF

  @idx_transparent {:index, 0}
  @idx_face {:index, 1}
  @idx_needle {:index, 2}
  @idx_mark {:index, 3}

  @frame_delay_ms 20

  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    lw = min_i(w, h)
    zoom = lw / @meter_size
    px = div(w, 2)
    py = div(h, 2)

    try do
      with {:ok, true} <- AtomLGFX.supports_sprite?(port),
           {:ok, true} <- AtomLGFX.supports_palette?(port),
           :ok <- prepare_lcd_chrome(port, w, h, lw, px, py, zoom),
           :ok <- prepare_meter_sprites(port),
           :ok <- build_base_sprite(port),
           :ok <- build_needle_sprite(port),
           :ok <- AtomLGFX.set_pivot(port, @canvas_target, @half, @half),
           :ok <- AtomLGFX.set_pivot(port, @needle_target, 1, 9) do
        IO.puts("meter running zoom=#{zoom}")

        loop(port, px, py, zoom, 0.0, :rise)
      else
        {:ok, false} ->
          {:error, :cap_sprite_or_palette_missing}

        {:error, reason} = err ->
          IO.puts("meter failed: #{AtomLGFX.format_error(reason)}")
          err
      end
    after
      _ = safe_delete_sprite(port, @needle_target)
      _ = safe_delete_sprite(port, @base_target)
      _ = safe_delete_sprite(port, @canvas_target)
    end
  end

  defp prepare_lcd_chrome(port, w, h, lw, px, py, zoom) do
    outer_r0 = div(lw, 2)
    outer_r1 = max_i(1, outer_r0 - round(zoom * 3))
    inner_r0 = max_i(1, outer_r0 - round(zoom * 4))
    inner_r1 = max_i(1, outer_r0 - round(zoom * 7))

    with :ok <- AtomLGFX.clear(port, @lcd_bg),
         :ok <- draw_outer_ring(port, px, py, outer_r0, outer_r1, 0),
         :ok <- draw_inner_ring(port, px, py, inner_r0, inner_r1, 0),
         :ok <- AtomLGFX.draw_rect(port, 0, 0, w, h, 0x2104, 0) do
      :ok
    end
  end

  defp draw_outer_ring(_port, _px, _py, _r0, _r1, i) when i >= 180, do: :ok

  defp draw_outer_ring(port, px, py, r0, r1, i) do
    color = gradient_rgb565(i)

    with :ok <- AtomLGFX.fill_arc(port, px, py, r0, r1, 90 + i, 92 + i, color, 0),
         :ok <- AtomLGFX.fill_arc(port, px, py, r0, r1, 88 - i, 90 - i, color, 0) do
      draw_outer_ring(port, px, py, r0, r1, i + 2)
    end
  end

  defp draw_inner_ring(_port, _px, _py, _r0, _r1, i) when i >= 180, do: :ok

  defp draw_inner_ring(port, px, py, r0, r1, i) do
    color = gradient_rgb565(i)

    with :ok <- AtomLGFX.fill_arc(port, px, py, r0, r1, 270 + i, 272 + i, color, 0),
         :ok <- AtomLGFX.fill_arc(port, px, py, r0, r1, 268 - i, 270 - i, color, 0) do
      draw_inner_ring(port, px, py, r0, r1, i + 2)
    end
  end

  defp gradient_rgb565(i) do
    r = clamp_i(round(i * 1.4), 0, 255)
    g = clamp_i(round(i * 1.4 + 2), 0, 255)
    b = clamp_i(round(i * 1.4 + 4), 0, 255)
    Color.color565(r, g, b)
  end

  defp prepare_meter_sprites(port) do
    with :ok <- safe_delete_sprite(port, @needle_target),
         :ok <- safe_delete_sprite(port, @base_target),
         :ok <- safe_delete_sprite(port, @canvas_target),
         :ok <-
           AtomLGFX.create_sprite(port, @meter_size, @meter_size, @palette_depth, @canvas_target),
         :ok <-
           AtomLGFX.create_sprite(port, @meter_size, @meter_size, @palette_depth, @base_target),
         :ok <- AtomLGFX.create_sprite(port, 3, 11, @palette_depth, @needle_target),
         :ok <- prepare_palette(port, @canvas_target),
         :ok <- prepare_palette(port, @base_target),
         :ok <- prepare_palette(port, @needle_target) do
      :ok
    end
  end

  defp prepare_palette(port, target) do
    with :ok <- AtomLGFX.create_palette(port, target),
         :ok <- AtomLGFX.set_palette_color(port, target, 0, 0x000000),
         :ok <- AtomLGFX.set_palette_color(port, target, 1, 0x00000F),
         :ok <- AtomLGFX.set_palette_color(port, target, 2, 0xFF1F1F),
         :ok <- AtomLGFX.set_palette_color(port, target, 3, 0xFFFFBF) do
      :ok
    end
  end

  defp build_base_sprite(port) do
    outer_face_r = @half - 8
    rim_outer = @half - 10
    rim_inner = @half - 11
    accent_outer = @half - 20
    accent_inner = @half - 23

    with :ok <- AtomLGFX.clear(port, @idx_transparent, @base_target),
         :ok <- AtomLGFX.set_text_font_preset(port, :ascii, @base_target),
         :ok <- AtomLGFX.set_text_size(port, 1, @base_target),
         :ok <- AtomLGFX.set_text_color(port, @idx_mark, nil, @base_target),
         :ok <- AtomLGFX.fill_circle(port, @half, @half, outer_face_r, @idx_face, @base_target),
         :ok <-
           AtomLGFX.fill_arc(
             port,
             @half,
             @half,
             rim_outer,
             rim_inner,
             135.0,
             45.0,
             @idx_mark,
             @base_target
           ),
         :ok <-
           AtomLGFX.fill_arc(
             port,
             @half,
             @half,
             accent_outer,
             accent_inner,
             2.0,
             43.0,
             @idx_needle,
             @base_target
           ),
         :ok <-
           AtomLGFX.fill_arc(
             port,
             @half,
             @half,
             accent_outer,
             accent_inner,
             317.0,
             358.0,
             @idx_needle,
             @base_target
           ) do
      draw_scale_marks(port, -5)
    end
  end

  defp draw_scale_marks(_port, i) when i > 25, do: :ok

  defp draw_scale_marks(port, i) do
    angle = 180.0 + i * 9.0

    if rem(i, 5) == 0 do
      rad = i * 9.0 * 0.0174532925
      ty = -:math.sin(rad) * (@half * 10 / 15)
      tx = -:math.cos(rad) * (@half * 10 / 17)

      label = format_tenth(i)
      label_x = round(@half + tx) - 10
      label_y = round(@half + ty) - 6

      with :ok <-
             AtomLGFX.fill_arc(
               port,
               @half,
               @half,
               @half - 10,
               @half - 24,
               179.8 + i * 9.0,
               180.2 + i * 9.0,
               @idx_mark,
               @base_target
             ),
           :ok <-
             AtomLGFX.fill_arc(
               port,
               @half,
               @half,
               @half - 10,
               @half - 20,
               179.4 + i * 9.0,
               180.6 + i * 9.0,
               @idx_mark,
               @base_target
             ),
           :ok <-
             AtomLGFX.fill_arc(
               port,
               @half,
               @half,
               @half - 10,
               @half - 14,
               179.0 + i * 9.0,
               181.0 + i * 9.0,
               @idx_mark,
               @base_target
             ),
           :ok <- AtomLGFX.draw_string(port, label_x, label_y, label, @base_target) do
        draw_scale_marks(port, i + 1)
      end
    else
      with :ok <-
             AtomLGFX.fill_arc(
               port,
               @half,
               @half,
               @half - 10,
               @half - 17,
               179.5 + i * 9.0,
               180.5 + i * 9.0,
               @idx_mark,
               @base_target
             ) do
        draw_scale_marks(port, i + 1)
      end
    end
  end

  defp build_needle_sprite(port) do
    with :ok <- AtomLGFX.clear(port, @idx_transparent, @needle_target),
         :ok <- AtomLGFX.draw_rect(port, 0, 0, 3, 11, @idx_needle, @needle_target),
         :ok <- AtomLGFX.fill_rect(port, 1, 0, 1, 9, @idx_needle, @needle_target) do
      :ok
    end
  end

  defp loop(port, px, py, zoom, value, phase) do
    with :ok <- draw_frame(port, px, py, zoom, value) do
      sleep_ms(@frame_delay_ms)
      yield()
      {next_value, next_phase} = next_value_phase(value, phase)
      loop(port, px, py, zoom, next_value, next_phase)
    else
      {:error, reason} = err ->
        IO.puts("meter failed: #{AtomLGFX.format_error(reason)}")
        err
    end
  end

  defp draw_frame(port, px, py, zoom, value) do
    angle = 270.0 + value * 90.0
    alert_y = py + round(@meter_size * zoom * 0.40)

    with :ok <- AtomLGFX.push_sprite_to(port, @base_target, @canvas_target, 0, 0),
         :ok <-
           AtomLGFX.push_rotate_zoom_to(
             port,
             @needle_target,
             @canvas_target,
             @half,
             @half,
             angle,
             3.0,
             10.0,
             @idx_transparent
           ),
         :ok <- AtomLGFX.fill_circle(port, @half, @half, 7, @idx_mark, @canvas_target),
         :ok <-
           AtomLGFX.push_rotate_zoom_to(
             port,
             @canvas_target,
             0,
             px,
             py,
             0.0,
             zoom,
             zoom,
             @idx_transparent
           ),
         :ok <- maybe_draw_alert(port, px, alert_y, value),
         :ok <- AtomLGFX.display(port) do
      :ok
    end
  end

  defp maybe_draw_alert(port, px, y, value) when value >= 1.5 do
    AtomLGFX.fill_circle(port, px, y, 5, @outer_alert, 0)
  end

  defp maybe_draw_alert(_port, _px, _y, _value), do: :ok

  defp next_value_phase(value, :rise) do
    next_value = value + 0.005 + value * 0.05

    if next_value < 1.9 do
      {next_value, :rise}
    else
      {next_value, :fall}
    end
  end

  defp next_value_phase(value, :fall) do
    next_value = value - 0.1

    if next_value > -0.5 do
      {next_value, :fall}
    else
      {next_value, :recover}
    end
  end

  defp next_value_phase(value, :recover) do
    next_value = value + 0.05

    if next_value < 0.0 do
      {next_value, :recover}
    else
      {0.0, :rise}
    end
  end

  defp format_tenth(i) when is_integer(i) do
    :erlang.float_to_binary(i / 10.0, decimals: 1)
  end

  defp safe_delete_sprite(port, sprite_target) do
    case AtomLGFX.delete_sprite(port, sprite_target) do
      :ok -> :ok
      {:error, _reason} -> :ok
    end
  end

  defp sleep_ms(ms) when is_integer(ms) and ms > 0 do
    receive do
    after
      ms -> :ok
    end
  end

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
