defmodule LGFXPort.Primitives do
  @moduledoc false

  import LGFXPort.Guards

  alias LGFXPort.Protocol

  def fill_screen(port, color, target \\ 0)
      when target_any(target) do
    scalar_call(port, :fillScreen, target, [color])
  end

  def clear(port, color, target \\ 0)
      when target_any(target) do
    scalar_call(port, :clear, target, [color])
  end

  def draw_pixel(port, x, y, color, target \\ 0)
      when i16(x) and i16(y) and target_any(target) do
    scalar_call(port, :drawPixel, target, [x, y, color])
  end

  def draw_fast_vline(port, x, y, height, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(height) and
             target_any(target) do
    scalar_call(port, :drawFastVLine, target, [x, y, height, color])
  end

  def draw_fast_hline(port, x, y, width, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and
             target_any(target) do
    scalar_call(port, :drawFastHLine, target, [x, y, width, color])
  end

  def draw_line(port, x0, y0, x1, y1, color, target \\ 0)
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and
             target_any(target) do
    scalar_call(port, :drawLine, target, [x0, y0, x1, y1, color])
  end

  def draw_rect(port, x, y, width, height, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and
             u16(height) and
             target_any(target) do
    scalar_call(port, :drawRect, target, [x, y, width, height, color])
  end

  def fill_rect(port, x, y, width, height, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and
             u16(height) and
             target_any(target) do
    scalar_call(port, :fillRect, target, [x, y, width, height, color])
  end

  def draw_circle(port, x, y, radius, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius) and
             target_any(target) do
    scalar_call(port, :drawCircle, target, [x, y, radius, color])
  end

  def fill_circle(port, x, y, radius, color, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius) and
             target_any(target) do
    scalar_call(port, :fillCircle, target, [x, y, radius, color])
  end

  def draw_triangle(port, x0, y0, x1, y1, x2, y2, color, target \\ 0)
      when i16(x0) and i16(y0) and
             i16(x1) and i16(y1) and
             i16(x2) and i16(y2) and
             target_any(target) do
    scalar_call(port, :drawTriangle, target, [x0, y0, x1, y1, x2, y2, color])
  end

  def fill_triangle(port, x0, y0, x1, y1, x2, y2, color, target \\ 0)
      when i16(x0) and i16(y0) and
             i16(x1) and i16(y1) and
             i16(x2) and i16(y2) and
             target_any(target) do
    scalar_call(port, :fillTriangle, target, [x0, y0, x1, y1, x2, y2, color])
  end

  defp scalar_call(port, op, target, args) do
    with {:ok, prefix_args, color} <- split_args_and_color(args),
         {:ok, flags, wire_color} <- normalize_scalar_color_arg(color) do
      Protocol.call_ok(
        port,
        op,
        target,
        flags,
        prefix_args ++ [wire_color],
        Protocol.long_timeout()
      )
    end
  end

  defp split_args_and_color([]), do: {:error, :missing_scalar_color}
  defp split_args_and_color([color]), do: {:ok, [], color}

  defp split_args_and_color([head | tail]) do
    with {:ok, rest_args, color} <- split_args_and_color(tail) do
      {:ok, [head | rest_args], color}
    end
  end

  defp normalize_scalar_color_arg(color) when color888(color) do
    {:ok, 0, color}
  end

  defp normalize_scalar_color_arg({:rgb888, color}) when color888(color) do
    {:ok, 0, color}
  end

  defp normalize_scalar_color_arg({:index, palette_index}) when palette_index(palette_index) do
    {:ok, Protocol.color_index_flag(), palette_index}
  end

  defp normalize_scalar_color_arg(other) do
    {:error, {:bad_scalar_color, other}}
  end
end
