# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.RenderBatchTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.RenderBatch

  describe "encode/2" do
    test "encodes common render commands through BinaryBatch" do
      expected =
        BinaryBatch.batch([
          BinaryBatch.fill_screen(0x0000),
          BinaryBatch.set_text_color(0xFFFF),
          BinaryBatch.set_text_datum(4),
          BinaryBatch.set_cursor(10, 20),
          BinaryBatch.println("Hello"),
          BinaryBatch.draw_line(0, 40, 200, 40, 0xF800),
          BinaryBatch.display()
        ])

      assert RenderBatch.encode([
               {:fill_screen, :black},
               {:set_text_color, :white},
               {:set_text_datum, :middle_center},
               {:set_cursor, 10, 20},
               {:println, "Hello"},
               {:draw_line, 0, 40, 200, 40, {:rgb, 255, 0, 0}},
               :display
             ]) == {:ok, expected}
    end

    test "encodes default target and display options" do
      expected =
        BinaryBatch.batch([
          BinaryBatch.target(3),
          BinaryBatch.clear(0x0000),
          BinaryBatch.display()
        ])

      assert RenderBatch.encode([{:clear, :black}], target: 3, display: true) == {:ok, expected}
    end

    test "encodes explicit LCD target alias as no-op default target" do
      expected = BinaryBatch.batch([BinaryBatch.clear(0x0000)])

      assert RenderBatch.encode([{:clear, :black}], target: :lcd) == {:ok, expected}
    end

    test "encodes sprite target and sprite push commands" do
      expected =
        BinaryBatch.batch([
          BinaryBatch.target(3),
          BinaryBatch.color_mode(:palette_index),
          BinaryBatch.clear(0),
          BinaryBatch.fill_rect(0, 0, 10, 10, 1),
          BinaryBatch.set_text_color({:index, 2}, {:index, 0}),
          BinaryBatch.target(0),
          BinaryBatch.push_sprite(3, 10, 20, {:index, 0}),
          BinaryBatch.display()
        ])

      assert RenderBatch.encode(
               [
                 {:clear, {:index, 0}},
                 {:fill_rect, 0, 0, 10, 10, {:index, 1}},
                 {:set_text_color, {:index, 2}, {:index, 0}},
                 {:target, :lcd},
                 {:push_sprite, 3, 10, 20, {:index, 0}},
                 :display
               ],
               target: 3
             ) == {:ok, expected}
    end

    test "encodes MovingIcons-style strip sprite commands" do
      expected_strip =
        BinaryBatch.batch([
          BinaryBatch.target(10),
          BinaryBatch.clear(0x0000),
          BinaryBatch.push_sprite(1, 12, -4, 0x0000),
          BinaryBatch.push_sprite(2, 48, 18, 0x0000)
        ])

      expected_lcd =
        BinaryBatch.batch([
          BinaryBatch.push_sprite(10, 0, 40)
        ])

      assert RenderBatch.encode(
               [
                 {:clear, :black},
                 {:push_sprite, 1, 12, -4, :black},
                 {:push_sprite, 2, 48, 18, :black}
               ],
               target: 10
             ) == {:ok, expected_strip}

      assert RenderBatch.encode([{:push_sprite, 10, 0, 40}], target: :lcd) ==
               {:ok, expected_lcd}
    end

    test "returns normalization errors" do
      assert RenderBatch.encode([{:unknown, :command}]) ==
               {:error, {:bad_render_command, {:unknown, :command}}}
    end
  end

  describe "submit/3" do
    test "rejects invalid render options before NIF submission" do
      assert RenderBatch.submit(:not_a_handle, <<>>, :bad_opts) ==
               {:error, {:bad_render_options, :bad_opts}}
    end

    test "rejects invalid validate option before NIF submission" do
      assert RenderBatch.submit(:not_a_handle, <<>>, validate: :yes) ==
               {:error, {:bad_render_validate_option, :yes}}
    end
  end
end
