# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Device do
  @moduledoc false

  import AtomLGFX.Guards

  alias AtomLGFX.Cache
  alias AtomLGFX.Native
  alias AtomLGFX.Protocol

  @valid_color_depths [1, 2, 4, 8, 16, 24]

  def init(handle) do
    with {:ok, config} <- Cache.get_open_config(handle) do
      Native.init(config)
    end
  end

  def close(_handle), do: Native.close()

  @doc """
  LCD 機器上で LovyanGFX の書き込み区間を開始します。
  """
  def start_write(handle),
    do: Protocol.call_ok(handle, :start_write, 0, 0, [])

  @doc """
  LCD 機器上の LovyanGFX 書き込み区間を終了します。
  """
  def end_write(handle),
    do: Protocol.call_ok(handle, :end_write, 0, 0, [])

  def display(handle), do: Protocol.call_ok(handle, :display, 0, 0, [])

  def set_rotation(handle, rotation) when is_integer(rotation) and rotation in 0..7 do
    Protocol.call_ok(handle, :set_rotation, 0, 0, [rotation])
  end

  def set_brightness(handle, brightness) when u8(brightness) do
    Protocol.call_ok(handle, :set_brightness, 0, 0, [brightness])
  end

  def set_color_depth(handle, depth, target \\ 0)
      when is_integer(depth) and depth in @valid_color_depths and target_any(target) do
    Protocol.call_ok(handle, :set_color_depth, target, 0, [depth])
  end

  def set_swap_bytes(handle, enabled, target \\ 0)
      when is_boolean(enabled) and target_any(target) do
    Protocol.call_ok(handle, :set_swap_bytes, target, 0, [enabled])
  end
end
