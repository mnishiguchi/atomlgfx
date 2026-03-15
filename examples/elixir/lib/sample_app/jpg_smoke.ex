defmodule SampleApp.JpgSmoke do
  @moduledoc false

  alias LGFXPort, as: Port

  @bg 0x000000
  @fg 0xFFFFFF
  @dim 0xA0A0A0
  @frame 0x303030
  @accent 0x40D0FF

  # Tiny embedded 8x8 red JPEG.
  # Keeping this inline avoids adding a dedicated asset file just for smoke testing.
  @jpeg_8x8 "\xFF\xD8\xFF\xE0\x00\x10\x4A\x46\x49\x46\x00\x01\x01\x00\x00\x01\x00\x01\x00\x00\xFF\xDB\x00\x43\x00\x10\x0B\x0C\x0E\x0C\x0A\x10" <>
              "\x0E\x0D\x0E\x12\x11\x10\x13\x18\x28\x1A\x18\x16\x16\x18\x31\x23\x25\x1D\x28\x3A\x33\x3D\x3C\x39\x33\x38\x37\x40\x48\x5C\x4E\x40" <>
              "\x44\x57\x45\x37\x38\x50\x6D\x51\x57\x5F\x62\x67\x68\x67\x3E\x4D\x71\x79\x70\x64\x78\x5C\x65\x67\x63\xFF\xDB\x00\x43\x01\x11\x12" <>
              "\x12\x18\x15\x18\x2F\x1A\x1A\x2F\x63\x42\x38\x42\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63" <>
              "\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\x63\xFF\xC0" <>
              "\x00\x11\x08\x00\x08\x00\x08\x03\x01\x22\x00\x02\x11\x01\x03\x11\x01\xFF\xC4\x00\x15\x00\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00" <>
              "\x00\x00\x00\x00\x00\x00\x00\x05\xFF\xC4\x00\x14\x10\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xC4" <>
              "\x00\x15\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x05\x06\xFF\xC4\x00\x14\x11\x01\x00\x00\x00\x00\x00" <>
              "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xDA\x00\x0C\x03\x01\x00\x02\x11\x03\x11\x00\x3F\x00\x8A\x00\xB5\xE3\xFF\xD9"

  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    left_x = 8
    top_y = 26

    right_x = max_i(40, div(w, 2))
    scaled_box_w = min_i(64, max_i(16, w - right_x - 8))
    scaled_box_h = min_i(64, max_i(16, h - top_y - 8))

    status_y = max_i(0, h - 14)

    with :ok <- Port.fill_screen(port, @bg),
         :ok <- Port.reset_text_state(port, 0),
         :ok <- Port.set_text_wrap(port, false, 0),
         :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.set_text_color(port, @fg, nil, 0),
         :ok <- Port.draw_rect(port, 0, 0, w, h, @frame, 0),
         :ok <- Port.draw_string_bg(port, 4, 2, @fg, @bg, 1, "JPG SMOKE", 0),
         :ok <- Port.set_text_color(port, @dim, nil, 0),
         :ok <- Port.draw_string(port, 4, 14, "short form + scaled form", 0),
         :ok <- Port.draw_rect(port, left_x - 1, top_y - 1, 10, 10, @frame, 0),
         :ok <- Port.draw_jpg(port, left_x, top_y, @jpeg_8x8, 0),
         :ok <- Port.draw_string(port, left_x, top_y + 14, "raw 8x8", 0),
         :ok <-
           Port.draw_rect(
             port,
             right_x - 1,
             top_y - 1,
             scaled_box_w + 2,
             scaled_box_h + 2,
             @accent,
             0
           ),
         :ok <-
           Port.draw_jpg_scaled(
             port,
             right_x,
             top_y,
             scaled_box_w,
             scaled_box_h,
             0,
             0,
             4,
             4,
             @jpeg_8x8,
             0
           ),
         :ok <- Port.set_text_color(port, @accent, nil, 0),
         :ok <- Port.draw_string(port, right_x, top_y + scaled_box_h + 4, "scaled 4x", 0),
         :ok <- Port.set_text_color(port, @dim, nil, 0),
         :ok <- Port.draw_string(port, 4, status_y, "draw_jpg ok", 0) do
      IO.puts("jpg_smoke ok")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("jpg_smoke failed: #{Port.format_error(reason)}")
        err
    end
  end

  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
