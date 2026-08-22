# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.ProtocolSmoke do
  @moduledoc false

  import Bitwise

  @bg 0x0000
  @fg 0xFFFF
  @muted 0x8410
  @ok 0x07E0
  @accent 0x07FF

  # Protocol FeatureBits (must stay aligned with native/include/atom_lgfx/constants.h)
  @cap_sprite 1 <<< 0
  @cap_pushimage 1 <<< 1
  @cap_last_error 1 <<< 2
  @cap_touch 1 <<< 3
  @cap_palette 1 <<< 4
  @cap_batch 1 <<< 5

  @cap_constants [
    cap_sprite: @cap_sprite,
    cap_pushimage: @cap_pushimage,
    cap_last_error: @cap_last_error,
    cap_touch: @cap_touch,
    cap_palette: @cap_palette,
    cap_batch: @cap_batch
  ]

  @known_caps_mask @cap_sprite ||| @cap_pushimage ||| @cap_last_error ||| @cap_touch |||
                     @cap_palette ||| @cap_batch

  def run(handle), do: run(handle, &AtomLGFX.raw_call/5)

  def draw_summary(handle, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    panel_w = max(80, w - 16)
    panel_h = max(48, min(88, h - 24))

    with :ok <- AtomLGFX.fill_screen(handle, @bg),
         :ok <- AtomLGFX.reset_text_state(handle, 0),
         :ok <- AtomLGFX.set_text_font_preset(handle, :ascii, 0),
         :ok <- AtomLGFX.set_text_wrap(handle, false, 0),
         :ok <- AtomLGFX.draw_rect(handle, 8, 12, panel_w, panel_h, @accent, 0),
         :ok <- AtomLGFX.draw_string_bg(handle, 16, 22, @fg, @bg, 2, "PROTOCOL", 0),
         :ok <- AtomLGFX.draw_string_bg(handle, 18, 48, @ok, @bg, 1, "v3 handshake ok", 0),
         :ok <-
           AtomLGFX.draw_string_bg(
             handle,
             18,
             64,
             @muted,
             @bg,
             1,
             "caps and error path checked",
             0
           ) do
      :ok
    end
  end

  def run(handle, raw_call) when is_function(raw_call, 5) do
    with :ok <- check_local_cap_constants(),
         :ok <- check_write_session_requires_init(handle, raw_call),
         {:ok, feature_bits} <- check_get_caps_feature_bits_and_required_caps(handle, raw_call),
         :ok <- check_last_error_cap_matches_availability(handle, raw_call, feature_bits) do
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
  defp check_write_session_requires_init(handle, raw_call) do
    with :ok <-
           expect_not_initialized(
             raw_call.(handle, :start_write, 0, 0, []),
             :start_write
           ),
         :ok <-
           expect_not_initialized(raw_call.(handle, :end_write, 0, 0, []), :end_write) do
      :ok
    end
  end

  defp expect_not_initialized({:error, :not_initialized}, _op), do: :ok
  defp expect_not_initialized({:error, reason}, op), do: {:error, {op, :unexpected_error, reason}}
  defp expect_not_initialized({:ok, payload}, op), do: {:error, {op, :unexpected_ok, payload}}

  # -----------------------------------------------------------------------------
  # 2) get_caps feature-bit sanity + required capabilities must be advertised
  # -----------------------------------------------------------------------------
  defp check_get_caps_feature_bits_and_required_caps(handle, raw_call) do
    case raw_call.(handle, :get_caps, 0, 0, []) do
      {:ok, feature_bits} when is_integer(feature_bits) and feature_bits >= 0 ->
        with :ok <- check_cap_pushimage(feature_bits),
             :ok <- check_cap_batch(feature_bits) do
          maybe_log_unknown_feature_bits(feature_bits)
          {:ok, feature_bits}
        end

      {:ok, payload} ->
        {:error, {:bad_caps_payload, payload}}

      {:error, reason} ->
        {:error, {:get_caps_failed, reason}}
    end
  end

  defp check_cap_pushimage(feature_bits) do
    if cap_set?(feature_bits, @cap_pushimage) do
      :ok
    else
      {:error, {:cap_pushimage_missing, feature_bits}}
    end
  end

  defp check_cap_batch(feature_bits) do
    if cap_set?(feature_bits, @cap_batch) do
      :ok
    else
      {:error, {:cap_batch_missing, feature_bits}}
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
  # 3) CAP_LAST_ERROR bit must match actual get_last_error availability
  # -----------------------------------------------------------------------------
  defp check_last_error_cap_matches_availability(handle, raw_call, feature_bits) do
    cap_last_error? = cap_set?(feature_bits, @cap_last_error)

    case raw_call.(handle, :get_last_error, 0, 0, []) do
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
