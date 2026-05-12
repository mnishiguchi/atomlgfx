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

  @proto_ver 3
  @max_binary_bytes 256 * 1024

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

  def opcode!(op_name), do: OpSchema.opcode!(op_name)

  @doc false
  def __encode_v3_request__(op_name, target, flags, args)
      when is_atom(op_name) and
             is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    encode_v3_request(op_name, target, flags, args)
  end

  # Raw protocol call for smoke tests. Target is intentionally not range-checked.
  def raw_call(port, op, target, flags, args, timeout \\ @t_short)
      when is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    with {:ok, op_name} <- normalize_op_name(op) do
      call_named(port, op_name, target, flags, args, timeout)
    end
  end

  def ping(port), do: call_ok(port, :ping, 0, 0, [], @t_short)

  def get_caps(port) do
    case call(port, :get_caps, 0, 0, [], @t_short) do
      {:ok, feature_bits} when is_integer(feature_bits) and feature_bits >= 0 ->
        {:ok, feature_bits}

      {:ok, other} ->
        {:error, {:bad_caps_payload, other}}

      {:error, reason} ->
        {:error, reason}
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

  def supports_sprite?(port), do: supports_cap?(port, @cap_sprite)
  def supports_pushimage?(port), do: supports_cap?(port, @cap_pushimage)
  def supports_last_error?(port), do: supports_cap?(port, @cap_last_error)
  def supports_touch?(port), do: supports_cap?(port, @cap_touch)
  def supports_palette?(port), do: supports_cap?(port, @cap_palette)
  def supports_batch?(port), do: supports_cap?(port, @cap_batch)

  def max_binary_bytes(port) do
    case Cache.get_max_binary_bytes(port) do
      {:ok, value} -> {:ok, value}
      :error -> {:ok, @max_binary_bytes}
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
         {:ok, op_name} <- normalize_op_name(op) do
      call_named(port, op_name, target, flags, args, timeout)
    end
  end

  def call_opcode(port, opcode, target, flags, args, timeout)
      when is_integer(opcode) and
             opcode >= 0 and
             is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    with {:ok, op_name} <- normalize_op_name(opcode) do
      call_named(port, op_name, target, flags, args, timeout)
    end
  end

  defp call_named(port, op_name, target, flags, args, timeout) do
    request = encode_v3_request(op_name, target, flags, args)

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
    submit_binary_batch(port, 0, command_binary, timeout)
  end

  def submit_binary_batch(port, target, command_binary)
      when target_any(target) and is_binary(command_binary) do
    submit_binary_batch(port, target, command_binary, @t_long)
  end

  def submit_binary_batch(_port, target, command_binary) when target_any(target) do
    {:error, {:bad_binary_batch, command_binary}}
  end

  def submit_binary_batch(_port, command_binary, _timeout) do
    {:error, {:bad_binary_batch, command_binary}}
  end

  def submit_binary_batch(port, target, command_binary, timeout)
      when target_any(target) and is_binary(command_binary) do
    with :ok <- validate_command_binary_size(port, command_binary, :submit_binary_batch) do
      call(port, :submit_binary_batch, target, 0, [command_binary], timeout)
    end
  end

  def submit_binary_batch(_port, _target, command_binary, _timeout) do
    {:error, {:bad_binary_batch, command_binary}}
  end

  defp normalize_op_name(op) when is_atom(op) do
    case OpSchema.canonical_name(op) do
      {:ok, op_name} -> {:ok, op_name}
      :error -> {:error, {:unknown_lgfx_op, op}}
    end
  end

  defp normalize_op_name(opcode) when is_integer(opcode) and opcode >= 0 do
    case OpSchema.name(opcode) do
      {:ok, op_name} -> {:ok, op_name}
      :error -> {:error, {:unknown_lgfx_op, opcode}}
    end
  end

  defp normalize_op_name(op), do: {:error, {:unknown_lgfx_op, op}}

  defp encode_v3_request(op_name, target, flags, args) do
    {:ok, meta} = OpSchema.meta(op_name)
    target_policy = Keyword.fetch!(meta, :target_policy)

    cond do
      op_name == :submit_binary_batch ->
        List.to_tuple([:lgfx, @proto_ver, op_name, target, flags | args])

      target_policy in [:any, :sprite_only] and flags == 0 ->
        List.to_tuple([:lgfx, @proto_ver, op_name, target | args])

      target_policy in [:any, :sprite_only] ->
        List.to_tuple([:lgfx, @proto_ver, op_name, target, flags | args])

      target != 0 or flags != 0 ->
        List.to_tuple([:lgfx, @proto_ver, op_name, target, flags | args])

      true ->
        List.to_tuple([:lgfx, @proto_ver, op_name | args])
    end
  end

  defp validate_command_binary_size(_port, <<>>, _op_name), do: {:error, :empty_batch}

  defp validate_command_binary_size(port, command_binary, op_name) do
    payload_size = byte_size(command_binary)

    with {:ok, max_binary_bytes} when is_integer(max_binary_bytes) and max_binary_bytes > 0 <-
           max_binary_bytes(port) do
      if payload_size <= max_binary_bytes do
        :ok
      else
        {:error, {:binary_too_large, op_name, payload_size, max_binary_bytes}}
      end
    end
  end

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
    with {:ok, feature_bits} <- get_caps(port) do
      {:ok, (feature_bits &&& cap_bit) != 0}
    end
  end
end
