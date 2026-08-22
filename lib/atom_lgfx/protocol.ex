# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Protocol do
  @moduledoc false

  import Bitwise
  import AtomLGFX.Guards

  alias AtomLGFX.Cache
  alias AtomLGFX.Native
  alias AtomLGFX.OpSchema

  @max_binary_bytes 256 * 1024

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

  # Raw NIF call for smoke tests. Target is intentionally not range-checked.
  def raw_call(handle, op, target, flags, args)
      when is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    with {:ok, op_name} <- normalize_op_name(op) do
      call_named(handle, op_name, target, flags, args)
    end
  end

  def ping(handle), do: call_ok(handle, :ping, 0, 0, [])

  def get_caps(handle) do
    case call(handle, :get_caps, 0, 0, []) do
      {:ok, feature_bits} when is_integer(feature_bits) and feature_bits >= 0 ->
        {:ok, feature_bits}

      {:ok, other} ->
        {:error, {:bad_caps_payload, other}}

      {:error, reason} ->
        {:error, reason}
    end
  end

  def get_last_error(handle) do
    with {:ok, payload} <- call(handle, :get_last_error, 0, 0, []) do
      decode_last_error(payload)
    end
  end

  def width(handle, target \\ 0) when target_any(target) do
    integer_query(handle, :width, :width, target)
  end

  def height(handle, target \\ 0) when target_any(target) do
    integer_query(handle, :height, :height, target)
  end

  def supports_sprite?(handle), do: supports_cap?(handle, @cap_sprite)
  def supports_pushimage?(handle), do: supports_cap?(handle, @cap_pushimage)
  def supports_last_error?(handle), do: supports_cap?(handle, @cap_last_error)
  def supports_touch?(handle), do: supports_cap?(handle, @cap_touch)
  def supports_palette?(handle), do: supports_cap?(handle, @cap_palette)
  def supports_batch?(handle), do: supports_cap?(handle, @cap_batch)

  def max_binary_bytes(handle) do
    case Cache.get_max_binary_bytes(handle) do
      {:ok, value} -> {:ok, value}
      :error -> {:ok, @max_binary_bytes}
    end
  end

  def call_ok(_handle, op, target, flags, args)
      when is_atom(op) and
             is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    case OpSchema.opcode(op) do
      {:ok, opcode} ->
        case Native.call(opcode, target, flags, args) do
          {:ok, _result} -> :ok
          {:error, reason} -> {:error, reason}
          other -> {:error, {:unexpected_reply, other}}
        end

      {:error, reason} ->
        {:error, reason}
    end
  end

  def call(_handle, op, target, flags, args)
      when is_atom(op) and
             is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    case OpSchema.opcode(op) do
      {:ok, opcode} ->
        Native.call(opcode, target, flags, args)
        |> normalize_native_reply()

      {:error, reason} ->
        {:error, reason}
    end
  end

  def call_opcode(handle, opcode, target, flags, args)
      when is_integer(opcode) and
             opcode >= 0 and
             is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    with {:ok, op_name} <- normalize_op_name(opcode) do
      call_named(handle, op_name, target, flags, args)
    end
  end

  defp call_named(_handle, op_name, target, flags, args) do
    Native.call(OpSchema.opcode!(op_name), target, flags, args)
    |> normalize_native_reply()
  end

  defp normalize_native_reply({:ok, result}), do: {:ok, result}
  defp normalize_native_reply({:error, reason}), do: {:error, reason}
  defp normalize_native_reply(other), do: {:error, {:unexpected_reply, other}}

  def submit_binary_batch(handle, command_binary) when is_binary(command_binary) do
    submit_binary_batch(handle, 0, command_binary)
  end

  def submit_binary_batch(_handle, command_binary) do
    {:error, {:bad_binary_batch, command_binary}}
  end

  def submit_binary_batch(handle, target, command_binary)
      when target_any(target) and is_binary(command_binary) do
    with :ok <- validate_command_binary_size(handle, command_binary, :submit_binary_batch) do
      submit_native_batch(target, command_binary)
    end
  end

  def submit_binary_batch(_handle, _target, command_binary) do
    {:error, {:bad_binary_batch, command_binary}}
  end

  defp submit_native_batch(target, command_binary) do
    case Native.batch(target, command_binary) do
      :ok -> :ok
      {:error, reason} -> {:error, reason}
      other -> {:error, {:unexpected_reply, other}}
    end
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

  defp validate_command_binary_size(_handle, <<>>, _op_name), do: {:error, :empty_batch}

  defp validate_command_binary_size(handle, command_binary, op_name) do
    payload_size = byte_size(command_binary)

    with {:ok, max_binary_bytes} when is_integer(max_binary_bytes) and max_binary_bytes > 0 <-
           max_binary_bytes(handle) do
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

  defp integer_query(handle, op, name, target) do
    with {:ok, value} <- call(handle, op, target, 0, []),
         true <- is_integer(value) do
      {:ok, value}
    else
      false -> {:error, {:bad_reply_value, name}}
      {:error, reason} -> {:error, reason}
    end
  end

  defp supports_cap?(handle, cap_bit) do
    with {:ok, feature_bits} <- get_caps(handle) do
      {:ok, (feature_bits &&& cap_bit) != 0}
    end
  end
end
