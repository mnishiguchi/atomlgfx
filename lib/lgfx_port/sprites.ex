# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule LGFXPort.Sprites do
  @moduledoc false

  import LGFXPort.Guards

  alias LGFXPort.Protocol

  @valid_color_depths [1, 2, 4, 8, 16, 24]
  @max_f32 3.4028234663852886e38

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
      when sprite_handle(target) and palette_index(palette_index) and color888(rgb888) do
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

  def push_rotate_zoom_to(port, src_target, dst_target, x, y, angle_deg, zoom)
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             is_number(angle_deg) and
             is_number(zoom) and zoom > 0 do
    push_rotate_zoom_to(port, src_target, dst_target, x, y, angle_deg, zoom, zoom)
  end

  def push_rotate_zoom_to(
        port,
        src_target,
        dst_target,
        x,
        y,
        angle_deg,
        zoom_x,
        zoom_y
      )
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             is_number(angle_deg) and
             is_number(zoom_x) and zoom_x > 0 and
             is_number(zoom_y) and zoom_y > 0 do
    with {:ok, normalized_angle_deg} <- normalize_angle_deg(angle_deg),
         {:ok, normalized_zoom_x} <- normalize_zoom(zoom_x),
         {:ok, normalized_zoom_y} <- normalize_zoom(zoom_y) do
      Protocol.call_ok(
        port,
        :pushRotateZoom,
        src_target,
        0,
        [dst_target, x, y, normalized_angle_deg, normalized_zoom_x, normalized_zoom_y],
        Protocol.long_timeout()
      )
    end
  end

  def push_rotate_zoom_to(
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
    with {:ok, normalized_angle_deg} <- normalize_angle_deg(angle_deg),
         {:ok, normalized_zoom_x} <- normalize_zoom(zoom_x),
         {:ok, normalized_zoom_y} <- normalize_zoom(zoom_y),
         {:ok, flags, transparent_value} <- normalize_transparent_arg(transparent) do
      Protocol.call_ok(
        port,
        :pushRotateZoom,
        src_target,
        flags,
        [
          dst_target,
          x,
          y,
          normalized_angle_deg,
          normalized_zoom_x,
          normalized_zoom_y,
          transparent_value
        ],
        Protocol.long_timeout()
      )
    end
  end

  # Backward-compatible helper for callers that still have old centi-degree / x1024 values.
  def push_rotate_zoom_raw_to(
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
             positive_i32(zoom_x1024) and
             positive_i32(zoom_y1024) do
    push_rotate_zoom_to(
      port,
      src_target,
      dst_target,
      x,
      y,
      angle_centi_deg / 100.0,
      zoom_x1024 / 1024.0,
      zoom_y1024 / 1024.0
    )
  end

  # Backward-compatible helper for callers that still have old centi-degree / x1024 values.
  def push_rotate_zoom_raw_to(
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
             positive_i32(zoom_x1024) and
             positive_i32(zoom_y1024) do
    push_rotate_zoom_to(
      port,
      src_target,
      dst_target,
      x,
      y,
      angle_centi_deg / 100.0,
      zoom_x1024 / 1024.0,
      zoom_y1024 / 1024.0,
      transparent
    )
  end

  def push_rotate_zoom_deg_to(port, src_target, dst_target, x, y, angle_deg, zoom)
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             is_number(angle_deg) and
             is_number(zoom) and zoom > 0 do
    push_rotate_zoom_to(port, src_target, dst_target, x, y, angle_deg, zoom)
  end

  def push_rotate_zoom_deg_to(
        port,
        src_target,
        dst_target,
        x,
        y,
        angle_deg,
        zoom_x,
        zoom_y
      )
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             is_number(angle_deg) and
             is_number(zoom_x) and zoom_x > 0 and
             is_number(zoom_y) and zoom_y > 0 do
    push_rotate_zoom_to(port, src_target, dst_target, x, y, angle_deg, zoom_x, zoom_y)
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
    push_rotate_zoom_to(
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
  end

  defp normalize_transparent_arg(transparent) when rgb565(transparent) do
    {:ok, 0, transparent}
  end

  defp normalize_transparent_arg({:rgb565, transparent}) when rgb565(transparent) do
    {:ok, 0, transparent}
  end

  defp normalize_transparent_arg({:index, transparent_index})
       when palette_index(transparent_index) do
    {:ok, Protocol.transparent_index_flag(), transparent_index}
  end

  defp normalize_transparent_arg(other), do: {:error, {:bad_transparent_color, other}}

  defp normalize_angle_deg(value)
       when is_integer(value) and value >= -@max_f32 and value <= @max_f32 do
    {:ok, value * 1.0}
  end

  defp normalize_angle_deg(value)
       when is_float(value) and value >= -@max_f32 and value <= @max_f32 do
    {:ok, value}
  end

  defp normalize_angle_deg(value), do: {:error, {:bad_angle_deg, value}}

  defp normalize_zoom(value) when is_integer(value) and value > 0 and value <= @max_f32 do
    {:ok, value * 1.0}
  end

  defp normalize_zoom(value) when is_float(value) and value > 0.0 and value <= @max_f32 do
    {:ok, value}
  end

  defp normalize_zoom(value), do: {:error, {:bad_zoom, value}}
end
