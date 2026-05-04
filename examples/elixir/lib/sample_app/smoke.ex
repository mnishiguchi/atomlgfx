# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.Smoke do
  @moduledoc false

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.Color

  @bg 0x0000
  @fg 0xFFFF
  @frame 0x4208
  @accent 0x07FF
  @muted 0x8410
  @ok 0x07E0

  @sprite_target 20
  @palette_sprite_target 21

  # Tiny embedded 8x8 red JPEG. Keeping it inline avoids a separate smoke asset.
  @jpeg_8x8 "\xFF\xD8\xFF\xE0\x00\x10\x4A\x46\x49\x46\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00\xFF\xDB\x00\x43\x00\x10\x0B\x0C\x0E\x0C\x0A\x10" <>
              "\x0E\x0D\x0E\x12\x11\x10\x13\x18\x28\x1A\x18\x16\x16\x18\x31\x23\x25\x1D\x28\x3A\x33\x3D\x3C\x39\x33\x38\x37\x40\x48\x5C\x4E\x40" <>
              "\x44\x57\x45\x37\x38\x50\x6D\x51\x57\x5F\x62\x67\x68\x67\x3E\x4D\x71\x79\x70\x64\x78\x5C\x65\x67\x63\xFF\xDB\x00\x43\x01\x11\x12" <>
              "\x12\x18\x15\x18\x2F\x1A\x1A\x2F\x63\x42\x38\x42\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63" <>
              "\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\xFF\xC0" <>
              "\x00\x11\x08\x00\x08\x00\x08\x03\x01\x22\x00\x02\x11\x01\x03\x11\x01\xFF\xC4\x00\x15\x00\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00" <>
              "\x00\x00\x00\x00\x00\x00\x00\x05\xFF\xC4\x00\x14\x10\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xC4" <>
              "\x00\x15\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x06\xFF\xC4\x00\x14\x11\x01\x00\x00\x00\x00\x00" <>
              "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xDA\x00\x0C\x03\x01\x00\x02\x11\x03\x11\x00\x3F\x00\x8A\x00\xB5\xE3\xFF\xD9"

  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    try do
      with :ok <- binary_batch_smoke(port, w, h),
           :ok <- primitives_and_text(port, w, h),
           :ok <- clip_rects(port, w, h),
           :ok <- image_paths(port, w, h),
           :ok <- colors_and_palette(port, w, h),
           :ok <- touch_probe(port) do
        IO.puts("smoke ok")
        :ok
      else
        {:error, reason} = err ->
          IO.puts("smoke failed: #{AtomLGFX.format_error(reason)}")
          err
      end
    after
      cleanup(port)
    end
  end

  def write_session(port) do
    with :ok <- raw_ok(port, :start_write),
         :ok <- raw_ok(port, :start_write),
         :ok <- AtomLGFX.fill_screen(port, @bg),
         :ok <- AtomLGFX.set_text_color(port, @fg, nil, 0),
         :ok <- AtomLGFX.draw_string(port, 8, 8, "AtomLGFX v2", 0),
         :ok <- AtomLGFX.draw_string(port, 8, 28, "write session ok", 0),
         :ok <- raw_ok(port, :end_write),
         :ok <- raw_ok(port, :end_write),
         :ok <- raw_ok(port, :end_write) do
      IO.puts("write_session_smoke ok")
      :ok
    end
  end

  def calibrate_touch(port, w, h) do
    with {:ok, true} <- AtomLGFX.supports_touch?(port),
         {:ok, params} <- AtomLGFX.calibrate_touch(port),
         :ok <- AtomLGFX.set_touch_calibrate(port, params),
         :ok <- draw_touch_calibrated(port, w, h) do
      IO.puts("touch_calibrate ok params=#{inspect(params)}")
      :ok
    else
      {:ok, false} -> {:error, :cap_touch_missing}
      {:error, reason} -> {:error, reason}
    end
  end

  defp binary_batch_smoke(port, w, h) do
    batch =
      BinaryBatch.batch([
        BinaryBatch.target(0),
        BinaryBatch.fill_screen(0x0841),
        BinaryBatch.draw_rect(6, 6, max(16, w - 12), max(16, h - 12), @frame),
        BinaryBatch.draw_line(10, div(h, 2), w - 11, div(h, 2), @accent),
        BinaryBatch.draw_line(div(w, 2), 10, div(w, 2), h - 11, @accent),
        BinaryBatch.fill_circle(div(w, 2), div(h, 2), max(3, div(min(w, h), 7)), @fg),
        BinaryBatch.draw_pixel(div(w, 2), div(h, 2), @bg)
      ])

    with {:ok, true} <- AtomLGFX.supports_batch?(port),
         :ok <- BinaryBatch.render(port, batch),
         :ok <- draw_label(port, 8, 8, "BBATCH", 0x0841) do
      IO.puts("binary_batch_smoke ok")
      :ok
    else
      {:ok, false} -> {:error, :cap_batch_missing}
      {:error, reason} -> {:error, reason}
    end
  end

  defp primitives_and_text(port, w, h) do
    margin = 10
    top_h = max(28, div(h, 4))
    mid_y = margin + top_h + 8
    mid_h = max(28, div(h, 4))
    bottom_y = mid_y + mid_h + 8
    cx = div(w, 2)

    with :ok <- AtomLGFX.fill_screen(port, 0x10A3),
         :ok <- AtomLGFX.reset_text_state(port, 0),
         :ok <- AtomLGFX.set_text_font_preset(port, :ascii, 0),
         :ok <- AtomLGFX.set_text_size(port, 1, 0),
         :ok <- AtomLGFX.set_text_color(port, @fg, nil, 0),
         :ok <- AtomLGFX.draw_string_bg(port, 4, 2, @fg, 0x10A3, 1, "SMOKE", 0),
         :ok <- AtomLGFX.draw_string_bg(port, 52, 2, @muted, 0x10A3, 1, "primitives", 0),
         :ok <- AtomLGFX.draw_round_rect(port, margin, margin, w - margin * 2, top_h, 8, @fg),
         :ok <-
           AtomLGFX.fill_ellipse(
             port,
             cx,
             mid_y + div(mid_h, 2),
             max(8, div(w, 5)),
             max(6, div(mid_h, 3)),
             0xF4E2
           ),
         :ok <- AtomLGFX.draw_arc(port, cx, bottom_y + 24, 12, 32, -30, 210, 0x256C),
         :ok <-
           AtomLGFX.draw_bezier(port, margin, h - 12, cx, bottom_y, w - margin, h - 12, 0xE8EC),
         :ok <- AtomLGFX.draw_string(port, 4, max(0, h - 14), "primitives/text ok", 0) do
      IO.puts("primitives_text ok")
      :ok
    end
  end

  defp clip_rects(port, w, h) do
    clip_x = max(8, div(w, 8))
    clip_y = max(24, div(h, 6))
    clip_w = max(16, w - clip_x * 2)
    clip_h = max(16, h - clip_y * 2)

    with :ok <- AtomLGFX.clear_clip_rect(port, 0),
         :ok <- AtomLGFX.fill_screen(port, @bg),
         :ok <- draw_label(port, 4, 4, "CLIP", @bg),
         :ok <- AtomLGFX.draw_rect(port, clip_x, clip_y, clip_w, clip_h, @frame, 0),
         :ok <- AtomLGFX.set_clip_rect(port, clip_x, clip_y, clip_w, clip_h, 0),
         :ok <- AtomLGFX.fill_rect(port, 0, 0, w, h, 0x118C, 0),
         :ok <- AtomLGFX.draw_fast_hline(port, 0, clip_y + div(clip_h, 2), w, 0xFE88, 0),
         :ok <- AtomLGFX.draw_fast_vline(port, clip_x + div(clip_w, 2), 0, h, 0xFE88, 0),
         :ok <- AtomLGFX.clear_clip_rect(port, 0),
         :ok <- maybe_sprite_clip(port, w, h) do
      IO.puts("clip_rects ok")
      :ok
    end
  end

  defp maybe_sprite_clip(port, w, h) do
    case AtomLGFX.supports_sprite?(port) do
      {:ok, true} -> sprite_clip(port, w, h)
      {:ok, false} -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

  defp sprite_clip(port, w, h) do
    sw = max(32, min(80, div(w, 3)))
    sh = max(24, min(48, div(h, 4)))
    sx = max(4, w - sw - 4)
    sy = max(24, h - sh - 4)

    with :ok <- safe_delete_sprite(port, @sprite_target),
         :ok <- AtomLGFX.create_sprite(port, sw, sh, 16, @sprite_target),
         :ok <- AtomLGFX.clear(port, @bg, @sprite_target),
         :ok <-
           AtomLGFX.set_clip_rect(port, 6, 6, max(8, sw - 12), max(8, sh - 12), @sprite_target),
         :ok <- AtomLGFX.fill_rect(port, 0, 0, sw, sh, 0x2202, @sprite_target),
         :ok <- AtomLGFX.clear_clip_rect(port, @sprite_target),
         :ok <- AtomLGFX.draw_rect(port, 0, 0, sw, sh, 0x87F0, @sprite_target),
         :ok <- AtomLGFX.push_sprite(port, @sprite_target, sx, sy) do
      :ok
    end
  end

  defp image_paths(port, w, h) do
    image_w = 8
    image_h = 8
    pixels = checker_pixels(image_w, image_h)

    with :ok <- AtomLGFX.fill_screen(port, @bg),
         :ok <- AtomLGFX.draw_string_bg(port, 4, 2, @fg, @bg, 1, "IMAGE", 0),
         :ok <- AtomLGFX.draw_string_bg(port, 48, 2, @muted, @bg, 1, "push + jpg", 0),
         :ok <- AtomLGFX.set_swap_bytes(port, false, 0),
         :ok <- AtomLGFX.push_image_rgb565(port, 8, 24, image_w, image_h, pixels, 0, 0),
         :ok <- jpg_paths(port, w, h) do
      IO.puts("image_paths ok")
      :ok
    end
  end

  defp jpg_paths(port, w, h) do
    scaled_x = max(48, div(w, 2))
    scaled_w = min(64, div(w, 3))
    scaled_h = min(64, div(h, 3))

    case AtomLGFX.draw_jpg(port, 28, 24, @jpeg_8x8, 0) do
      :ok ->
        draw_jpg_scaled_optional(port, scaled_x, 24, scaled_w, scaled_h)

      {:error, {:internal, -1}} ->
        IO.puts("draw_jpg skipped reason=internal(-1)")
        :ok

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp draw_jpg_scaled_optional(port, x, y, w, h) do
    case AtomLGFX.draw_jpg_scaled(port, x, y, w, h, 0, 0, 4, 4, @jpeg_8x8, 0) do
      :ok ->
        :ok

      {:error, {:internal, -1}} ->
        IO.puts("draw_jpg_scaled skipped reason=internal(-1)")
        :ok

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp colors_and_palette(port, w, h) do
    with :ok <- AtomLGFX.fill_screen(port, @bg),
         :ok <- draw_label(port, 4, 4, "COLOR", @bg),
         :ok <- draw_swatches(port, 4, 20, max(80, w - 8), max(28, div(h, 4)), 0),
         :ok <- maybe_palette_sprite(port, w, h) do
      IO.puts("colors_palette ok")
      :ok
    end
  end

  defp maybe_palette_sprite(port, w, h) do
    case AtomLGFX.supports_palette?(port) do
      {:ok, true} -> palette_sprite(port, w, h)
      {:ok, false} -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

  defp palette_sprite(port, w, h) do
    sw = max(32, min(96, div(w, 3)))
    sh = max(24, min(48, div(h, 4)))

    with :ok <- safe_delete_sprite(port, @palette_sprite_target),
         :ok <- AtomLGFX.create_sprite(port, sw, sh, 8, @palette_sprite_target),
         :ok <- AtomLGFX.create_palette(port, @palette_sprite_target),
         :ok <-
           AtomLGFX.set_palette_color(port, @palette_sprite_target, 0, Color.color888(0, 0, 0)),
         :ok <-
           AtomLGFX.set_palette_color(port, @palette_sprite_target, 1, Color.color888(0, 255, 0)),
         :ok <- AtomLGFX.clear(port, Color.index(0), @palette_sprite_target),
         :ok <-
           AtomLGFX.fill_rect(
             port,
             2,
             2,
             max(4, sw - 4),
             max(4, sh - 4),
             Color.index(1),
             @palette_sprite_target
           ),
         :ok <-
           AtomLGFX.push_sprite(
             port,
             @palette_sprite_target,
             max(4, w - sw - 4),
             max(24, h - sh - 4),
             Color.index(0)
           ) do
      :ok
    end
  end

  defp touch_probe(port) do
    case AtomLGFX.supports_touch?(port) do
      {:ok, true} ->
        with {:ok, _touch} <- AtomLGFX.get_touch(port),
             {:ok, _raw} <- AtomLGFX.get_touch_raw(port) do
          IO.puts("touch_probe ok")
          :ok
        end

      {:ok, false} ->
        IO.puts("touch_probe skipped")
        :ok

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp draw_swatches(port, x, y, w, h, target) do
    colors = [
      {"red", Color.red()},
      {"green", Color.green()},
      {"blue", Color.blue()},
      {"white", Color.white()}
    ]

    cell_w = max(24, div(w, length(colors)))
    swatch_h = max(12, h - 12)

    draw_swatches_loop(port, colors, 0, x, y, cell_w, swatch_h, target)
  end

  defp draw_swatches_loop(_port, [], _index, _x, _y, _cell_w, _swatch_h, _target), do: :ok

  defp draw_swatches_loop(port, [{label, color} | rest], index, x, y, cell_w, swatch_h, target) do
    cell_x = x + index * cell_w

    with :ok <- draw_swatch(port, cell_x, y, max(8, cell_w - 2), swatch_h, label, color, target) do
      draw_swatches_loop(port, rest, index + 1, x, y, cell_w, swatch_h, target)
    end
  end

  defp draw_swatch(port, x, y, w, h, label, color, target) do
    with :ok <- AtomLGFX.fill_rect(port, x, y, w, h, color, target),
         :ok <- AtomLGFX.draw_rect(port, x, y, w, h, @frame, target),
         :ok <- AtomLGFX.set_text_color(port, @fg, nil, target),
         :ok <- AtomLGFX.draw_string(port, x + 1, y + h + 1, label, target) do
      :ok
    end
  end

  defp draw_touch_calibrated(port, w, h) do
    with :ok <- AtomLGFX.fill_screen(port, @bg),
         :ok <-
           AtomLGFX.draw_rect(
             port,
             8,
             12,
             max(40, w - 16),
             max(36, min(72, h - 24)),
             @ok,
             0
           ),
         :ok <- AtomLGFX.draw_string_bg(port, 16, 24, @fg, @bg, 2, "TOUCH", 0),
         :ok <- AtomLGFX.draw_string_bg(port, 18, 50, @ok, @bg, 1, "calibration saved", 0) do
      :ok
    end
  end

  defp draw_label(port, x, y, text, bg) do
    AtomLGFX.draw_string_bg(port, x, y, @fg, bg, 1, text, 0)
  end

  defp checker_pixels(w, h) do
    for y <- 0..(h - 1), x <- 0..(w - 1) do
      if rem(x + y, 2) == 0, do: Color.red(), else: Color.blue()
    end
    |> Color.pixels_le()
  end

  defp raw_ok(port, op_name) do
    case AtomLGFX.Raw.call(port, op_name) do
      {:ok, :ok} -> :ok
      {:ok, other} -> {:error, {:unexpected_reply, other}}
      {:error, _reason} = err -> err
    end
  end

  defp cleanup(port) do
    _ = AtomLGFX.clear_clip_rect(port, 0)
    _ = AtomLGFX.clear_clip_rect(port, @sprite_target)
    _ = AtomLGFX.set_swap_bytes(port, false, 0)
    _ = safe_delete_sprite(port, @sprite_target)
    _ = safe_delete_sprite(port, @palette_sprite_target)
    :ok
  end

  defp safe_delete_sprite(port, sprite_target) do
    case AtomLGFX.delete_sprite(port, sprite_target) do
      :ok -> :ok
      {:error, _reason} -> :ok
    end
  end
end
