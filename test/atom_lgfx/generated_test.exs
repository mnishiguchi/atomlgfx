# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.GeneratedTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.Generated
  alias AtomLGFX.Protocol
  alias AtomLGFX.OpSchema

  test "protocol version is v2" do
    assert Protocol.proto_ver() == 2
  end

  test "maps snake_case operation names to stable opcodes" do
    assert Generated.opcode(:fill_screen) == OpSchema.opcode(:fill_screen)
    assert Generated.opcode(:draw_line) == OpSchema.opcode(:draw_line)
    assert Generated.opcode(:fill_rect) == OpSchema.opcode(:fill_rect)
    assert Generated.opcode(:set_rotation) == OpSchema.opcode(:set_rotation)
    assert Generated.opcode(:push_rotate_zoom_list) == OpSchema.opcode(:push_rotate_zoom_list)

    assert Generated.opcode(:get_presentation_strip_height) ==
             OpSchema.opcode(:get_presentation_strip_height)
  end

  test "does not resolve LovyanGFX-style camelCase atoms" do
    assert Generated.elixir_name(:fillRect) == {:error, {:unknown_lgfx_op, :fillRect}}
    assert Generated.opcode(:fillRect) == {:error, {:unknown_lgfx_op, :fillRect}}
  end

  test "exposes generated protocol validation metadata" do
    assert Generated.arg_range(:draw_bezier) == {:ok, 7..9}
    assert Generated.allowed_flags(:fill_rect) == {:ok, Protocol.color_index_flag()}
    assert Generated.target_policy(:create_sprite) == {:ok, :sprite_only}
    assert Generated.state_policy(:ping) == {:ok, :any}
    assert Generated.capability(:push_sprite) == {:ok, :sprite}
  end

  test "marks pixel operations as raw-only foot-guns" do
    refute Generated.public?(:draw_pixel)
    refute Keyword.has_key?(Keyword.fetch!(Generated.ops(), :draw_pixel), :batch)
    assert Generated.raw?(:draw_pixel)
  end

  test "marks write-session operations as raw-only" do
    refute Generated.public?(:start_write)
    refute Generated.public?(:end_write)
    assert Generated.raw?(:start_write)
    assert Generated.raw?(:end_write)
  end
end
