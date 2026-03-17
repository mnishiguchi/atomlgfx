defmodule LGFXPort.Text do
  @moduledoc false

  import Bitwise
  import LGFXPort.Guards

  alias LGFXPort.Cache
  alias LGFXPort.Protocol

  @font_preset_ascii 0
  @font_preset_jp 1

  @text_scale_one_x256 256
  @text_scale_min_x256 1
  @text_scale_max_x256 0xFFFF

  @f_text_has_bg 1 <<< 0
  @f_text_fg_index 1 <<< 2
  @f_text_bg_index 1 <<< 3

  def set_text_size(port, scale, target \\ 0)
      when is_number(scale) and scale > 0 and target_any(target) do
    with {:ok, scale_x256} <- normalize_text_scale_x256(scale),
         :ok <-
           Protocol.call_ok(port, :setTextSize, target, 0, [scale_x256], Protocol.long_timeout()) do
      Cache.put_text_size(port, target, {scale_x256, scale_x256})
      :ok
    end
  end

  def set_text_size_xy(port, sx, sy, target \\ 0)
      when is_number(sx) and sx > 0 and is_number(sy) and sy > 0 and target_any(target) do
    with {:ok, sx_x256} <- normalize_text_scale_x256(sx),
         {:ok, sy_x256} <- normalize_text_scale_x256(sy),
         :ok <-
           Protocol.call_ok(
             port,
             :setTextSize,
             target,
             0,
             [sx_x256, sy_x256],
             Protocol.long_timeout()
           ) do
      Cache.put_text_size(port, target, {sx_x256, sy_x256})
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
      Cache.put_text_size(port, target, implied_text_scale_x256_for_preset(canonical_preset))
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
    case Protocol.raw_call(port, :getCursor, target, 0, [], Protocol.long_timeout()) do
      {:ok, {x, y}} when is_integer(x) and is_integer(y) ->
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

  defp implied_text_scale_x256_for_preset(:ascii),
    do: {@text_scale_one_x256, @text_scale_one_x256}

  defp implied_text_scale_x256_for_preset(:jp),
    do: {@text_scale_one_x256, @text_scale_one_x256}

  defp maybe_set_text_color(port, fg_color, bg_color, target) do
    with {:ok, _flags, _args, desired} <- normalize_text_color_args(fg_color, bg_color) do
      case Cache.get_text_color(port, target) do
        ^desired -> :ok
        _ -> set_text_color(port, fg_color, bg_color, target)
      end
    end
  end

  defp maybe_set_text_size(port, scale, target) do
    with {:ok, scale_x256} <- normalize_text_scale_x256(scale) do
      desired = {scale_x256, scale_x256}

      case Cache.get_text_size(port, target) do
        ^desired -> :ok
        _ -> set_text_size(port, scale, target)
      end
    end
  end

  defp normalize_text_color_args(fg_color, nil) do
    with {:ok, fg_flags, fg_arg, fg_desired} <-
           normalize_text_color_arg(fg_color, @f_text_fg_index, :fg) do
      {:ok, fg_flags, [fg_arg], {fg_desired, nil}}
    end
  end

  defp normalize_text_color_args(fg_color, bg_color) do
    with {:ok, fg_flags, fg_arg, fg_desired} <-
           normalize_text_color_arg(fg_color, @f_text_fg_index, :fg),
         {:ok, bg_flags, bg_arg, bg_desired} <-
           normalize_text_color_arg(bg_color, @f_text_bg_index, :bg) do
      flags = @f_text_has_bg ||| fg_flags ||| bg_flags
      {:ok, flags, [fg_arg, bg_arg], {fg_desired, bg_desired}}
    end
  end

  defp normalize_text_color_arg(color, _index_flag, _role) when color888(color) do
    {:ok, 0, color, {:rgb888, color}}
  end

  defp normalize_text_color_arg({:rgb888, color}, _index_flag, _role) when color888(color) do
    {:ok, 0, color, {:rgb888, color}}
  end

  defp normalize_text_color_arg({:index, index}, index_flag, _role) when palette_index(index) do
    {:ok, index_flag, index, {:index, index}}
  end

  defp normalize_text_color_arg(other, _index_flag, role),
    do: {:error, {:bad_text_color, role, other}}

  defp normalize_text_scale_x256(value) when is_number(value) and value > 0 do
    scale_x256 =
      cond do
        is_integer(value) -> value * @text_scale_one_x256
        is_float(value) -> round(value * @text_scale_one_x256)
      end

    if scale_x256 >= @text_scale_min_x256 and scale_x256 <= @text_scale_max_x256 do
      {:ok, scale_x256}
    else
      {:error, {:bad_text_scale, value}}
    end
  end

  defp normalize_text_scale_x256(value), do: {:error, {:bad_text_scale, value}}
end
