defmodule SampleApp.ProtocolSmoke do
  @moduledoc false

  import Bitwise

  @t_short 5_000
  @proto_ver 1

  # Protocol FeatureBits (LGFX_PORT_PROTOCOL.md)
  @cap_sprite 1 <<< 0
  @cap_pushimage 1 <<< 1
  @cap_last_error 1 <<< 4
  @known_caps_mask @cap_sprite ||| @cap_pushimage ||| @cap_last_error

  def run(port), do: run(port, &SampleApp.Port.raw_call/6)

  def run(port, raw_call) when is_function(raw_call, 6) do
    with :ok <- check_local_cap_constants(),
         :ok <- check_set_color_depth_target_nonzero(port, raw_call),
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
    cond do
      not power_of_two?(@cap_pushimage) ->
        {:error, {:bad_cap_constant_not_power_of_two, :cap_pushimage, @cap_pushimage}}

      not power_of_two?(@cap_last_error) ->
        {:error, {:bad_cap_constant_not_power_of_two, :cap_last_error, @cap_last_error}}

      @cap_pushimage == @cap_last_error ->
        {:error, {:duplicate_cap_constants, @cap_pushimage}}

      true ->
        :ok
    end
  end

  defp power_of_two?(n) when is_integer(n) and n > 0 do
    (n &&& n - 1) == 0
  end

  defp power_of_two?(_), do: false

  # -----------------------------------------------------------------------------
  # 1) setColorDepth Target != 0 must return unsupported (protocol v1 policy)
  # -----------------------------------------------------------------------------
  defp check_set_color_depth_target_nonzero(port, raw_call) do
    # {lgfx, Ver, setColorDepth, Target=1, Flags=0, Depth=16}
    case raw_call.(port, :setColorDepth, 1, 0, [16], @t_short) do
      {:error, :unsupported} ->
        :ok

      {:error, reason} ->
        {:error, {:set_color_depth_target_nonzero_expected_unsupported, reason}}

      {:ok, result} ->
        {:error, {:set_color_depth_target_nonzero_unexpected_ok, result}}
    end
  end

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
