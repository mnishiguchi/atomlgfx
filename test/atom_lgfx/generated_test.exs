# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.GeneratedTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.Generated
  alias AtomLGFX.Protocol
  alias AtomLGFX.OpSchema

  test "protocol version is v3" do
    assert Protocol.proto_ver() == 3
  end

  test "encodes ordinary requests as v3 flat tuples" do
    assert Protocol.__encode_v3_request__(:ping, 0, 0, []) == {:lgfx, 3, :ping}

    assert Protocol.__encode_v3_request__(:fill_rect, 0, 0, [1, 2, 3, 4, 0xFFFF]) ==
             {:lgfx, 3, :fill_rect, 0, 1, 2, 3, 4, 0xFFFF}

    assert Protocol.__encode_v3_request__(:create_sprite, 1, 0, [320, 240, 4]) ==
             {:lgfx, 3, :create_sprite, 1, 320, 240, 4}

    assert Protocol.__encode_v3_request__(:fill_rect, 1, Protocol.color_index_flag(), [
             1,
             2,
             3,
             4,
             5
           ]) ==
             {:lgfx, 3, :fill_rect, 1, Protocol.color_index_flag(), 1, 2, 3, 4, 5}

    assert Protocol.__encode_v3_request__(:submit_binary_batch, 0, 0, [<<1, 2, 3>>]) ==
             {:lgfx, 3, :submit_binary_batch, 0, 0, <<1, 2, 3>>}
  end

  test "maps snake_case operation names to stable opcodes" do
    assert Generated.opcode(:fill_screen) == OpSchema.opcode(:fill_screen)
    assert Generated.opcode(:draw_line) == OpSchema.opcode(:draw_line)
    assert Generated.opcode(:fill_rect) == OpSchema.opcode(:fill_rect)
    assert Generated.opcode(:set_rotation) == OpSchema.opcode(:set_rotation)
    assert Generated.opcode(:push_rotate_zoom_list) == OpSchema.opcode(:push_rotate_zoom_list)
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

  test "exposes draw_pixel as a normal LovyanGFX operation" do
    assert Generated.public?(:draw_pixel)
    assert Keyword.fetch!(Keyword.fetch!(Generated.ops(), :draw_pixel), :batchable)
    refute Generated.raw?(:draw_pixel)
  end

  test "marks write-session operations as raw-only" do
    refute Generated.public?(:start_write)
    refute Generated.public?(:end_write)
    assert Generated.raw?(:start_write)
    assert Generated.raw?(:end_write)
  end
end
