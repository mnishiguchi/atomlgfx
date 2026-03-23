# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.ProtocolSmoke do
  @moduledoc false

  import Bitwise

  @t_short 5_000
  @proto_ver 1

  # Protocol FeatureBits (must stay aligned with lgfx_port/include_internal/lgfx_port/protocol.h)
  @cap_sprite 1 <<< 0
  @cap_pushimage 1 <<< 1
  @cap_last_error 1 <<< 2
  @cap_touch 1 <<< 3
  @cap_palette 1 <<< 4

  @cap_constants [
    cap_sprite: @cap_sprite,
    cap_pushimage: @cap_pushimage,
    cap_last_error: @cap_last_error,
    cap_touch: @cap_touch,
    cap_palette: @cap_palette
  ]

  @known_caps_mask @cap_sprite ||| @cap_pushimage ||| @cap_last_error ||| @cap_touch |||
                     @cap_palette

  def run(port), do: run(port, &AtomLGFX.raw_call/6)

  def run(port, raw_call) when is_function(raw_call, 6) do
    with :ok <- check_local_cap_constants(),
         :ok <- check_write_session_requires_init(port, raw_call),
         {:ok, caps} <- check_get_caps_metadata_and_pushimage(port, raw_call),
         :ok <- check_last_error_cap_matches_availability(port, raw_call, caps.feature_bits) do
      IO.puts("protocol smoke ok")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("protocol smoke failed: #{inspect(reason)}")
        err
    end
  end

  # -----------------------------------------------------------------------------
  # 0) Tiny local metadata self-test (future-proof)
  # -----------------------------------------------------------------------------
  defp check_local_cap_constants do
    with :ok <- check_cap_constants_are_powers_of_two(@cap_constants),
         :ok <- check_cap_constants_are_unique(@cap_constants) do
      :ok
    end
  end

  defp check_cap_constants_are_powers_of_two([]), do: :ok

  defp check_cap_constants_are_powers_of_two([{name, value} | rest]) do
    if power_of_two?(value) do
      check_cap_constants_are_powers_of_two(rest)
    else
      {:error, {:bad_cap_constant_not_power_of_two, name, value}}
    end
  end

  defp check_cap_constants_are_unique(cap_constants) do
    case find_duplicate_cap_constant_value(cap_constants, []) do
      :ok ->
        :ok

      {:error, duplicate_value} ->
        {:error, {:duplicate_cap_constants, duplicate_value, cap_constants}}
    end
  end

  defp find_duplicate_cap_constant_value([], _seen_values), do: :ok

  defp find_duplicate_cap_constant_value([{_name, value} | rest], seen_values) do
    if list_member?(value, seen_values) do
      {:error, value}
    else
      find_duplicate_cap_constant_value(rest, [value | seen_values])
    end
  end

  defp list_member?(_value, []), do: false
  defp list_member?(value, [value | _rest]), do: true
  defp list_member?(value, [_other | rest]), do: list_member?(value, rest)

  defp power_of_two?(n) when is_integer(n) and n > 0 do
    (n &&& n - 1) == 0
  end

  defp power_of_two?(_), do: false

  # -----------------------------------------------------------------------------
  # 1) Pre-init write-session ops must be rejected consistently
  # -----------------------------------------------------------------------------
  defp check_write_session_requires_init(port, raw_call) do
    with :ok <-
           expect_not_initialized(raw_call.(port, :startWrite, 0, 0, [], @t_short), :startWrite),
         :ok <- expect_not_initialized(raw_call.(port, :endWrite, 0, 0, [], @t_short), :endWrite) do
      :ok
    end
  end

  defp expect_not_initialized({:error, :not_initialized}, _op), do: :ok
  defp expect_not_initialized({:error, reason}, op), do: {:error, {op, :unexpected_error, reason}}
  defp expect_not_initialized({:ok, payload}, op), do: {:error, {op, :unexpected_ok, payload}}

  # -----------------------------------------------------------------------------
  # 2) getCaps metadata sanity + CAP_PUSHIMAGE must be advertised
  # -----------------------------------------------------------------------------
  defp check_get_caps_metadata_and_pushimage(port, raw_call) do
    case raw_call.(port, :getCaps, 0, 0, [], @t_short) do
      {:ok, {:caps, proto_ver, max_binary_bytes, max_sprites, feature_bits}}
      when is_integer(proto_ver) and is_integer(max_binary_bytes) and is_integer(max_sprites) and
             is_integer(feature_bits) ->
        with :ok <- check_caps_metadata(proto_ver, max_binary_bytes, max_sprites, feature_bits),
             :ok <- check_cap_pushimage(feature_bits) do
          maybe_log_unknown_feature_bits(feature_bits)

          {:ok,
           %{
             proto_ver: proto_ver,
             max_binary_bytes: max_binary_bytes,
             max_sprites: max_sprites,
             feature_bits: feature_bits
           }}
        end

      {:ok, payload} ->
        {:error, {:bad_caps_payload, payload}}

      {:error, reason} ->
        {:error, {:get_caps_failed, reason}}
    end
  end

  defp check_caps_metadata(proto_ver, max_binary_bytes, max_sprites, feature_bits) do
    cond do
      proto_ver != @proto_ver ->
        {:error, {:proto_ver_mismatch, @proto_ver, proto_ver}}

      max_binary_bytes <= 0 ->
        {:error, {:bad_max_binary_bytes, max_binary_bytes}}

      max_sprites < 0 ->
        {:error, {:bad_max_sprites, max_sprites}}

      feature_bits < 0 ->
        {:error, {:bad_feature_bits, feature_bits}}

      true ->
        :ok
    end
  end

  defp check_cap_pushimage(feature_bits) do
    if cap_set?(feature_bits, @cap_pushimage) do
      :ok
    else
      {:error, {:cap_pushimage_missing, feature_bits}}
    end
  end

  defp maybe_log_unknown_feature_bits(feature_bits) do
    known = feature_bits &&& @known_caps_mask
    unknown = bxor(feature_bits, known)

    if unknown != 0 do
      IO.puts("protocol smoke note: unknown feature bits present (future caps): #{unknown}")
    end

    :ok
  end

  # -----------------------------------------------------------------------------
  # 3) CAP_LAST_ERROR bit must match actual getLastError availability
  # -----------------------------------------------------------------------------
  defp check_last_error_cap_matches_availability(port, raw_call, feature_bits) do
    cap_last_error? = cap_set?(feature_bits, @cap_last_error)

    case raw_call.(port, :getLastError, 0, 0, [], @t_short) do
      {:ok, {:last_error, _last_op, _reason, _flags, _target, _esp_err}} ->
        if cap_last_error? do
          :ok
        else
          {:error, :cap_last_error_clear_but_get_last_error_is_available}
        end

      {:ok, payload} ->
        {:error, {:bad_last_error_payload, payload}}

      {:error, :unsupported} ->
        if cap_last_error? do
          {:error, :cap_last_error_set_but_get_last_error_returns_unsupported}
        else
          :ok
        end

      {:error, reason} ->
        {:error, {:get_last_error_unexpected_error, reason}}
    end
  end

  defp cap_set?(feature_bits, cap_bit) do
    (feature_bits &&& cap_bit) != 0
  end
end
