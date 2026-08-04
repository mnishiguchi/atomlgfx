# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.RenderBatch.Encoder do
  @moduledoc false

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.Command

  @type normalized_command :: Command.normalized_command()

  @doc """
  正規化済み命令を1つのバイナリーバッチ命令列へ符号化します。
  """
  @spec encode_normalized([normalized_command()]) :: {:ok, binary()} | {:error, term()}
  def encode_normalized(commands) when is_list(commands) do
    with {:ok, encoded_commands} <- encode_all(commands, []) do
      {:ok, BinaryBatch.batch(encoded_commands)}
    end
  end

  def encode_normalized(commands), do: {:error, {:bad_render_commands, commands}}

  defp encode_all([], acc), do: {:ok, :lists.reverse(acc)}

  defp encode_all([command | rest], acc) do
    case safe_encode_one(command) do
      {:ok, encoded_command} -> encode_all(rest, [encoded_command | acc])
      {:error, reason} -> {:error, reason}
    end
  end

  defp safe_encode_one(command) do
    case encode_one(command) do
      {:error, reason} -> {:error, reason}
      encoded_command -> {:ok, encoded_command}
    end
  rescue
    error in ArgumentError -> {:error, {:bad_render_batch, error.message}}
  end

  defp encode_one(:display), do: BinaryBatch.display()
  defp encode_one(:clear_clip_rect), do: BinaryBatch.clear_clip_rect()
  defp encode_one({:target, target}), do: BinaryBatch.target(target)
  defp encode_one({:color_mode, mode}), do: BinaryBatch.color_mode(mode)
  defp encode_one({:fill_screen, color}), do: BinaryBatch.fill_screen(color)
  defp encode_one({:clear, color}), do: BinaryBatch.clear(color)
  defp encode_one({:draw_pixel, x, y, color}), do: BinaryBatch.draw_pixel(x, y, color)

  defp encode_one({:draw_fast_vline, x, y, height, color}) do
    BinaryBatch.draw_fast_vline(x, y, height, color)
  end

  defp encode_one({:draw_fast_hline, x, y, width, color}) do
    BinaryBatch.draw_fast_hline(x, y, width, color)
  end

  defp encode_one({:draw_line, x0, y0, x1, y1, color}) do
    BinaryBatch.draw_line(x0, y0, x1, y1, color)
  end

  defp encode_one({:draw_rect, x, y, width, height, color}) do
    BinaryBatch.draw_rect(x, y, width, height, color)
  end

  defp encode_one({:fill_rect, x, y, width, height, color}) do
    BinaryBatch.fill_rect(x, y, width, height, color)
  end

  defp encode_one({:draw_round_rect, x, y, width, height, radius, color}) do
    BinaryBatch.draw_round_rect(x, y, width, height, radius, color)
  end

  defp encode_one({:fill_round_rect, x, y, width, height, radius, color}) do
    BinaryBatch.fill_round_rect(x, y, width, height, radius, color)
  end

  defp encode_one({:draw_circle, x, y, radius, color}) do
    BinaryBatch.draw_circle(x, y, radius, color)
  end

  defp encode_one({:fill_circle, x, y, radius, color}) do
    BinaryBatch.fill_circle(x, y, radius, color)
  end

  defp encode_one({:draw_ellipse, x, y, radius_x, radius_y, color}) do
    BinaryBatch.draw_ellipse(x, y, radius_x, radius_y, color)
  end

  defp encode_one({:fill_ellipse, x, y, radius_x, radius_y, color}) do
    BinaryBatch.fill_ellipse(x, y, radius_x, radius_y, color)
  end

  defp encode_one({:draw_arc, x, y, radius0, radius1, angle0, angle1, color}) do
    BinaryBatch.draw_arc(x, y, radius0, radius1, angle0, angle1, color)
  end

  defp encode_one({:fill_arc, x, y, radius0, radius1, angle0, angle1, color}) do
    BinaryBatch.fill_arc(x, y, radius0, radius1, angle0, angle1, color)
  end

  defp encode_one({:draw_bezier, x0, y0, x1, y1, x2, y2, color}) do
    BinaryBatch.draw_bezier(x0, y0, x1, y1, x2, y2, color)
  end

  defp encode_one({:draw_bezier, x0, y0, x1, y1, x2, y2, x3, y3, color}) do
    BinaryBatch.draw_bezier(x0, y0, x1, y1, x2, y2, x3, y3, color)
  end

  defp encode_one({:draw_triangle, x0, y0, x1, y1, x2, y2, color}) do
    BinaryBatch.draw_triangle(x0, y0, x1, y1, x2, y2, color)
  end

  defp encode_one({:fill_triangle, x0, y0, x1, y1, x2, y2, color}) do
    BinaryBatch.fill_triangle(x0, y0, x1, y1, x2, y2, color)
  end

  defp encode_one({:set_clip_rect, x, y, width, height}) do
    BinaryBatch.set_clip_rect(x, y, width, height)
  end

  defp encode_one({:set_text_font_preset, preset}), do: BinaryBatch.set_text_font_preset(preset)
  defp encode_one({:set_text_size, scale}), do: BinaryBatch.set_text_size(scale)

  defp encode_one({:set_text_size_xy, scale_x, scale_y}),
    do: BinaryBatch.set_text_size_xy(scale_x, scale_y)

  defp encode_one({:set_text_datum, datum}), do: BinaryBatch.set_text_datum(datum)
  defp encode_one({:set_text_wrap, wrap}), do: BinaryBatch.set_text_wrap(wrap)

  defp encode_one({:set_text_wrap_xy, wrap_x, wrap_y}),
    do: BinaryBatch.set_text_wrap_xy(wrap_x, wrap_y)

  defp encode_one({:set_cursor, x, y}), do: BinaryBatch.set_cursor(x, y)
  defp encode_one({:set_text_color, fg_color}), do: BinaryBatch.set_text_color(fg_color)

  defp encode_one({:set_text_color, fg_color, bg_color}) do
    BinaryBatch.set_text_color(fg_color, bg_color)
  end

  defp encode_one({:draw_string, text, x, y}), do: BinaryBatch.draw_string(x, y, text)
  defp encode_one({:print, text}), do: BinaryBatch.print(text)
  defp encode_one({:println, text}), do: BinaryBatch.println(text)

  defp encode_one({:set_palette_color, palette_index, rgb888}) do
    BinaryBatch.set_palette_color(palette_index, rgb888)
  end

  defp encode_one({:set_pivot, x, y}), do: BinaryBatch.set_pivot(x, y)

  defp encode_one({:push_sprite, source_target, x, y}),
    do: BinaryBatch.push_sprite(source_target, x, y)

  defp encode_one({:push_sprite, source_target, x, y, transparent}) do
    BinaryBatch.push_sprite(source_target, x, y, transparent)
  end

  defp encode_one({:push_rotate_zoom, source_target, x, y, angle_deg, zoom}) do
    BinaryBatch.push_rotate_zoom(source_target, x, y, angle_deg, zoom)
  end

  defp encode_one({:push_rotate_zoom, source_target, x, y, angle_deg, zoom_x, zoom_y}) do
    BinaryBatch.push_rotate_zoom(source_target, x, y, angle_deg, zoom_x, zoom_y)
  end

  defp encode_one(
         {:push_rotate_zoom, source_target, x, y, angle_deg, zoom_x, zoom_y, transparent}
       ) do
    BinaryBatch.push_rotate_zoom(source_target, x, y, angle_deg, zoom_x, zoom_y, transparent)
  end

  defp encode_one(command) do
    {:error, {:bad_render_batch, "unsupported normalized render command: #{inspect(command)}"}}
  end
end
