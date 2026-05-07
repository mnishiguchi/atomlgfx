# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.RenderProgram do
  @moduledoc """
  Retained native render-program lifecycle helpers.
  """

  import AtomLGFX.Guards

  alias AtomLGFX.Protocol

  @program_type_striped_sprite_transform 1
  @update_none 0
  @update_bounce 1
  @mode_exclusive 1
  @max_f32 3.4028234663852886e38

  def create(port, opts) when is_list(opts) do
    with {:ok, type_id} <- normalize_program_type(Keyword.get(opts, :type)),
         {:ok, object_buffer} <- normalize_object_buffer_handle(Keyword.get(opts, :object_buffer)),
         {:ok, sources_binary} <- encode_sources(Keyword.get(opts, :sources)),
         {:ok, strip_height} <- normalize_strip_height(Keyword.get(opts, :strip_height)),
         {:ok, background_color} <-
           normalize_background_color(Keyword.get(opts, :background_color)),
         {:ok, update_policy} <- normalize_update_policy(Keyword.get(opts, :update, :none)),
         {:ok, has_transparent, transparent_value} <-
           normalize_transparent_color(Keyword.get(opts, :transparent_color, nil)),
         {:ok, zoom_min_x1024, zoom_max_x1024} <-
           normalize_zoom_bounds(
             Keyword.get(opts, :zoom_min, 0.5),
             Keyword.get(opts, :zoom_max, 2.0)
           ) do
      case Protocol.call(
             port,
             :create_render_program,
             0,
             0,
             [
               type_id,
               object_buffer,
               sources_binary,
               strip_height,
               background_color,
               update_policy,
               has_transparent,
               transparent_value,
               zoom_min_x1024,
               zoom_max_x1024
             ],
             Protocol.long_timeout()
           ) do
        {:ok, handle} when u8(handle) and handle > 0 -> {:ok, handle}
        {:ok, other} -> {:error, {:bad_reply_value, :create_render_program, other}}
        {:error, reason} -> {:error, reason}
      end
    end
  end

  def start(port, handle, opts \\ []) when u8(handle) and handle > 0 and is_list(opts) do
    with {:ok, mode_id} <- normalize_mode(Keyword.get(opts, :mode, :exclusive)) do
      Protocol.call_ok(
        port,
        :start_render_program,
        0,
        0,
        [handle, mode_id],
        Protocol.long_timeout()
      )
    end
  end

  def stop(port, handle) when u8(handle) and handle > 0 do
    Protocol.call_ok(port, :stop_render_program, 0, 0, [handle], Protocol.long_timeout())
  end

  def destroy(port, handle) when u8(handle) and handle > 0 do
    Protocol.call_ok(port, :destroy_render_program, 0, 0, [handle], Protocol.long_timeout())
  end

  def stats(port, handle) when u8(handle) and handle > 0 do
    with {:ok, payload} <-
           Protocol.call(port, :get_render_program_stats, 0, 0, [handle], Protocol.long_timeout()) do
      decode_stats(payload)
    end
  end

  @doc false
  def decode_stats(
        {:render_program_stats, running, frame_count, object_count, drawn_count, culled_count,
         strip_height, last_frame_us, last_update_us, last_draw_us, last_present_us}
      )
      when is_boolean(running) and is_integer(frame_count) and is_integer(object_count) and
             is_integer(drawn_count) and is_integer(culled_count) and is_integer(strip_height) and
             is_integer(last_frame_us) and is_integer(last_update_us) and
             is_integer(last_draw_us) and is_integer(last_present_us) do
    {:ok,
     %{
       running: running,
       frame_count: frame_count,
       object_count: object_count,
       drawn_count: drawn_count,
       culled_count: culled_count,
       strip_height: strip_height,
       last_frame_us: last_frame_us,
       last_update_us: last_update_us,
       last_draw_us: last_draw_us,
       last_present_us: last_present_us
     }}
  end

  def decode_stats(other), do: {:error, {:bad_render_program_stats_payload, other}}

  defp normalize_program_type(:striped_sprite_transform),
    do: {:ok, @program_type_striped_sprite_transform}

  defp normalize_program_type(other),
    do: {:error, {:bad_retained_render_program_type, other}}

  defp normalize_update_policy(:none), do: {:ok, @update_none}
  defp normalize_update_policy(:bounce), do: {:ok, @update_bounce}
  defp normalize_update_policy(other), do: {:error, {:bad_retained_render_update, other}}

  defp normalize_mode(:exclusive), do: {:ok, @mode_exclusive}
  defp normalize_mode(other), do: {:error, {:bad_retained_render_program_mode, other}}

  defp normalize_object_buffer_handle(handle) when u8(handle) and handle > 0, do: {:ok, handle}

  defp normalize_object_buffer_handle(other),
    do: {:error, {:bad_retained_object_buffer_handle, other}}

  defp normalize_strip_height(value) when positive_i32(value) and value <= 0xFFFF,
    do: {:ok, value}

  defp normalize_strip_height(other), do: {:error, {:bad_retained_render_strip_height, other}}

  defp normalize_background_color(value) when rgb565(value), do: {:ok, value}
  defp normalize_background_color(other), do: {:error, {:bad_scalar_color, other}}

  defp normalize_transparent_color(nil), do: {:ok, false, 0}
  defp normalize_transparent_color(value) when rgb565(value), do: {:ok, true, value}
  defp normalize_transparent_color(other), do: {:error, {:bad_transparent_color, other}}

  defp normalize_zoom_bounds(min_zoom, max_zoom) do
    with {:ok, min_zoom_x1024} <- normalize_zoom_x1024(min_zoom),
         {:ok, max_zoom_x1024} <- normalize_zoom_x1024(max_zoom) do
      if min_zoom_x1024 <= max_zoom_x1024 do
        {:ok, min_zoom_x1024, max_zoom_x1024}
      else
        {:error, {:bad_retained_render_zoom_bounds, min_zoom, max_zoom}}
      end
    end
  end

  defp encode_sources(sources) when is_list(sources) and sources != [] do
    try do
      encoded =
        Enum.map(sources, fn
          handle when sprite_handle(handle) -> handle
          other -> throw({:bad_retained_render_sources, other})
        end)
        |> :erlang.list_to_binary()

      {:ok, encoded}
    catch
      {:bad_retained_render_sources, value} ->
        {:error, {:bad_retained_render_sources, value}}
    end
  end

  defp encode_sources(other), do: {:error, {:bad_retained_render_sources, other}}

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
end
