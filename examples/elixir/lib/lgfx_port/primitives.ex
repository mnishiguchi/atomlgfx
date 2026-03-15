defmodule LGFXPort.Primitives do
  @moduledoc false

  import LGFXPort.Guards

  alias LGFXPort.Protocol

  def fill_screen(port, color888, target \\ 0)
      when color888(color888) and target_any(target) do
    Protocol.call_ok(port, :fillScreen, target, 0, [color888], Protocol.long_timeout())
  end

  def clear(port, color888, target \\ 0)
      when color888(color888) and target_any(target) do
    Protocol.call_ok(port, :clear, target, 0, [color888], Protocol.long_timeout())
  end

  def draw_pixel(port, x, y, color888, target \\ 0)
      when i16(x) and i16(y) and
             color888(color888) and
             target_any(target) do
    Protocol.call_ok(port, :drawPixel, target, 0, [x, y, color888], Protocol.long_timeout())
  end

  def draw_fast_vline(port, x, y, height, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(height) and
             color888(color888) and
             target_any(target) do
    Protocol.call_ok(
      port,
      :drawFastVLine,
      target,
      0,
      [x, y, height, color888],
      Protocol.long_timeout()
    )
  end

  def draw_fast_hline(port, x, y, width, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and
             color888(color888) and
             target_any(target) do
    Protocol.call_ok(
      port,
      :drawFastHLine,
      target,
      0,
      [x, y, width, color888],
      Protocol.long_timeout()
    )
  end

  def draw_line(port, x0, y0, x1, y1, color888, target \\ 0)
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and
             color888(color888) and
             target_any(target) do
    Protocol.call_ok(
      port,
      :drawLine,
      target,
      0,
      [x0, y0, x1, y1, color888],
      Protocol.long_timeout()
    )
  end

  def draw_rect(port, x, y, width, height, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and
             u16(height) and
             color888(color888) and
             target_any(target) do
    Protocol.call_ok(
      port,
      :drawRect,
      target,
      0,
      [x, y, width, height, color888],
      Protocol.long_timeout()
    )
  end

  def fill_rect(port, x, y, width, height, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and
             u16(height) and
             color888(color888) and
             target_any(target) do
    Protocol.call_ok(
      port,
      :fillRect,
      target,
      0,
      [x, y, width, height, color888],
      Protocol.long_timeout()
    )
  end

  def draw_circle(port, x, y, radius, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius) and
             color888(color888) and
             target_any(target) do
    Protocol.call_ok(
      port,
      :drawCircle,
      target,
      0,
      [x, y, radius, color888],
      Protocol.long_timeout()
    )
  end

  def fill_circle(port, x, y, radius, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius) and
             color888(color888) and
             target_any(target) do
    Protocol.call_ok(
      port,
      :fillCircle,
      target,
      0,
      [x, y, radius, color888],
      Protocol.long_timeout()
    )
  end

  def draw_triangle(port, x0, y0, x1, y1, x2, y2, color888, target \\ 0)
      when i16(x0) and i16(y0) and
             i16(x1) and i16(y1) and
             i16(x2) and i16(y2) and
             color888(color888) and
             target_any(target) do
    Protocol.call_ok(
      port,
      :drawTriangle,
      target,
      0,
      [x0, y0, x1, y1, x2, y2, color888],
      Protocol.long_timeout()
    )
  end

  def fill_triangle(port, x0, y0, x1, y1, x2, y2, color888, target \\ 0)
      when i16(x0) and i16(y0) and
             i16(x1) and i16(y1) and
             i16(x2) and i16(y2) and
             color888(color888) and
             target_any(target) do
    Protocol.call_ok(
      port,
      :fillTriangle,
      target,
      0,
      [x0, y0, x1, y1, x2, y2, color888],
      Protocol.long_timeout()
    )
  end
end
