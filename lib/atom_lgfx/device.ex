# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Device do
  @moduledoc false

  import AtomLGFX.Guards

  alias AtomLGFX.Protocol

  @valid_color_depths [1, 2, 4, 8, 16, 24]

  def init(port), do: Protocol.call_ok(port, :init, 0, 0, [], Protocol.long_timeout())

  def close(port), do: Protocol.call_ok(port, :close, 0, 0, [], Protocol.long_timeout())

  @doc """
  Starts a LovyanGFX write session on the LCD device.

  This maps directly to native `startWrite()` and participates in LovyanGFX's
  nested write counter. Calls should normally be paired with `end_write/1`.
  """
  def start_write(port),
    do: Protocol.call_ok(port, :startWrite, 0, 0, [], Protocol.long_timeout())

  @doc """
  Ends a LovyanGFX write session on the LCD device.

  This maps directly to native `endWrite()` and decrements LovyanGFX's nested
  write counter.
  """
  def end_write(port),
    do: Protocol.call_ok(port, :endWrite, 0, 0, [], Protocol.long_timeout())

  def display(port), do: Protocol.call_ok(port, :display, 0, 0, [], Protocol.long_timeout())

  def set_rotation(port, rotation) when is_integer(rotation) and rotation in 0..7 do
    Protocol.call_ok(port, :setRotation, 0, 0, [rotation], Protocol.long_timeout())
  end

  def set_brightness(port, brightness) when u8(brightness) do
    Protocol.call_ok(port, :setBrightness, 0, 0, [brightness], Protocol.long_timeout())
  end

  def set_color_depth(port, depth, target \\ 0)
      when is_integer(depth) and depth in @valid_color_depths and target_any(target) do
    Protocol.call_ok(port, :setColorDepth, target, 0, [depth], Protocol.long_timeout())
  end

  def set_swap_bytes(port, enabled, target \\ 0)
      when is_boolean(enabled) and target_any(target) do
    Protocol.call_ok(port, :setSwapBytes, target, 0, [enabled], Protocol.long_timeout())
  end
end
