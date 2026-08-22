# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Touch do
  @moduledoc false

  alias AtomLGFX.Protocol

  def get_touch(handle) do
    with {:ok, payload} <- Protocol.call(handle, :get_touch, 0, 0, []) do
      decode_touch_payload(:get_touch, payload)
    end
  end

  def get_touch_raw(handle) do
    with {:ok, payload} <- Protocol.call(handle, :get_touch_raw, 0, 0, []) do
      decode_touch_payload(:get_touch_raw, payload)
    end
  end

  def set_touch_calibrate(handle, params8) do
    with {:ok, params_list} <- normalize_u16_8(params8) do
      Protocol.call_ok(handle, :set_touch_calibrate, 0, 0, params_list)
    end
  end

  def calibrate_touch(handle) do
    with {:ok, payload} <-
           Protocol.call(handle, :calibrate_touch, 0, 0, []) do
      decode_calibrate_payload(payload)
    end
  end

  defp decode_touch_payload(_op, :none), do: {:ok, :none}

  defp decode_touch_payload(_op, {x, y, size})
       when is_integer(x) and is_integer(y) and is_integer(size) do
    {:ok, {x, y, size}}
  end

  defp decode_touch_payload(op, other), do: {:error, {:bad_touch_payload, op, other}}

  defp decode_calibrate_payload({p0, p1, p2, p3, p4, p5, p6, p7} = tuple)
       when is_integer(p0) and is_integer(p1) and is_integer(p2) and is_integer(p3) and
              is_integer(p4) and is_integer(p5) and is_integer(p6) and is_integer(p7) do
    {:ok, tuple}
  end

  defp decode_calibrate_payload(other), do: {:error, {:bad_touch_calibrate_payload, other}}

  defp normalize_u16_8({p0, p1, p2, p3, p4, p5, p6, p7}) do
    normalize_u16_8([p0, p1, p2, p3, p4, p5, p6, p7])
  end

  defp normalize_u16_8([p0, p1, p2, p3, p4, p5, p6, p7] = list) do
    if u16?(p0) and u16?(p1) and u16?(p2) and u16?(p3) and
         u16?(p4) and u16?(p5) and u16?(p6) and u16?(p7) do
      {:ok, list}
    else
      {:error, {:bad_touch_calibrate_params, list}}
    end
  end

  defp normalize_u16_8(other), do: {:error, {:bad_touch_calibrate_params, other}}

  defp u16?(v) when is_integer(v) and v >= 0 and v <= 0xFFFF, do: true
  defp u16?(_), do: false
end
