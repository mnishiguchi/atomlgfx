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
    "LGFX_RENDER_OP_PUSH_SPRITE_TRANSPARENT" => {:push_sprite_transparent, 0xF2},
    "LGFX_RENDER_OP_PUSH_SPRITE_LIST" => {:push_sprite_list, 0xF3},
    "LGFX_RENDER_OP_PUSH_SPRITE_REGION_LIST" => {:push_sprite_region_list, 0xF4},
    "LGFX_RENDER_OP_BEGIN_STRIP" => {:begin_strip, 0xF5},
    "LGFX_RENDER_OP_PRESENT_STRIP" => {:present_strip, 0xF6},
    "LGFX_RENDER_OP_FILL_RECT_LIST" => {:fill_rect_list, 0xF7},
    "LGFX_RENDER_OP_DRAW_LINE_LIST" => {:draw_line_list, 0xF8},
    "LGFX_RENDER_OP_DRAW_PIXEL_LIST" => {:draw_pixel_list, 0xF9},
    "LGFX_RENDER_OP_DRAW_RECT_LIST" => {:draw_rect_list, 0xFA},
    "LGFX_RENDER_OP_FILL_CIRCLE_LIST" => {:fill_circle_list, 0xFB},
    "LGFX_RENDER_OP_DRAW_CIRCLE_LIST" => {:draw_circle_list, 0xFC},
    "LGFX_RENDER_OP_FILL_TRIANGLE_LIST" => {:fill_triangle_list, 0xFD},
    "LGFX_RENDER_OP_DRAW_TRIANGLE_LIST" => {:draw_triangle_list, 0xFE},
    "LGFX_RENDER_OP_ELLIPSE_LIST" => {:ellipse_list, 0xFF}
  }

  describe "render-private opcode drift checks" do
    test "Elixir keeps the render-private opcode range explicit and contiguous" do
      assert BinaryBatch.__render_private_opcodes__() == [
               target: 0xF0,
               color_mode: 0xF1,
               push_sprite_transparent: 0xF2,
               push_sprite_list: 0xF3,
               push_sprite_region_list: 0xF4,
               begin_strip: 0xF5,
               present_strip: 0xF6,
               fill_rect_list: 0xF7,
               draw_line_list: 0xF8,
               draw_pixel_list: 0xF9,
               draw_rect_list: 0xFA,
               fill_circle_list: 0xFB,
               draw_circle_list: 0xFC,
               fill_triangle_list: 0xFD,
               draw_triangle_list: 0xFE,
               ellipse_list: 0xFF
             ]

      assert Keyword.values(BinaryBatch.__render_private_opcodes__()) == Enum.to_list(0xF0..0xFF)
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
end
