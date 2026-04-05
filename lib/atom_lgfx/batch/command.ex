# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Batch.Command do
  @moduledoc """
  Explicit batch command descriptor.

  This module keeps the batch envelope generic while adding operation-specific
  builders for the current inline primitive slice.
  """

  import AtomLGFX.Guards

  alias AtomLGFX.Protocol

  @max_f32 3.4028234663852886e38

  @type t :: {:cmd, atom, 0..254, non_neg_integer, [term]}

  @spec new(atom, 0..254, non_neg_integer, [term]) :: {:ok, t} | {:error, term}
  def new(op, target, flags, args)
      when is_atom(op) and target_any(target) and is_integer(flags) and flags >= 0 and
             is_list(args) do
    {:ok, {:cmd, op, target, flags, args}}
  end

  def new(op, target, flags, args) do
    {:error, {:bad_batch_command, {op, target, flags, args}}}
  end

  @spec to_wire(t) :: {:ok, tuple} | {:error, term}
  def to_wire({:cmd, op, target, flags, args})
      when is_atom(op) and target_any(target) and is_integer(flags) and flags >= 0 and
             is_list(args) do
    {:ok, List.to_tuple([op, target, flags | args])}
  end

  def to_wire(other), do: {:error, {:bad_batch_command, other}}

  @spec fill_screen(term, 0..254) :: {:ok, t} | {:error, term}
  def fill_screen(color, target \\ 0) when target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:fillScreen, target, flags, [color_arg])
    end
  end

  @spec clear(term, 0..254) :: {:ok, t} | {:error, term}
  def clear(color, target \\ 0) when target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:clear, target, flags, [color_arg])
    end
  end

  @spec fill_rect(integer, integer, integer, integer, term, 0..254) ::
          {:ok, t} | {:error, term}
  def fill_rect(x, y, width, height, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:fillRect, target, flags, [x, y, width, height, color_arg])
    end
  end

  @spec draw_pixel(integer, integer, term, 0..254) :: {:ok, t} | {:error, term}
  def draw_pixel(x, y, color, target \\ 0)
      when i16(x) and i16(y) and target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawPixel, target, flags, [x, y, color_arg])
    end
  end

  @spec draw_rect(integer, integer, integer, integer, term, 0..254) ::
          {:ok, t} | {:error, term}
  def draw_rect(x, y, width, height, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawRect, target, flags, [x, y, width, height, color_arg])
    end
  end

  @spec draw_round_rect(integer, integer, integer, integer, integer, term, 0..254) ::
          {:ok, t} | {:error, term}
  def draw_round_rect(x, y, width, height, radius, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             u16(radius) and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawRoundRect, target, flags, [x, y, width, height, radius, color_arg])
    end
  end

  @spec fill_round_rect(integer, integer, integer, integer, integer, term, 0..254) ::
          {:ok, t} | {:error, term}
  def fill_round_rect(x, y, width, height, radius, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             u16(radius) and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:fillRoundRect, target, flags, [x, y, width, height, radius, color_arg])
    end
  end

  @spec draw_circle(integer, integer, integer, term, 0..254) :: {:ok, t} | {:error, term}
  def draw_circle(x, y, radius, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius) and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawCircle, target, flags, [x, y, radius, color_arg])
    end
  end

  @spec fill_circle(integer, integer, integer, term, 0..254) :: {:ok, t} | {:error, term}
  def fill_circle(x, y, radius, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius) and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:fillCircle, target, flags, [x, y, radius, color_arg])
    end
  end

  @spec draw_ellipse(integer, integer, integer, integer, term, 0..254) ::
          {:ok, t} | {:error, term}
  def draw_ellipse(x, y, radius_x, radius_y, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius_x) and radius_x >= 1 and
             u16(radius_y) and radius_y >= 1 and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawEllipse, target, flags, [x, y, radius_x, radius_y, color_arg])
    end
  end

  @spec fill_ellipse(integer, integer, integer, integer, term, 0..254) ::
          {:ok, t} | {:error, term}
  def fill_ellipse(x, y, radius_x, radius_y, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius_x) and radius_x >= 1 and
             u16(radius_y) and radius_y >= 1 and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:fillEllipse, target, flags, [x, y, radius_x, radius_y, color_arg])
    end
  end

  @spec draw_arc(integer, integer, integer, integer, number, number, term, 0..254) ::
          {:ok, t} | {:error, term}
  def draw_arc(x, y, radius0, radius1, angle0, angle1, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius0) and radius0 >= 1 and
             u16(radius1) and radius1 >= 1 and
             is_number(angle0) and is_number(angle1) and
             target_any(target) do
    with {:ok, normalized_angle0} <- normalize_angle(angle0),
         {:ok, normalized_angle1} <- normalize_angle(angle1),
         {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawArc, target, flags, [
        x,
        y,
        radius0,
        radius1,
        normalized_angle0,
        normalized_angle1,
        color_arg
      ])
    end
  end

  @spec fill_arc(integer, integer, integer, integer, number, number, term, 0..254) ::
          {:ok, t} | {:error, term}
  def fill_arc(x, y, radius0, radius1, angle0, angle1, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius0) and radius0 >= 1 and
             u16(radius1) and radius1 >= 1 and
             is_number(angle0) and is_number(angle1) and
             target_any(target) do
    with {:ok, normalized_angle0} <- normalize_angle(angle0),
         {:ok, normalized_angle1} <- normalize_angle(angle1),
         {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:fillArc, target, flags, [
        x,
        y,
        radius0,
        radius1,
        normalized_angle0,
        normalized_angle1,
        color_arg
      ])
    end
  end

  @spec draw_bezier3(integer, integer, integer, integer, integer, integer, term, 0..254) ::
          {:ok, t} | {:error, term}
  def draw_bezier3(x0, y0, x1, y1, x2, y2, color, target \\ 0)
      when i16(x0) and i16(y0) and
             i16(x1) and i16(y1) and
             i16(x2) and i16(y2) and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawBezier, target, flags, [x0, y0, x1, y1, x2, y2, color_arg])
    end
  end

  @spec draw_bezier4(
          integer,
          integer,
          integer,
          integer,
          integer,
          integer,
          integer,
          integer,
          term,
          0..254
        ) :: {:ok, t} | {:error, term}
  def draw_bezier4(x0, y0, x1, y1, x2, y2, x3, y3, color, target \\ 0)
      when i16(x0) and i16(y0) and
             i16(x1) and i16(y1) and
             i16(x2) and i16(y2) and
             i16(x3) and i16(y3) and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawBezier, target, flags, [x0, y0, x1, y1, x2, y2, x3, y3, color_arg])
    end
  end

  @spec draw_triangle(integer, integer, integer, integer, integer, integer, term, 0..254) ::
          {:ok, t} | {:error, term}
  def draw_triangle(x0, y0, x1, y1, x2, y2, color, target \\ 0)
      when i16(x0) and i16(y0) and
             i16(x1) and i16(y1) and
             i16(x2) and i16(y2) and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawTriangle, target, flags, [x0, y0, x1, y1, x2, y2, color_arg])
    end
  end

  @spec fill_triangle(integer, integer, integer, integer, integer, integer, term, 0..254) ::
          {:ok, t} | {:error, term}
  def fill_triangle(x0, y0, x1, y1, x2, y2, color, target \\ 0)
      when i16(x0) and i16(y0) and
             i16(x1) and i16(y1) and
             i16(x2) and i16(y2) and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:fillTriangle, target, flags, [x0, y0, x1, y1, x2, y2, color_arg])
    end
  end

  @spec draw_fast_vline(integer, integer, integer, term, 0..254) :: {:ok, t} | {:error, term}
  def draw_fast_vline(x, y, height, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(height) and height >= 1 and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawFastVLine, target, flags, [x, y, height, color_arg])
    end
  end

  @spec draw_fast_hline(integer, integer, integer, term, 0..254) :: {:ok, t} | {:error, term}
  def draw_fast_hline(x, y, width, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and width >= 1 and
             target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawFastHLine, target, flags, [x, y, width, color_arg])
    end
  end

  @spec draw_line(integer, integer, integer, integer, term, 0..254) :: {:ok, t} | {:error, term}
  def draw_line(x0, y0, x1, y1, color, target \\ 0)
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and target_any(target) do
    with {:ok, flags, color_arg} <- normalize_scalar_color_arg(color) do
      new(:drawLine, target, flags, [x0, y0, x1, y1, color_arg])
    end
  end

  @spec line(integer, integer, integer, integer, term, 0..254) :: {:ok, t} | {:error, term}
  def line(x0, y0, x1, y1, color, target \\ 0)
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and target_any(target) do
    cond do
      x0 == x1 and y0 == y1 ->
        draw_line(x0, y0, x1, y1, color, target)

      x0 == x1 ->
        start_y = min(y0, y1)
        height = abs(y1 - y0) + 1
        draw_fast_vline(x0, start_y, height, color, target)

      y0 == y1 ->
        start_x = min(x0, x1)
        width = abs(x1 - x0) + 1
        draw_fast_hline(start_x, y0, width, color, target)

      true ->
        draw_line(x0, y0, x1, y1, color, target)
    end
  end

  defp normalize_scalar_color_arg(color) when rgb565(color) do
    {:ok, 0, color}
  end

  defp normalize_scalar_color_arg({:rgb565, color}) when rgb565(color) do
    {:ok, 0, color}
  end

  defp normalize_scalar_color_arg({:index, index}) when palette_index(index) do
    {:ok, Protocol.color_index_flag(), index}
  end

  defp normalize_scalar_color_arg(other), do: {:error, {:bad_scalar_color, other}}

  defp normalize_angle(value)
       when is_integer(value) and value >= -@max_f32 and value <= @max_f32 do
    {:ok, value * 1.0}
  end

  defp normalize_angle(value)
       when is_float(value) and value == value and value >= -@max_f32 and value <= @max_f32 do
    {:ok, value}
  end

  defp normalize_angle(other), do: {:error, {:bad_angle, other}}
end
