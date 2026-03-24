# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.ColorSmoke do
  @moduledoc false

  alias AtomLGFX.Color

  @bg Color.black()
  @fg Color.white()
  @frame 0x4208
  @accent 0x07FF

  @sprite_target 21
  @raw_sprite_target 22

  @top_pad 20
  @section_gap 8
  @label_h 12
  @section_min_h 72
  @grid_inner_pad 6
  @raw_cell_gap 6

  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    section_h = max_i(@section_min_h, div(max_i(h - @top_pad - 8, 1), 4))
    grid_w = max_i(80, w - 8)
    grid_h = max_i(40, section_h - @label_h - @section_gap)

    y0 = @top_pad
    y1 = y0 + section_h
    y2 = y1 + section_h
    y3 = y2 + section_h

    raw_cell_w = max_i(60, div(max_i(grid_w - @raw_cell_gap, 1), 2))
    raw_cell_h = max_i(24, div(max_i(grid_h - @raw_cell_gap, 1), 2))
    raw_image_w = max_i(8, raw_cell_w - 2)
    raw_image_h = max_i(8, raw_cell_h - 13)

    try do
      with :ok <- setup_lcd(port, w, h),
           :ok <- draw_header(port),
           :ok <- draw_section_label(port, 4, y0, "LCD direct"),
           :ok <- draw_swatches(port, 4, y0 + @label_h, grid_w, grid_h, 0),
           :ok <- AtomLGFX.create_sprite(port, grid_w, grid_h, 16, @sprite_target),
           :ok <- AtomLGFX.create_sprite(port, raw_image_w, raw_image_h, 16, @raw_sprite_target),
           :ok <- draw_section_label(port, 4, y1, "sprite push swap=false"),
           :ok <- draw_sprite_row(port, false, 4, y1 + @label_h, grid_w, grid_h),
           :ok <- draw_section_label(port, 4, y2, "sprite push swap=true"),
           :ok <- draw_sprite_row(port, true, 4, y2 + @label_h, grid_w, grid_h),
           :ok <- draw_section_label(port, 4, y3, "raw pushImage contract"),
           :ok <- AtomLGFX.draw_string(port, 160, y3, "ordinary rgb565 = little-endian", 0),
           :ok <- draw_raw_matrix(port, 4, y3 + @label_h, grid_w, grid_h),
           :ok <- AtomLGFX.set_swap_bytes(port, false, 0),
           :ok <- AtomLGFX.set_swap_bytes(port, false, @sprite_target),
           :ok <- AtomLGFX.set_swap_bytes(port, false, @raw_sprite_target) do
        IO.puts("color_smoke ok")
        :ok
      else
        {:error, reason} = err ->
          IO.puts("color_smoke failed: #{AtomLGFX.format_error(reason)}")
          err
      end
    after
      _ = AtomLGFX.set_swap_bytes(port, false, 0)
      _ = AtomLGFX.set_swap_bytes(port, false, @sprite_target)
      _ = AtomLGFX.set_swap_bytes(port, false, @raw_sprite_target)
      _ = AtomLGFX.delete_sprite(port, @raw_sprite_target)
      _ = AtomLGFX.delete_sprite(port, @sprite_target)
    end
  end

  defp setup_lcd(port, w, h) do
    with :ok <- AtomLGFX.set_swap_bytes(port, false, 0),
         :ok <- AtomLGFX.fill_screen(port, @bg, 0),
         :ok <- AtomLGFX.draw_rect(port, 0, 0, w, h, @frame, 0),
         :ok <- AtomLGFX.reset_text_state(port, 0),
         :ok <- AtomLGFX.set_text_wrap(port, false, 0),
         :ok <- AtomLGFX.set_text_font_preset(port, :ascii, 0),
         :ok <- AtomLGFX.set_text_size(port, 1, 0),
         :ok <- AtomLGFX.set_text_color(port, @fg, nil, 0) do
      :ok
    end
  end

  defp draw_header(port) do
    with :ok <- AtomLGFX.draw_string_bg(port, 4, 2, @fg, @bg, 1, "COLOR SMOKE", 0),
         :ok <- AtomLGFX.set_text_color(port, @accent, nil, 0),
         :ok <- AtomLGFX.draw_string(port, 118, 2, "rgb565 parity", 0),
         :ok <- AtomLGFX.set_text_color(port, @fg, nil, 0) do
      :ok
    end
  end

  defp draw_section_label(port, x, y, label) do
    with :ok <- AtomLGFX.set_text_color(port, @fg, nil, 0),
         :ok <- AtomLGFX.draw_string(port, x, y, label, 0) do
      :ok
    end
  end

  defp draw_sprite_row(port, swap_bytes?, x, y, w, h) do
    with :ok <- AtomLGFX.set_swap_bytes(port, swap_bytes?, @sprite_target),
         :ok <- AtomLGFX.fill_screen(port, @bg, @sprite_target),
         :ok <- draw_swatches(port, 0, 0, w, h, @sprite_target),
         :ok <- AtomLGFX.push_sprite(port, @sprite_target, x, y) do
      :ok
    end
  end

  defp draw_swatches(port, x, y, w, h, target) do
    colors = swatches()
    cols = 4
    rows = 2

    cell_w = max_i(28, div(max_i(w - @grid_inner_pad * (cols + 1), 1), cols))
    cell_h = max_i(18, div(max_i(h - @grid_inner_pad * (rows + 1), 1), rows))
    swatch_h = max_i(10, cell_h - 10)

    draw_swatches_loop(port, colors, 0, cols, x, y, cell_w, cell_h, swatch_h, target)
  end

  defp draw_swatches_loop(_port, colors, index, _cols, _x, _y, _cell_w, _cell_h, _swatch_h, _target)
       when index >= length(colors),
       do: :ok

  defp draw_swatches_loop(port, colors, index, cols, x, y, cell_w, cell_h, swatch_h, target) do
    {label, color} = list_at(colors, index)
    col = rem(index, cols)
    row = div(index, cols)

    cell_x = x + @grid_inner_pad + col * (cell_w + @grid_inner_pad)
    cell_y = y + @grid_inner_pad + row * (cell_h + @grid_inner_pad)

    with :ok <- AtomLGFX.fill_rect(port, cell_x, cell_y, cell_w, swatch_h, color, target),
         :ok <- AtomLGFX.draw_rect(port, cell_x, cell_y, cell_w, swatch_h, @frame, target),
         :ok <- AtomLGFX.set_text_color(port, @fg, nil, target),
         :ok <- AtomLGFX.draw_string(port, cell_x + 2, cell_y + swatch_h + 1, label, target) do
      draw_swatches_loop(port, colors, index + 1, cols, x, y, cell_w, cell_h, swatch_h, target)
    end
  end

  defp draw_raw_matrix(port, x, y, w, h) do
    cell_w = max_i(60, div(max_i(w - @raw_cell_gap, 1), 2))
    cell_h = max_i(24, div(max_i(h - @raw_cell_gap, 1), 2))

    x0 = x
    x1 = x + cell_w + @raw_cell_gap
    y0 = y
    y1 = y + cell_h + @raw_cell_gap

    with :ok <- draw_raw_cell(port, "ordinary rgb565 / swap off", :ordinary, false, x0, y0, cell_w, cell_h),
         :ok <- draw_raw_cell(port, "ordinary rgb565 / swap on", :ordinary, true, x1, y0, cell_w, cell_h),
         :ok <- draw_raw_cell(port, "pre-swapped / swap off", :pre_swapped, false, x0, y1, cell_w, cell_h),
         :ok <- draw_raw_cell(port, "pre-swapped / swap on", :pre_swapped, true, x1, y1, cell_w, cell_h) do
      :ok
    end
  end

  defp draw_raw_cell(port, label, payload_mode, swap_bytes?, x, y, w, h) do
    image_w = max_i(8, w - 2)
    image_h = max_i(8, h - 13)
    pixels = raw_swatch_pixels(image_w, image_h, payload_mode)

    with :ok <- AtomLGFX.fill_rect(port, x, y, w, h, @bg, 0),
         :ok <- AtomLGFX.draw_rect(port, x, y, w, h, @frame, 0),
         :ok <- AtomLGFX.draw_string_bg(port, x + 2, y + 1, @fg, @bg, 1, label, 0),
         :ok <- AtomLGFX.set_swap_bytes(port, swap_bytes?, @raw_sprite_target),
         :ok <- AtomLGFX.fill_screen(port, @bg, @raw_sprite_target),
         :ok <- AtomLGFX.push_image_rgb565(port, 0, 0, image_w, image_h, pixels, 0, @raw_sprite_target),
         :ok <- AtomLGFX.push_sprite(port, @raw_sprite_target, x + 1, y + 12),
         :ok <- AtomLGFX.set_swap_bytes(port, false, @raw_sprite_target) do
      :ok
    end
  end

  defp raw_swatch_pixels(w, h, payload_mode) when payload_mode in [:ordinary, :pre_swapped] do
    row_colors = raw_swatch_row_colors(w, swatch_colors())

    row_binary =
      case payload_mode do
        :ordinary -> Color.pixels_le(row_colors)
        :pre_swapped -> Color.pixels_swap565(row_colors)
      end

    :binary.copy(row_binary, h)
  end

  defp raw_swatch_row_colors(w, colors) do
    count = length(colors)
    base_w = div(w, count)
    extra = rem(w, count)

    raw_swatch_row_colors(colors, base_w, extra, 0, [])
  end

  defp raw_swatch_row_colors([], _base_w, _extra, _index, acc) do
    :lists.reverse(acc)
  end

  defp raw_swatch_row_colors([color | rest], base_w, extra, index, acc) do
    stripe_w = base_w + if(index < extra, do: 1, else: 0)
    next_acc = prepend_repeated_color(color, stripe_w, acc)
    raw_swatch_row_colors(rest, base_w, extra, index + 1, next_acc)
  end

  defp prepend_repeated_color(_color, count, acc) when count <= 0, do: acc

  defp prepend_repeated_color(color, count, acc) do
    prepend_repeated_color(color, count - 1, [color | acc])
  end

  defp swatches do
    [
      {"RED", Color.red()},
      {"GRN", Color.green()},
      {"BLU", Color.blue()},
      {"YEL", Color.yellow()},
      {"CYA", Color.cyan()},
      {"ORN", Color.color565(255, 165, 0)},
      {"WHT", Color.white()},
      {"PNK", Color.color565(255, 105, 180)}
    ]
  end

  defp swatch_colors do
    swatch_colors(swatches(), [])
  end

  defp swatch_colors([], acc), do: :lists.reverse(acc)

  defp swatch_colors([{_label, color} | rest], acc) do
    swatch_colors(rest, [color | acc])
  end

  defp list_at([head | _rest], 0), do: head
  defp list_at([_head | rest], index) when index > 0, do: list_at(rest, index - 1)

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
