# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Sprites do
  @moduledoc false

  import Bitwise
  import AtomLGFX.Guards

  alias AtomLGFX.Protocol

  @valid_color_depths [1, 2, 4, 8, 16, 24]
  @max_f32 3.4028234663852886e38
  @przl_option_has_transparent 0x01
  @przl_option_approx_cull 0x02

  def create_sprite(port, width, height, target)
      when u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             sprite_handle(target) do
    Protocol.call_ok(port, :create_sprite, target, 0, [width, height], Protocol.long_timeout())
  end

  def create_sprite(port, width, height, color_depth, target)
      when u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             is_integer(color_depth) and color_depth in @valid_color_depths and
             sprite_handle(target) do
    Protocol.call_ok(
      port,
      :create_sprite,
      target,
      0,
      [width, height, color_depth],
      Protocol.long_timeout()
    )
  end

  def delete_sprite(port, target) when sprite_handle(target) do
    Protocol.call_ok(port, :delete_sprite, target, 0, [], Protocol.long_timeout())
  end

  def create_palette(port, target) when sprite_handle(target) do
    Protocol.call_ok(port, :create_palette, target, 0, [], Protocol.long_timeout())
  end

  def set_palette_color(port, target, palette_index, rgb888)
      when sprite_handle(target) and palette_index(palette_index) and color888(rgb888) do
    Protocol.call_ok(
      port,
      :set_palette_color,
      target,
      0,
      [palette_index, rgb888],
      Protocol.long_timeout()
    )
  end

  def set_pivot(port, target, x, y)
      when target_any(target) and i16(x) and i16(y) do
    Protocol.call_ok(port, :set_pivot, target, 0, [x, y], Protocol.long_timeout())
  end

  def push_sprite_to(port, src_target, dst_target, x, y)
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) do
    Protocol.call_ok(
      port,
      :push_sprite,
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
        :push_sprite,
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
        :push_rotate_zoom,
        src_target,
        0,
        [dst_target, x, y, normalized_angle_deg, normalized_zoom_x, normalized_zoom_y],
        Protocol.long_timeout()
      )
    end
  end

  def push_rotate_zoom_list_to(port, dst_target, instances, opts \\ [])
      when target_any(dst_target) and is_list(instances) and is_list(opts) do
    with {:ok, flags, payload} <- encode_push_rotate_zoom_list_payload(instances, opts) do
      Protocol.call_ok(
        port,
        :push_rotate_zoom_list,
        dst_target,
        flags,
        [payload],
        Protocol.long_timeout()
      )
    end
  end

  @doc false
  def encode_push_rotate_zoom_list_payload(instances, opts \\ [])
      when is_list(instances) and is_list(opts) do
    with {:ok, y_offset} <- normalize_y_offset(Keyword.get(opts, :y_offset, 0)),
         {:ok, flags, options, transparent_value} <- normalize_list_options(opts),
         {:ok, count, records} <- encode_push_rotate_zoom_instances(instances) do
      payload =
        [
          <<?P, ?R, ?Z, ?L, 1, options, transparent_value::little-16, y_offset::little-signed-16,
            count::little-16>>,
          records
        ]
        |> :erlang.iolist_to_binary()

      {:ok, flags, payload}
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
        :push_rotate_zoom,
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

  defp normalize_list_options(opts) do
    with {:ok, flags, options, transparent_value} <- normalize_list_transparent(opts),
         {:ok, options} <- normalize_approx_cull_option(opts, options) do
      {:ok, flags, options, transparent_value}
    end
  end

  defp normalize_list_transparent(opts) do
    if Keyword.has_key?(opts, :transparent) do
      with {:ok, flags, transparent_value} <-
             normalize_transparent_arg(Keyword.fetch!(opts, :transparent)) do
        {:ok, flags, @przl_option_has_transparent, transparent_value}
      end
    else
      {:ok, 0, 0, 0}
    end
  end

  defp normalize_approx_cull_option(opts, options) do
    case Keyword.get(opts, :approx_cull, false) do
      false -> {:ok, options}
      true -> {:ok, options ||| @przl_option_approx_cull}
      other -> {:error, {:bad_approx_cull, other}}
    end
  end

  @doc false
  def encode_push_rotate_zoom_records(instances) when is_list(instances) do
    encode_push_rotate_zoom_instances(instances)
  end

  defp encode_push_rotate_zoom_instances(instances) do
    case encode_push_rotate_zoom_instances_i(instances, 0, []) do
      {:ok, 0, _records} -> {:error, :empty_batch}
      other -> other
    end
  end

  defp encode_push_rotate_zoom_instances_i([], count, acc), do: {:ok, count, :lists.reverse(acc)}

  defp encode_push_rotate_zoom_instances_i([instance | rest], count, acc)
       when count < 0xFFFF do
    with {:ok, src, x, y, angle_cdeg, zoom_x1024, zoom_y1024} <-
           normalize_push_rotate_zoom_instance(instance) do
      record =
        <<src, 0, x::little-signed-16, y::little-signed-16, angle_cdeg::little-16,
          zoom_x1024::little-16, zoom_y1024::little-16>>

      encode_push_rotate_zoom_instances_i(rest, count + 1, [record | acc])
    end
  end

  defp encode_push_rotate_zoom_instances_i([_instance | _rest], count, _acc) do
    {:error, {:too_many_sprite_transform_instances, count + 1}}
  end

  defp normalize_push_rotate_zoom_instance({src, x, y, angle_cdeg, zoom_x1024, zoom_y1024})
       when sprite_handle(src) and i16(x) and i16(y) and
              is_integer(angle_cdeg) and angle_cdeg >= 0 and angle_cdeg < 36_000 and
              u16(zoom_x1024) and zoom_x1024 > 0 and
              u16(zoom_y1024) and zoom_y1024 > 0 do
    {:ok, src, x, y, angle_cdeg, zoom_x1024, zoom_y1024}
  end

  defp normalize_push_rotate_zoom_instance(other),
    do: {:error, {:bad_sprite_transform_instance, other}}

  defp normalize_y_offset(value) when i16(value), do: {:ok, value}
  defp normalize_y_offset(other), do: {:error, {:bad_y_offset, other}}

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
