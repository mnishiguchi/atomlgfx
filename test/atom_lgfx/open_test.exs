# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.OpenTest do
  use ExUnit.Case, async: true

  test "open stores normalized config for the creating process" do
    assert {:ok, handle} =
             AtomLGFX.open(
               panel_driver: :ili9488,
               width: 320,
               height: 480
             )

    assert is_reference(handle)
    assert {:ok, config} = AtomLGFX.get_open_config(handle)
    assert config[:panel_driver] == :ili9488
    assert config[:width] == 320
    assert config[:height] == 480
  end

  test "unknown handles do not silently use build defaults" do
    handle = make_ref()

    assert AtomLGFX.get_open_config(handle) == {:error, :invalid_handle}
    assert AtomLGFX.init(handle) == {:error, :invalid_handle}
  end

  test "handles are owned by the process that opened them" do
    assert {:ok, handle} = AtomLGFX.open(panel_driver: :ili9488)
    parent = self()

    spawn(fn ->
      send(parent, {:open_config_from_other_process, AtomLGFX.get_open_config(handle)})
    end)

    assert_receive {:open_config_from_other_process, {:error, :invalid_handle}}
    assert {:ok, config} = AtomLGFX.get_open_config(handle)
    assert config[:panel_driver] == :ili9488
  end
end
