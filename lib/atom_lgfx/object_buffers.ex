# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.ObjectBuffers do
  @moduledoc false

  import AtomLGFX.Guards

  alias AtomLGFX.Protocol

  @layout_sprite_transform_2d 1
  @max_f32 3.4028234663852886e38

  def create_object_buffer(port, opts) when is_list(opts) do
    with {:ok, layout_id} <- normalize_layout(Keyword.get(opts, :layout)),
         {:ok, capacity} <- normalize_capacity(Keyword.get(opts, :capacity)) do
      case Protocol.call(
             port,
             :create_object_buffer,
             0,
             0,
             [layout_id, capacity],
             Protocol.long_timeout()
           ) do
        {:ok, handle} when u8(handle) and handle > 0 -> {:ok, handle}
        {:ok, other} -> {:error, {:bad_reply_value, :create_object_buffer, other}}
        {:error, reason} -> {:error, reason}
      end
    end
  end

  def write_object_buffer(port, handle, objects)
      when u8(handle) and handle > 0 and is_binary(objects) do
    Protocol.call_ok(port, :write_object_buffer, 0, 0, [handle, objects], Protocol.long_timeout())
  end

  def write_object_buffer(port, handle, objects)
      when u8(handle) and handle > 0 and is_list(objects) do
    with {:ok, encoded} <- encode_sprite_transform_2d_objects(objects) do
      write_object_buffer(port, handle, encoded)
    end
  end

  def delete_object_buffer(port, handle) when u8(handle) and handle > 0 do
    Protocol.call_ok(port, :delete_object_buffer, 0, 0, [handle], Protocol.long_timeout())
  end

  @doc false
  def encode_sprite_transform_2d_objects(objects) when is_list(objects) do
    case encode_sprite_transform_2d_objects_i(objects, 0, []) do
      {:ok, 0, _acc} -> {:ok, <<>>}
      {:ok, _count, acc} -> {:ok, :erlang.iolist_to_binary(:lists.reverse(acc))}
      {:error, _reason} = error -> error
    end
  end

  defp encode_sprite_transform_2d_objects_i([], count, acc), do: {:ok, count, acc}

  defp encode_sprite_transform_2d_objects_i([object | rest], count, acc) do
    with {:ok, record} <- encode_sprite_transform_2d_object(object) do
      encode_sprite_transform_2d_objects_i(rest, count + 1, [record | acc])
    end
  end

  defp encode_sprite_transform_2d_object(
         {source_index, x, y, vx, vy, angle_deg, zoom, dangle_deg, dzoom}
       )
       when palette_index(source_index) and i16(x) and i16(y) and i16(vx) and i16(vy) do
    with {:ok, angle_cdeg} <- normalize_angle_cdeg(angle_deg),
         {:ok, zoom_x1024} <- normalize_zoom_x1024(zoom),
         {:ok, dangle_cdeg} <- normalize_delta_angle_cdeg(dangle_deg),
         {:ok, dzoom_x1024} <- normalize_delta_zoom_x1024(dzoom) do
      {:ok,
       <<source_index, 0, x::little-signed-16, y::little-signed-16, vx::little-signed-16,
         vy::little-signed-16, angle_cdeg::little-16, zoom_x1024::little-16,
         dangle_cdeg::little-signed-16, dzoom_x1024::little-signed-16>>}
    end
  end

  defp encode_sprite_transform_2d_object(other),
    do: {:error, {:bad_retained_object_record, other}}

  defp normalize_layout(:sprite_transform_2d), do: {:ok, @layout_sprite_transform_2d}
  defp normalize_layout(other), do: {:error, {:bad_retained_object_buffer_layout, other}}

  defp normalize_capacity(value) when positive_i32(value) and value <= 0xFFFF,
    do: {:ok, value}

  defp normalize_capacity(other), do: {:error, {:bad_retained_object_buffer_capacity, other}}

  defp normalize_angle_cdeg(value)
       when is_integer(value) and value >= -@max_f32 and value <= @max_f32 do
    normalize_angle_cdeg(value * 1.0)
  end

  defp normalize_angle_cdeg(value)
       when is_float(value) and value >= -@max_f32 and value <= @max_f32 do
    normalized = rem(round(value * 100), 36_000)
    {:ok, if(normalized < 0, do: normalized + 36_000, else: normalized)}
  end

  defp normalize_angle_cdeg(other), do: {:error, {:bad_angle_deg, other}}

  defp normalize_delta_angle_cdeg(value)
       when is_integer(value) and value >= -@max_f32 and value <= @max_f32 do
    normalize_delta_angle_cdeg(value * 1.0)
  end

  defp normalize_delta_angle_cdeg(value)
       when is_float(value) and value >= -@max_f32 and value <= @max_f32 do
    scaled = round(value * 100)

    if scaled >= -32_768 and scaled <= 32_767 do
      {:ok, scaled}
    else
      {:error, {:bad_angle_deg, value}}
    end
  end

  defp normalize_delta_angle_cdeg(other), do: {:error, {:bad_angle_deg, other}}

  defp normalize_zoom_x1024(value) when is_integer(value) and value > 0 and value <= @max_f32 do
    normalize_zoom_x1024(value * 1.0)
  end

  defp normalize_zoom_x1024(value) when is_float(value) and value > 0.0 and value <= @max_f32 do
    scaled = round(value * 1024)

    if scaled >= 1 and scaled <= 0xFFFF do
      {:ok, scaled}
    else
      {:error, {:bad_zoom, value}}
    end
  end

  defp normalize_zoom_x1024(other), do: {:error, {:bad_zoom, other}}

  defp normalize_delta_zoom_x1024(value)
       when is_integer(value) and value >= -@max_f32 and value <= @max_f32 do
    normalize_delta_zoom_x1024(value * 1.0)
  end

  defp normalize_delta_zoom_x1024(value)
       when is_float(value) and value >= -@max_f32 and value <= @max_f32 do
    scaled = round(value * 1024)

    if scaled >= -32_768 and scaled <= 32_767 do
      {:ok, scaled}
    else
      {:error, {:bad_zoom, value}}
    end
  end

  defp normalize_delta_zoom_x1024(other), do: {:error, {:bad_zoom, other}}
end
