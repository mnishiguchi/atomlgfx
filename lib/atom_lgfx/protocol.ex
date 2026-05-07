# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Protocol do
  @moduledoc false

  @compile {:no_warn_undefined, :port}

  import Bitwise
  import AtomLGFX.Guards

  alias AtomLGFX.Cache
  alias AtomLGFX.OpSchema

  @proto_ver 2

  @t_short 5_000
  @t_long 10_000
  @t_touch_calibrate 60_000

  @f_text_has_bg 1 <<< 0
  @f_color_index 1 <<< 1
  @f_text_fg_index 1 <<< 2
  @f_text_bg_index 1 <<< 3
  @f_transparent_index 1 <<< 4

  @cap_sprite 1 <<< 0
  @cap_pushimage 1 <<< 1
  @cap_last_error 1 <<< 2
  @cap_touch 1 <<< 3
  @cap_palette 1 <<< 4
  @cap_batch 1 <<< 5
  @cap_retained_render 1 <<< 6

  def proto_ver, do: @proto_ver

  def short_timeout, do: @t_short
  def long_timeout, do: @t_long
  def touch_calibrate_timeout, do: @t_touch_calibrate

  def text_has_bg_flag, do: @f_text_has_bg
  def color_index_flag, do: @f_color_index
  def text_fg_index_flag, do: @f_text_fg_index
  def text_bg_index_flag, do: @f_text_bg_index
  def transparent_index_flag, do: @f_transparent_index

  def cap_sprite, do: @cap_sprite
  def cap_pushimage, do: @cap_pushimage
  def cap_last_error, do: @cap_last_error
  def cap_touch, do: @cap_touch
  def cap_palette, do: @cap_palette
  def cap_batch, do: @cap_batch
  def cap_retained_render, do: @cap_retained_render

  def opcode!(op_name), do: OpSchema.opcode!(op_name)

  # Raw protocol call for smoke tests. Target is intentionally not range-checked.
  def raw_call(port, op, target, flags, args, timeout \\ @t_short)
      when is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    with {:ok, opcode} <- normalize_opcode(op) do
      call_opcode(port, opcode, target, flags, args, timeout)
    end
  end

  def ping(port), do: call_ok(port, :ping, 0, 0, [], @t_short)

  def get_caps(port) do
    with {:ok, payload} <- call(port, :get_caps, 0, 0, [], @t_short) do
      decode_caps(payload)
    end
  end

  def get_last_error(port) do
    with {:ok, payload} <- call(port, :get_last_error, 0, 0, [], @t_short) do
      decode_last_error(payload)
    end
  end

  def width(port, target \\ 0) when target_any(target) do
    integer_query(port, :width, :width, target)
  end

  def height(port, target \\ 0) when target_any(target) do
    integer_query(port, :height, :height, target)
  end

  def get_presentation_strip_height(port) do
    integer_query(port, :get_presentation_strip_height, :presentation_strip_height, 0)
  end

  def supports_sprite?(port), do: supports_cap?(port, @cap_sprite)
  def supports_pushimage?(port), do: supports_cap?(port, @cap_pushimage)
  def supports_last_error?(port), do: supports_cap?(port, @cap_last_error)
  def supports_touch?(port), do: supports_cap?(port, @cap_touch)
  def supports_palette?(port), do: supports_cap?(port, @cap_palette)
  def supports_batch?(port), do: supports_cap?(port, @cap_batch)
  def supports_retained_render?(port), do: supports_cap?(port, @cap_retained_render)

  def max_binary_bytes(port) do
    case Cache.get_max_binary_bytes(port) do
      {:ok, value} ->
        {:ok, value}

      :error ->
        with {:ok, %{max_binary_bytes: max_binary_bytes}} <- get_caps(port) do
          Cache.put_max_binary_bytes(port, max_binary_bytes)
          {:ok, max_binary_bytes}
        end
    end
  end

  def call_ok(port, op, target, flags, args, timeout) do
    case call(port, op, target, flags, args, timeout) do
      {:ok, _result} -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

  def call(port, op, target, flags, args, timeout)
      when is_atom(op) and
             is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    with :ok <- OpSchema.validate_wire_call(op, args, flags),
         {:ok, opcode} <- normalize_opcode(op) do
      call_opcode(port, opcode, target, flags, args, timeout)
    end
  end

  def call_opcode(port, opcode, target, flags, args, timeout)
      when is_integer(opcode) and
             opcode >= 0 and
             is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    request = {:lgfx, @proto_ver, :call, opcode, target, flags, args}

    try do
      case :port.call(port, request, timeout) do
        {:ok, result} -> {:ok, result}
        {:error, reason} -> {:error, reason}
        other -> {:error, {:unexpected_reply, other}}
      end
    rescue
      error in UndefinedFunctionError -> {:error, {:port_call_exit, error}}
    catch
      :exit, reason -> {:error, {:port_call_exit, reason}}
    end
  end

  def submit_binary_batch(port, command_binary, timeout \\ @t_long)

  def submit_binary_batch(port, command_binary, timeout) when is_binary(command_binary) do
    with :ok <- validate_command_binary_size(port, command_binary, :submit_binary_batch) do
      call(port, :submit_binary_batch, 0, 0, [command_binary], timeout)
    end
  end

  def submit_binary_batch(_port, command_binary, _timeout) do
    {:error, {:bad_binary_batch, command_binary}}
  end

  defp normalize_opcode(op) when is_atom(op), do: OpSchema.opcode(op)
  defp normalize_opcode(op) when is_integer(op) and op >= 0, do: {:ok, op}
  defp normalize_opcode(op), do: {:error, {:unknown_lgfx_op, op}}

  defp validate_command_binary_size(_port, <<>>, _op_name), do: {:error, :empty_batch}

  defp validate_command_binary_size(port, command_binary, op_name) do
    payload_size = byte_size(command_binary)

    case max_binary_bytes(port) do
      {:ok, max_binary_bytes} when is_integer(max_binary_bytes) and max_binary_bytes > 0 ->
        if payload_size <= max_binary_bytes do
          :ok
        else
          {:error, {:binary_too_large, op_name, payload_size, max_binary_bytes}}
        end

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp decode_caps({:caps, proto_ver, max_binary_bytes, max_sprites, feature_bits})
       when is_integer(proto_ver) and
              is_integer(max_binary_bytes) and
              is_integer(max_sprites) and
              is_integer(feature_bits) do
    sprite_cap_set? = (feature_bits &&& @cap_sprite) != 0

    cond do
      proto_ver != @proto_ver ->
        {:error, {:bad_caps_proto_ver, @proto_ver, proto_ver}}

      max_binary_bytes <= 0 ->
        {:error, {:bad_caps_payload, {:max_binary_bytes, max_binary_bytes}}}

      max_sprites < 0 ->
        {:error, {:bad_caps_payload, {:max_sprites, max_sprites}}}

      not sprite_cap_set? and max_sprites != 0 ->
        {:error,
         {:bad_caps_payload, {:max_sprites_without_cap_sprite, max_sprites, feature_bits}}}

      feature_bits < 0 ->
        {:error, {:bad_caps_payload, {:feature_bits, feature_bits}}}

      true ->
        {:ok,
         %{
           proto_ver: proto_ver,
           max_binary_bytes: max_binary_bytes,
           max_sprites: max_sprites,
           feature_bits: feature_bits
         }}
    end
  end

  defp decode_caps(other), do: {:error, {:bad_caps_payload, other}}

  defp decode_last_error({:last_error, last_op, reason, last_flags, last_target, esp_err})
       when is_integer(last_flags) and
              is_integer(last_target) and
              is_integer(esp_err) do
    {:ok,
     %{
       last_op: decode_last_op(last_op),
       reason: reason,
       last_flags: last_flags,
       last_target: last_target,
       esp_err: esp_err
     }}
  end

  defp decode_last_error(other), do: {:error, {:bad_last_error_payload, other}}

  defp decode_last_op(last_op) when is_integer(last_op) do
    case OpSchema.name(last_op) do
      {:ok, op_name} -> op_name
      :error -> last_op
    end
  end

  defp decode_last_op(last_op), do: last_op

  defp integer_query(port, op, name, target) do
    with {:ok, value} <- call(port, op, target, 0, [], @t_short),
         true <- is_integer(value) do
      {:ok, value}
    else
      false -> {:error, {:bad_reply_value, name}}
      {:error, reason} -> {:error, reason}
    end
  end

  defp supports_cap?(port, cap_bit) do
    with {:ok, %{feature_bits: feature_bits}} <- get_caps(port) do
      {:ok, (feature_bits &&& cap_bit) != 0}
    end
  end
end
