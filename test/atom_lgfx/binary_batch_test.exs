# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatchTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.OpSchema
  alias AtomLGFX.Protocol

  describe "minimal render-private commands" do
    test "encodes target and color mode commands" do
      assert BinaryBatch.target(0) == <<0xF0, 0>>
      assert BinaryBatch.target(42) == <<0xF0, 42>>
      assert BinaryBatch.color_mode(:rgb565) == <<0xF1, 0>>
      assert BinaryBatch.color_mode(:palette_index) == <<0xF1, 1>>
    end

    test "encodes native strip commands" do
      assert BinaryBatch.begin_strip(0x1234) == <<0xF3, 0x34, 0x12>>
      assert BinaryBatch.present_strip() == <<0xF4>>
    end

    test "encodes transparent sprite push through the retained private command" do
      assert BinaryBatch.push_sprite(3, -2, 300, {:index, 7}) ==
               <<0xF2, Protocol.transparent_index_flag()::little-16, 3, -2::signed-little-16,
                 300::signed-little-16, 7::little-16>>
    end
  end

  describe "scalar batch commands" do
    test "encodes ordinary scalar render commands" do
      assert BinaryBatch.fill_rect(-1, 2, 3, 4, 0x1234) ==
               <<OpSchema.opcode!(:fill_rect), -1::signed-little-16, 2::signed-little-16,
                 3::little-16, 4::little-16, 0x1234::little-16>>

      assert BinaryBatch.draw_line(1, 2, 3, 4, 0xFFFF) ==
               <<OpSchema.opcode!(:draw_line), 1::signed-little-16, 2::signed-little-16,
                 3::signed-little-16, 4::signed-little-16, 0xFFFF::little-16>>
    end

    test "encodes text overlay commands" do
      assert BinaryBatch.draw_string(-2, 3, "ok") ==
               <<OpSchema.opcode!(:draw_string), -2::signed-little-16, 3::signed-little-16,
                 2::little-16, "ok">>

      assert BinaryBatch.println("ok") == <<OpSchema.opcode!(:println), 2::little-16, "ok">>
    end
  end

  describe "retained transformed-sprite hot paths" do
    test "encodes and decodes push_rotate_zoom_list" do
      command = BinaryBatch.push_rotate_zoom_list([{1, -2, 300, 9000, 1024, 2048}])
      push_rotate_zoom_list_opcode = OpSchema.opcode!(:push_rotate_zoom_list)

      assert <<^push_rotate_zoom_list_opcode, _flags::little-16, "PRZL", _rest::binary>> = command

      assert {:ok,
              [
                %{
                  op: :push_rotate_zoom_list,
                  instances: [
                    %{
                      source_target: 1,
                      x: -2,
                      y: 300,
                      angle_cdeg: 9000,
                      zoom_x1024: 1024,
                      zoom_y1024: 2048
                    }
                  ]
                }
              ]} = BinaryBatch.decode(command)
    end

    test "encodes and decodes push_rotate_zoom_frame_strips" do
      command =
        BinaryBatch.push_rotate_zoom_frame_strips(
          [{1, -2, 300, 9000, 1024, 2048}],
          frame_height: 480,
          background: 0x0000,
          transparent: 0x0001
        )

      assert <<0xFF, 0x01, _flags::little-16, "PRZF", _rest::binary>> = command

      assert {:ok,
              [
                %{
                  op: :push_rotate_zoom_frame_strips,
                  frame_height: 480,
                  background: 0x0000,
                  transparent: 0x0001,
                  instances: [
                    %{
                      source_target: 1,
                      x: -2,
                      y: 300,
                      angle_cdeg: 9000,
                      zoom_x1024: 1024,
                      zoom_y1024: 2048
                    }
                  ]
                }
              ]} = BinaryBatch.decode(command)
    end

    test "rejects native frame command inside an active strip" do
      command =
        BinaryBatch.batch([
          BinaryBatch.begin_strip(0),
          BinaryBatch.push_rotate_zoom_frame_strips([{1, 0, 0, 0, 1024, 1024}], frame_height: 10),
          BinaryBatch.present_strip()
        ])

      assert {:error, {:batch_failed, 1, 0xFF, :strip_already_active}} =
               BinaryBatch.validate(command)
    end
  end

  describe "removed speculative commands" do
    test "does not export packed primitive-list or payload-heavy batch helpers" do
      removed = [
        {:draw_pixel_list, 1},
        {:draw_rect_list, 1},
        {:fill_rect_list, 1},
        {:draw_circle_list, 1},
        {:fill_circle_list, 1},
        {:draw_ellipse_list, 1},
        {:fill_ellipse_list, 1},
        {:draw_line_list, 1},
        {:draw_triangle_list, 1},
        {:fill_triangle_list, 1},
        {:push_sprite_list, 1},
        {:push_sprite_list, 2},
        {:push_sprite_region_list, 1},
        {:push_sprite_region_list, 2},
        {:draw_jpg, 3},
        {:draw_jpg, 9},
        {:push_image_rgb565, 5},
        {:push_image_rgb565, 6}
      ]

      for {name, arity} <- removed do
        refute function_exported?(BinaryBatch, name, arity),
               "#{name}/#{arity} should stay outside the v2 protocol BinaryBatch surface"
      end
    end
  end
end
