defmodule LGFXPort.Sprites do
  @moduledoc false

  import LGFXPort.Guards

  alias LGFXPort.Protocol

  @valid_color_depths [1, 2, 4, 8, 16, 24]

  def create_sprite(port, width, height, target)
      when u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             sprite_handle(target) do
    Protocol.call_ok(port, :createSprite, target, 0, [width, height], Protocol.long_timeout())
  end

  def create_sprite(port, width, height, color_depth, target)
      when u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             is_integer(color_depth) and color_depth in @valid_color_depths and
             sprite_handle(target) do
    Protocol.call_ok(
      port,
      :createSprite,
      target,
      0,
      [width, height, color_depth],
      Protocol.long_timeout()
    )
  end

  def delete_sprite(port, target) when sprite_handle(target) do
    Protocol.call_ok(port, :deleteSprite, target, 0, [], Protocol.long_timeout())
  end

  def create_palette(port, target) when sprite_handle(target) do
    Protocol.call_ok(port, :createPalette, target, 0, [], Protocol.long_timeout())
  end

  def set_palette_color(port, target, palette_index, rgb888)
      when sprite_handle(target) and u8(palette_index) and color888(rgb888) do
    Protocol.call_ok(
      port,
      :setPaletteColor,
      target,
      0,
      [palette_index, rgb888],
      Protocol.long_timeout()
    )
  end

  def set_pivot(port, target, x, y)
      when target_any(target) and i16(x) and i16(y) do
    Protocol.call_ok(port, :setPivot, target, 0, [x, y], Protocol.long_timeout())
  end

  def push_sprite_to(port, src_target, dst_target, x, y)
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) do
    Protocol.call_ok(
      port,
      :pushSprite,
      src_target,
      0,
      [dst_target, x, y],
      Protocol.long_timeout()
    )
  end

  def push_sprite_to(port, src_target, dst_target, x, y, transparent)
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) do
    with {:ok, flags, transparent_value} <- normalize_transparent_arg(transparent) do
      Protocol.call_ok(
        port,
        :pushSprite,
        src_target,
        flags,
        [dst_target, x, y, transparent_value],
        Protocol.long_timeout()
      )
    end
  end

  def push_sprite(port, src_target, x, y)
      when sprite_handle(src_target) and i16(x) and i16(y) do
    push_sprite_to(port, src_target, 0, x, y)
  end

  def push_sprite(port, src_target, x, y, transparent)
      when sprite_handle(src_target) and i16(x) and i16(y) do
    push_sprite_to(port, src_target, 0, x, y, transparent)
  end

  def push_rotate_zoom_to(
        port,
        src_target,
        dst_target,
        x,
        y,
        angle_centi_deg,
        zoom_x1024,
        zoom_y1024
      )
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             i32(angle_centi_deg) and
             i32(zoom_x1024) and zoom_x1024 > 0 and
             i32(zoom_y1024) and zoom_y1024 > 0 do
    Protocol.call_ok(
      port,
      :pushRotateZoom,
      src_target,
      0,
      [dst_target, x, y, angle_centi_deg, zoom_x1024, zoom_y1024],
      Protocol.long_timeout()
    )
  end

  def push_rotate_zoom_to(
        port,
        src_target,
        dst_target,
        x,
        y,
        angle_centi_deg,
        zoom_x1024,
        zoom_y1024,
        transparent
      )
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             i32(angle_centi_deg) and
             i32(zoom_x1024) and zoom_x1024 > 0 and
             i32(zoom_y1024) and zoom_y1024 > 0 do
    with {:ok, flags, transparent_value} <- normalize_transparent_arg(transparent) do
      Protocol.call_ok(
        port,
        :pushRotateZoom,
        src_target,
        flags,
        [dst_target, x, y, angle_centi_deg, zoom_x1024, zoom_y1024, transparent_value],
        Protocol.long_timeout()
      )
    end
  end

  def push_rotate_zoom_deg_to(port, src_target, dst_target, x, y, angle_deg, zoom_x, zoom_y)
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             is_number(angle_deg) and
             is_number(zoom_x) and zoom_x > 0 and
             is_number(zoom_y) and zoom_y > 0 do
    angle_centi_deg = round(angle_deg * 100)
    zx1024 = max(1, round(zoom_x * 1024))
    zy1024 = max(1, round(zoom_y * 1024))

    push_rotate_zoom_to(port, src_target, dst_target, x, y, angle_centi_deg, zx1024, zy1024)
  end

  def push_rotate_zoom_deg_to(
        port,
        src_target,
        dst_target,
        x,
        y,
        angle_deg,
        zoom_x,
        zoom_y,
        transparent
      )
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             is_number(angle_deg) and
             is_number(zoom_x) and zoom_x > 0 and
             is_number(zoom_y) and zoom_y > 0 do
    angle_centi_deg = round(angle_deg * 100)
    zx1024 = max(1, round(zoom_x * 1024))
    zy1024 = max(1, round(zoom_y * 1024))

    push_rotate_zoom_to(
      port,
      src_target,
      dst_target,
      x,
      y,
      angle_centi_deg,
      zx1024,
      zy1024,
      transparent
    )
  end

  def push_rotate_zoom_deg_to(port, src_target, dst_target, x, y, angle_deg, zoom)
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             is_number(angle_deg) and
             is_number(zoom) and zoom > 0 do
    push_rotate_zoom_deg_to(port, src_target, dst_target, x, y, angle_deg, zoom, zoom)
  end

  defp normalize_transparent_arg(transparent) when rgb565(transparent) do
    {:ok, 0, transparent}
  end

  defp normalize_transparent_arg({:rgb565, transparent}) when rgb565(transparent) do
    {:ok, 0, transparent}
  end

  defp normalize_transparent_arg({:index, transparent_index}) when u8(transparent_index) do
    {:ok, Protocol.transparent_index_flag(), transparent_index}
  end

  defp normalize_transparent_arg(other), do: {:error, {:bad_transparent_color, other}}
end
