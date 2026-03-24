# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Text do
  @moduledoc false

  import Bitwise
  import AtomLGFX.Guards

  alias AtomLGFX.Cache
  alias AtomLGFX.Protocol

  @font_preset_ascii 0
  @font_preset_jp 1

  @max_f32 3.4028234663852886e38

  def set_text_size(port, scale, target \\ 0)
      when is_number(scale) and scale > 0 and target_any(target) do
    with {:ok, normalized_scale} <- normalize_text_scale(scale),
         :ok <-
           Protocol.call_ok(
             port,
             :setTextSize,
             target,
             0,
             [normalized_scale],
             Protocol.long_timeout()
           ) do
      Cache.put_text_size(port, target, {normalized_scale, normalized_scale})
      :ok
    end
  end

  def set_text_size_xy(port, sx, sy, target \\ 0)
      when is_number(sx) and sx > 0 and is_number(sy) and sy > 0 and target_any(target) do
    with {:ok, normalized_sx} <- normalize_text_scale(sx),
         {:ok, normalized_sy} <- normalize_text_scale(sy),
         :ok <-
           Protocol.call_ok(
             port,
             :setTextSize,
             target,
             0,
             [normalized_sx, normalized_sy],
             Protocol.long_timeout()
           ) do
      Cache.put_text_size(port, target, {normalized_sx, normalized_sy})
      :ok
    end
  end

  def set_text_datum(port, datum, target \\ 0)
      when u8(datum) and target_any(target) do
    Protocol.call_ok(port, :setTextDatum, target, 0, [datum], Protocol.long_timeout())
  end

  def set_text_wrap(port, wrap, target \\ 0)
      when is_boolean(wrap) and target_any(target) do
    Protocol.call_ok(port, :setTextWrap, target, 0, [wrap], Protocol.long_timeout())
  end

  def set_text_wrap_xy(port, wrap_x, wrap_y, target \\ 0)
      when is_boolean(wrap_x) and is_boolean(wrap_y) and target_any(target) do
    Protocol.call_ok(port, :setTextWrap, target, 0, [wrap_x, wrap_y], Protocol.long_timeout())
  end

  def set_text_font_preset(port, preset, target \\ 0)
      when target_any(target) do
    with {:ok, preset_id, canonical_preset} <- font_preset_to_wire(preset),
         :ok <-
           Protocol.call_ok(
             port,
             :setTextFontPreset,
             target,
             0,
             [preset_id],
             Protocol.long_timeout()
           ) do
      Cache.put_text_font_selection(port, target, {:preset, canonical_preset})
      Cache.put_text_size(port, target, implied_text_scale_for_preset(canonical_preset))
      :ok
    end
  end

  def set_text_color(port, fg_color, bg_color \\ nil, target \\ 0)
      when target_any(target) do
    with {:ok, flags, args, desired} <- normalize_text_color_args(fg_color, bg_color),
         :ok <-
           Protocol.call_ok(port, :setTextColor, target, flags, args, Protocol.long_timeout()) do
      Cache.put_text_color(port, target, desired)
      :ok
    end
  end

  def set_cursor(port, x, y, target \\ 0)
      when i16(x) and i16(y) and target_any(target) do
    Protocol.call_ok(port, :setCursor, target, 0, [x, y], Protocol.long_timeout())
  end

  def get_cursor(port, target \\ 0) when target_any(target) do
    case Protocol.call(port, :getCursor, target, 0, [], Protocol.short_timeout()) do
      {:ok, {x, y}} when i16(x) and i16(y) ->
        {:ok, {x, y}}

      {:ok, other} ->
        {:error, {:bad_cursor_reply, other}}

      {:error, _reason} = error ->
        error
    end
  end

  def draw_string(port, x, y, text, target \\ 0)
      when i16(x) and i16(y) and is_binary(text) and target_any(target) do
    with :ok <- validate_nonempty_text_binary(text) do
      Protocol.call_ok(port, :drawString, target, 0, [x, y, text], Protocol.long_timeout())
    end
  end

  def print(port, text, target \\ 0)
      when is_binary(text) and target_any(target) do
    with :ok <- validate_stream_text_binary(text) do
      Protocol.call_ok(port, :print, target, 0, [text], Protocol.long_timeout())
    end
  end

  def println(port, text, target \\ 0)
      when is_binary(text) and target_any(target) do
    with :ok <- validate_stream_text_binary(text) do
      Protocol.call_ok(port, :println, target, 0, [text], Protocol.long_timeout())
    end
  end

  def draw_string_bg(port, x, y, fg_color, bg_color, scale, text, target \\ 0)
      when i16(x) and i16(y) and
             is_number(scale) and scale > 0 and
             is_binary(text) and
             target_any(target) do
    with :ok <- validate_nonempty_text_binary(text),
         :ok <- maybe_set_text_color(port, fg_color, bg_color, target),
         :ok <- maybe_set_text_size(port, scale, target),
         :ok <- draw_string(port, x, y, text, target) do
      :ok
    end
  end

  def reset_text_state(port, target \\ 0) when target_any(target) do
    Cache.erase_text_cache(port, target)
    :ok
  end

  defp validate_nonempty_text_binary(<<>>), do: {:error, :empty_text}

  defp validate_nonempty_text_binary(text) when is_binary(text) do
    if contains_nul?(text) do
      {:error, :text_contains_nul}
    else
      :ok
    end
  end

  defp validate_stream_text_binary(text) when is_binary(text) do
    if contains_nul?(text) do
      {:error, :text_contains_nul}
    else
      :ok
    end
  end

  defp contains_nul?(<<>>), do: false
  defp contains_nul?(<<0, _::binary>>), do: true

  defp contains_nul?(<<a, b, c, d, e, f, g, h, rest::binary>>) do
    a == 0 or b == 0 or c == 0 or d == 0 or e == 0 or f == 0 or g == 0 or h == 0 or
      contains_nul?(rest)
  end

  defp contains_nul?(<<_byte, rest::binary>>), do: contains_nul?(rest)

  defp font_preset_to_wire(:ascii), do: {:ok, @font_preset_ascii, :ascii}
  defp font_preset_to_wire(:jp), do: {:ok, @font_preset_jp, :jp}
  defp font_preset_to_wire(other), do: {:error, {:bad_font_preset, other}}

  defp implied_text_scale_for_preset(_preset), do: {1.0, 1.0}

  defp maybe_set_text_color(port, fg_color, bg_color, target) do
    with {:ok, _flags, _args, desired} <- normalize_text_color_args(fg_color, bg_color) do
      case Cache.get_text_color(port, target) do
        ^desired -> :ok
        _ -> set_text_color(port, fg_color, bg_color, target)
      end
    end
  end

  defp maybe_set_text_size(port, scale, target) do
    with {:ok, normalized_scale} <- normalize_text_scale(scale) do
      desired = {normalized_scale, normalized_scale}

      case Cache.get_text_size(port, target) do
        ^desired -> :ok
        _ -> set_text_size(port, scale, target)
      end
    end
  end

  defp normalize_text_color_args(fg_color, nil) do
    with {:ok, fg_flags, fg_arg, fg_desired} <-
           normalize_text_color_arg(fg_color, Protocol.text_fg_index_flag(), :fg) do
      {:ok, fg_flags, [fg_arg], {fg_desired, nil}}
    end
  end

  defp normalize_text_color_args(fg_color, bg_color) do
    with {:ok, fg_flags, fg_arg, fg_desired} <-
           normalize_text_color_arg(fg_color, Protocol.text_fg_index_flag(), :fg),
         {:ok, bg_flags, bg_arg, bg_desired} <-
           normalize_text_color_arg(bg_color, Protocol.text_bg_index_flag(), :bg) do
      flags = Protocol.text_has_bg_flag() ||| fg_flags ||| bg_flags
      {:ok, flags, [fg_arg, bg_arg], {fg_desired, bg_desired}}
    end
  end

  defp normalize_text_color_arg(color, _index_flag, _role) when rgb565(color) do
    {:ok, 0, color, {:rgb565, color}}
  end

  defp normalize_text_color_arg({:rgb565, color}, _index_flag, _role) when rgb565(color) do
    {:ok, 0, color, {:rgb565, color}}
  end

  defp normalize_text_color_arg({:index, index}, index_flag, _role) when palette_index(index) do
    {:ok, index_flag, index, {:index, index}}
  end

  defp normalize_text_color_arg(other, _index_flag, role),
    do: {:error, {:bad_text_color, role, other}}

  defp normalize_text_scale(value) when is_integer(value) and value > 0 and value <= @max_f32 do
    {:ok, value * 1.0}
  end

  defp normalize_text_scale(value) when is_float(value) and value > 0.0 and value <= @max_f32 do
    {:ok, value}
  end

  defp normalize_text_scale(value), do: {:error, {:bad_text_scale, value}}
end
