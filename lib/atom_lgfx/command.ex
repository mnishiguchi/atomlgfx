# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Command do
  @moduledoc """
  Normalizes render-first command lists for `AtomLGFX.render/3`.

  This module keeps the user-facing command shape small and LovyanGFX-like while
  leaving binary encoding and port submission to the internal render-batch
  bridge.

  Primitive colors use RGB565 by default. Use `{:index, n}` explicitly when
  drawing into a palette-backed sprite; normalization inserts the required
  low-level color-mode transitions.
  """

  import AtomLGFX.Guards

  alias AtomLGFX.Color

  @type color ::
          0..0xFFFF
          | atom()
          | {:rgb565, 0..0xFFFF}
          | {:rgb565, 0..255, 0..255, 0..255}
          | {:rgb888, 0..0xFFFFFF}
          | {:rgb888, 0..255, 0..255, 0..255}
          | {:rgb, 0..255, 0..255, 0..255}
          | {:index, 0..0xFF}
  @type command :: atom() | tuple()
  @type normalized_command :: atom() | tuple()

  @text_datum_values %{
    top_left: 0,
    top_center: 1,
    top_right: 2,
    middle_left: 3,
    middle_center: 4,
    center: 4,
    middle_right: 5,
    bottom_left: 6,
    bottom_center: 7,
    bottom_right: 8,
    baseline_left: 9,
    baseline_center: 10,
    baseline_right: 11,
    tl: 0,
    tc: 1,
    tr: 2,
    ml: 3,
    mc: 4,
    mr: 5,
    bl: 6,
    bc: 7,
    br: 8
  }

  @primitive_color_ops [
    :fill_screen,
    :clear,
    :draw_pixel,
    :draw_fast_vline,
    :draw_fast_hline,
    :draw_line,
    :draw_rect,
    :fill_rect,
    :draw_round_rect,
    :fill_round_rect,
    :draw_circle,
    :fill_circle,
    :draw_ellipse,
    :fill_ellipse,
    :draw_arc,
    :fill_arc,
    :draw_bezier,
    :draw_triangle,
    :fill_triangle
  ]

  @doc """
  Normalizes one render command.
  """
  @spec normalize_one(command()) :: {:ok, normalized_command()} | {:error, term()}
  def normalize_one(:display), do: {:ok, :display}
  def normalize_one(:clear_clip_rect), do: {:ok, :clear_clip_rect}

  def normalize_one({:target, target}) do
    case normalize_target(target) do
      {:ok, target} -> {:ok, {:target, target}}
      {:error, reason} -> {:error, reason}
    end
  end

  def normalize_one({:fill_screen, color}) do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:fill_screen, color}}
    end
  end

  def normalize_one({:clear, color}) do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:clear, color}}
    end
  end

  def normalize_one({:draw_pixel, x, y, color}) when i16(x) and i16(y) do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_pixel, x, y, color}}
    end
  end

  def normalize_one({:draw_fast_vline, x, y, height, color})
      when i16(x) and i16(y) and u16(height) and height >= 1 do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_fast_vline, x, y, height, color}}
    end
  end

  def normalize_one({:draw_fast_hline, x, y, width, color})
      when i16(x) and i16(y) and u16(width) and width >= 1 do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_fast_hline, x, y, width, color}}
    end
  end

  def normalize_one({:draw_line, x0, y0, x1, y1, color})
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_line, x0, y0, x1, y1, color}}
    end
  end

  def normalize_one({:draw_rect, x, y, width, height, color})
      when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_rect, x, y, width, height, color}}
    end
  end

  def normalize_one({:fill_rect, x, y, width, height, color})
      when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:fill_rect, x, y, width, height, color}}
    end
  end

  def normalize_one({:draw_round_rect, x, y, width, height, radius, color})
      when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 and
             u16(radius) and radius >= 1 do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_round_rect, x, y, width, height, radius, color}}
    end
  end

  def normalize_one({:fill_round_rect, x, y, width, height, radius, color})
      when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 and
             u16(radius) and radius >= 1 do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:fill_round_rect, x, y, width, height, radius, color}}
    end
  end

  def normalize_one({:draw_circle, x, y, radius, color})
      when i16(x) and i16(y) and u16(radius) and radius >= 1 do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_circle, x, y, radius, color}}
    end
  end

  def normalize_one({:fill_circle, x, y, radius, color})
      when i16(x) and i16(y) and u16(radius) and radius >= 1 do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:fill_circle, x, y, radius, color}}
    end
  end

  def normalize_one({:draw_ellipse, x, y, radius_x, radius_y, color})
      when i16(x) and i16(y) and u16(radius_x) and radius_x >= 1 and u16(radius_y) and
             radius_y >= 1 do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_ellipse, x, y, radius_x, radius_y, color}}
    end
  end

  def normalize_one({:fill_ellipse, x, y, radius_x, radius_y, color})
      when i16(x) and i16(y) and u16(radius_x) and radius_x >= 1 and u16(radius_y) and
             radius_y >= 1 do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:fill_ellipse, x, y, radius_x, radius_y, color}}
    end
  end

  def normalize_one({:draw_arc, x, y, radius0, radius1, angle0, angle1, color})
      when i16(x) and i16(y) and u16(radius0) and radius0 >= 1 and u16(radius1) and
             radius1 >= 1 and is_number(angle0) and is_number(angle1) do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_arc, x, y, radius0, radius1, angle0, angle1, color}}
    end
  end

  def normalize_one({:fill_arc, x, y, radius0, radius1, angle0, angle1, color})
      when i16(x) and i16(y) and u16(radius0) and radius0 >= 1 and u16(radius1) and
             radius1 >= 1 and is_number(angle0) and is_number(angle1) do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:fill_arc, x, y, radius0, radius1, angle0, angle1, color}}
    end
  end

  def normalize_one({:draw_bezier, x0, y0, x1, y1, x2, y2, color})
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and i16(x2) and i16(y2) do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_bezier, x0, y0, x1, y1, x2, y2, color}}
    end
  end

  def normalize_one({:draw_bezier, x0, y0, x1, y1, x2, y2, x3, y3, color})
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and i16(x2) and i16(y2) and i16(x3) and
             i16(y3) do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_bezier, x0, y0, x1, y1, x2, y2, x3, y3, color}}
    end
  end

  def normalize_one({:draw_triangle, x0, y0, x1, y1, x2, y2, color})
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and i16(x2) and i16(y2) do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:draw_triangle, x0, y0, x1, y1, x2, y2, color}}
    end
  end

  def normalize_one({:fill_triangle, x0, y0, x1, y1, x2, y2, color})
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and i16(x2) and i16(y2) do
    with {:ok, color} <- normalize_primitive_color(color) do
      {:ok, {:fill_triangle, x0, y0, x1, y1, x2, y2, color}}
    end
  end

  def normalize_one({:set_clip_rect, x, y, width, height})
      when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 do
    {:ok, {:set_clip_rect, x, y, width, height}}
  end

  def normalize_one({:set_text_font_preset, preset}) when preset in [:ascii, :jp] do
    {:ok, {:set_text_font_preset, preset}}
  end

  def normalize_one({:set_text_size, scale}) when is_number(scale) and scale > 0 do
    {:ok, {:set_text_size, scale}}
  end

  def normalize_one({:set_text_size, scale_x, scale_y})
      when is_number(scale_x) and scale_x > 0 and is_number(scale_y) and scale_y > 0 do
    {:ok, {:set_text_size_xy, scale_x, scale_y}}
  end

  def normalize_one({:set_text_size_xy, scale_x, scale_y})
      when is_number(scale_x) and scale_x > 0 and is_number(scale_y) and scale_y > 0 do
    {:ok, {:set_text_size_xy, scale_x, scale_y}}
  end

  def normalize_one({:set_text_datum, datum}) do
    with {:ok, datum} <- normalize_text_datum(datum) do
      {:ok, {:set_text_datum, datum}}
    end
  end

  def normalize_one({:set_text_wrap, wrap}) when is_boolean(wrap) do
    {:ok, {:set_text_wrap, wrap}}
  end

  def normalize_one({:set_text_wrap, wrap_x, wrap_y})
      when is_boolean(wrap_x) and is_boolean(wrap_y) do
    {:ok, {:set_text_wrap_xy, wrap_x, wrap_y}}
  end

  def normalize_one({:set_text_wrap_xy, wrap_x, wrap_y})
      when is_boolean(wrap_x) and is_boolean(wrap_y) do
    {:ok, {:set_text_wrap_xy, wrap_x, wrap_y}}
  end

  def normalize_one({:set_cursor, x, y}) when i16(x) and i16(y) do
    {:ok, {:set_cursor, x, y}}
  end

  def normalize_one({:set_text_color, fg_color}) do
    with {:ok, fg_color} <- normalize_color(fg_color) do
      {:ok, {:set_text_color, fg_color}}
    end
  end

  def normalize_one({:set_text_color, fg_color, bg_color}) do
    with {:ok, fg_color} <- normalize_color(fg_color),
         {:ok, bg_color} <- normalize_color(bg_color) do
      {:ok, {:set_text_color, fg_color, bg_color}}
    end
  end

  def normalize_one({:draw_string, text, x, y}) when is_binary(text) and i16(x) and i16(y) do
    {:ok, {:draw_string, text, x, y}}
  end

  def normalize_one({:draw_string, x, y, text}) when i16(x) and i16(y) and is_binary(text) do
    {:ok, {:draw_string, text, x, y}}
  end

  def normalize_one({:print, text}) when is_binary(text) do
    {:ok, {:print, text}}
  end

  def normalize_one(:println), do: {:ok, {:println, ""}}

  def normalize_one({:println, text}) when is_binary(text) do
    {:ok, {:println, text}}
  end

  def normalize_one({:set_palette_color, palette_index_value, color})
      when palette_index(palette_index_value) do
    with {:ok, color} <- normalize_palette_color(color) do
      {:ok, {:set_palette_color, palette_index_value, color}}
    end
  end

  def normalize_one({:set_pivot, x, y}) when i16(x) and i16(y) do
    {:ok, {:set_pivot, x, y}}
  end

  def normalize_one({:push_sprite, source_target, x, y})
      when sprite_handle(source_target) and i16(x) and i16(y) do
    {:ok, {:push_sprite, source_target, x, y}}
  end

  def normalize_one({:push_sprite, source_target, x, y, transparent})
      when sprite_handle(source_target) and i16(x) and i16(y) do
    with {:ok, transparent} <- normalize_color(transparent) do
      {:ok, {:push_sprite, source_target, x, y, transparent}}
    end
  end

  def normalize_one({:push_rotate_zoom, source_target, x, y, angle_deg, zoom})
      when sprite_handle(source_target) and i16(x) and i16(y) and is_number(angle_deg) and
             is_number(zoom) and zoom > 0 do
    {:ok, {:push_rotate_zoom, source_target, x, y, angle_deg, zoom}}
  end

  def normalize_one({:push_rotate_zoom, source_target, x, y, angle_deg, zoom_x, zoom_y})
      when sprite_handle(source_target) and i16(x) and i16(y) and is_number(angle_deg) and
             is_number(zoom_x) and zoom_x > 0 and is_number(zoom_y) and zoom_y > 0 do
    {:ok, {:push_rotate_zoom, source_target, x, y, angle_deg, zoom_x, zoom_y}}
  end

  def normalize_one(
        {:push_rotate_zoom, source_target, x, y, angle_deg, zoom_x, zoom_y, transparent}
      )
      when sprite_handle(source_target) and i16(x) and i16(y) and is_number(angle_deg) and
             is_number(zoom_x) and zoom_x > 0 and is_number(zoom_y) and zoom_y > 0 do
    with {:ok, transparent} <- normalize_color(transparent) do
      {:ok, {:push_rotate_zoom, source_target, x, y, angle_deg, zoom_x, zoom_y, transparent}}
    end
  end

  def normalize_one(command), do: {:error, {:bad_render_command, command}}

  @doc """
  Normalizes a render command list.
  """
  @spec normalize([command()], keyword()) :: {:ok, [normalized_command()]} | {:error, term()}
  def normalize(commands, opts \\ [])

  def normalize(commands, opts) when is_list(commands) and is_list(opts) do
    default_target = Keyword.get(opts, :target, 0)

    with {:ok, display?} <- display_option(opts),
         {:ok, commands} <- prepend_default_target(commands, default_target),
         {:ok, commands} <- normalize_all(commands, []),
         {:ok, commands} <- insert_primitive_color_modes(commands, :rgb565, []),
         {:ok, commands} <- maybe_append_display(commands, display?) do
      {:ok, commands}
    end
  end

  def normalize(commands, _opts), do: {:error, {:bad_render_commands, commands}}

  @doc """
  Normalizes a display color.
  """
  @spec normalize_color(term()) :: {:ok, color()} | {:error, term()}
  def normalize_color({:index, value}) when palette_index(value), do: {:ok, {:index, value}}

  def normalize_color(color) do
    case Color.normalize_display(color) do
      {:ok, color} -> {:ok, color}
      {:error, _reason} -> {:error, {:bad_render_color, color}}
    end
  end

  @doc """
  Normalizes a text datum name or numeric LovyanGFX datum value.
  """
  @spec normalize_text_datum(term()) :: {:ok, 0..255} | {:error, term()}
  def normalize_text_datum(datum) when u8(datum), do: {:ok, datum}

  def normalize_text_datum(datum) when is_atom(datum) do
    case Map.fetch(@text_datum_values, datum) do
      {:ok, value} -> {:ok, value}
      :error -> {:error, {:bad_text_datum, datum}}
    end
  end

  def normalize_text_datum(datum), do: {:error, {:bad_text_datum, datum}}

  defp normalize_primitive_color({:index, value}) when palette_index(value) do
    {:ok, {:index, value}}
  end

  defp normalize_primitive_color(color) do
    case Color.normalize_display(color) do
      {:ok, color} -> {:ok, color}
      {:error, _reason} -> {:error, {:bad_render_color, color}}
    end
  end

  defp normalize_palette_color(color) do
    case Color.normalize_palette(color) do
      {:ok, color} -> {:ok, color}
      {:error, _reason} -> {:error, {:bad_palette_render_color, color}}
    end
  end

  defp prepend_default_target(commands, target) do
    case normalize_target(target) do
      {:ok, 0} ->
        {:ok, commands}

      {:ok, target} ->
        {:ok, [{:target, target} | commands]}

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp normalize_target(:lcd), do: {:ok, 0}
  defp normalize_target(target) when target_any(target), do: {:ok, target}
  defp normalize_target(target), do: {:error, {:bad_render_target, target}}

  defp normalize_all([], acc), do: {:ok, :lists.reverse(acc)}

  defp normalize_all([command | rest], acc) do
    case normalize_one(command) do
      {:ok, normalized_command} -> normalize_all(rest, [normalized_command | acc])
      {:error, reason} -> {:error, reason}
    end
  end

  defp insert_primitive_color_modes([], _color_mode, acc) do
    {:ok, :lists.reverse(acc)}
  end

  defp insert_primitive_color_modes([command | rest], color_mode, acc) do
    case primitive_color_mode(command) do
      nil ->
        insert_primitive_color_modes(rest, color_mode, [command | acc])

      {^color_mode, command} ->
        insert_primitive_color_modes(rest, color_mode, [command | acc])

      {next_color_mode, command} ->
        insert_primitive_color_modes(
          rest,
          next_color_mode,
          [command, {:color_mode, next_color_mode} | acc]
        )
    end
  end

  defp primitive_color_mode(command) when is_tuple(command) and tuple_size(command) >= 2 do
    op = elem(command, 0)

    if op in @primitive_color_ops do
      color_index = tuple_size(command) - 1

      case elem(command, color_index) do
        {:index, value} -> {:palette_index, put_elem(command, color_index, value)}
        _rgb565 -> {:rgb565, command}
      end
    else
      nil
    end
  end

  defp primitive_color_mode(_command), do: nil

  defp display_option(opts) do
    case Keyword.get(opts, :display, false) do
      display? when is_boolean(display?) -> {:ok, display?}
      display? -> {:error, {:bad_render_display_option, display?}}
    end
  end

  defp maybe_append_display(commands, true) do
    if :lists.member(:display, commands) do
      {:ok, commands}
    else
      {:ok, commands ++ [:display]}
    end
  end

  defp maybe_append_display(commands, false), do: {:ok, commands}
end
