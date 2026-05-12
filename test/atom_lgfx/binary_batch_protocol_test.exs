# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatchProtocolTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.BinaryBatch

  @protocol_header Path.expand("../../lgfx_port/include_internal/lgfx_port/protocol.h", __DIR__)

  @expected_render_private_defines %{
    "LGFX_RENDER_OP_TARGET" => {:target, 0xF0},
    "LGFX_RENDER_OP_COLOR_MODE" => {:color_mode, 0xF1},
    "LGFX_RENDER_OP_PUSH_SPRITE_TRANSPARENT" => {:push_sprite_transparent, 0xF2}
  }

  @expected_render_extended_defines %{}

  describe "render-private opcode drift checks" do
    test "Elixir keeps the minimal render-private opcode registry explicit" do
      assert BinaryBatch.__render_private_opcodes__() == [
               target: 0xF0,
               color_mode: 0xF1,
               push_sprite_transparent: 0xF2
             ]
    end

    test "Elixir keeps extended render sub-opcodes explicit" do
      assert BinaryBatch.__render_extended_opcodes__() == []
    end

    test "known batch opcode registry has no duplicate opcode values" do
      known_opcodes = BinaryBatch.__known_batch_opcodes__()

      assert length(known_opcodes) == length(Enum.uniq(known_opcodes))
    end

    test "native protocol.h render-private defines match the Elixir registry" do
      native_defines = parse_native_render_private_defines()

      expected_defines =
        Map.new(@expected_render_private_defines, fn {define_name, {_op_name, opcode}} ->
          {define_name, opcode}
        end)

      assert native_defines == expected_defines
    end

    test "native protocol.h render-private defines match the expected Elixir names" do
      elixir_opcodes = Map.new(BinaryBatch.__render_private_opcodes__())

      for {define_name, {op_name, expected_opcode}} <- @expected_render_private_defines do
        assert Map.fetch!(elixir_opcodes, op_name) == expected_opcode,
               "#{define_name} should remain wired to #{inspect(op_name)}"
      end
    end

    test "native protocol.h extended render defines remain stable" do
      native_defines = parse_native_render_extended_defines()

      expected_defines =
        Map.new(@expected_render_extended_defines, fn {define_name, {_op_name, subopcode}} ->
          {define_name, subopcode}
        end)

      assert native_defines == expected_defines
    end

    test "native protocol.h extended render defines match the expected Elixir names" do
      elixir_subopcodes = Map.new(BinaryBatch.__render_extended_opcodes__())

      for {define_name, {op_name, expected_subopcode}} <- @expected_render_extended_defines do
        assert Map.fetch!(elixir_subopcodes, op_name) == expected_subopcode,
               "#{define_name} should remain wired to #{inspect(op_name)}"
      end
    end
  end

  defp parse_native_render_private_defines do
    @protocol_header
    |> File.read!()
    |> then(&Regex.scan(render_private_define_regex(), &1))
    |> Map.new(fn [_match, define_name, hex_opcode] ->
      {define_name, String.to_integer(hex_opcode, 16)}
    end)
  end

  defp render_private_define_regex do
    ~r/^#define\s+(LGFX_RENDER_OP_[A-Z0-9_]+)\s+\(\(uint8_t\)\s+0x([0-9A-Fa-f]+)u\)/m
  end

  defp parse_native_render_extended_defines do
    @protocol_header
    |> File.read!()
    |> then(&Regex.scan(render_extended_define_regex(), &1))
    |> Map.new(fn [_match, define_name, hex_opcode] ->
      {define_name, String.to_integer(hex_opcode, 16)}
    end)
  end

  defp render_extended_define_regex do
    ~r/^#define\s+(LGFX_RENDER_EXT_OP_[A-Z0-9_]+)\s+\(\(uint8_t\)\s+0x([0-9A-Fa-f]+)u\)/m
  end
end
