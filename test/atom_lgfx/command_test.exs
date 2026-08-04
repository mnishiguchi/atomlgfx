# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.CommandTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.Command

  describe "normalize/2" do
    test "normalizes common LovyanGFX-style commands" do
      assert Command.normalize(
               [
                 {:fill_screen, :black},
                 {:set_text_color, :white},
                 {:set_cursor, 10, 20},
                 {:println, "Hello"},
                 {:draw_line, 0, 40, 200, 40, :red},
                 :display
               ],
               []
             ) ==
               {:ok,
                [
                  {:fill_screen, 0x0000},
                  {:set_text_color, 0xFFFF},
                  {:set_cursor, 10, 20},
                  {:println, "Hello"},
                  {:draw_line, 0, 40, 200, 40, 0xF800},
                  :display
                ]}
    end

    test "prepends non-LCD default target" do
      assert Command.normalize([{:clear, :black}], target: 2) ==
               {:ok, [{:target, 2}, {:clear, 0x0000}]}
    end

    test "accepts LCD target alias" do
      assert Command.normalize([{:target, :lcd}, {:clear, :black}]) ==
               {:ok, [{:target, 0}, {:clear, 0x0000}]}

      assert Command.normalize([{:clear, :black}], target: :lcd) ==
               {:ok, [{:clear, 0x0000}]}
    end

    test "appends display when requested" do
      assert Command.normalize([{:clear, :black}], display: true) ==
               {:ok, [{:clear, 0x0000}, :display]}
    end

    test "does not append duplicate display" do
      assert Command.normalize([{:clear, :black}, :display], display: true) ==
               {:ok, [{:clear, 0x0000}, :display]}
    end

    test "normalizes RGB tuple colors for render commands" do
      assert Command.normalize([{:fill_screen, {:rgb, 255, 0, 0}}]) ==
               {:ok, [{:fill_screen, 0xF800}]}

      assert Command.normalize([{:set_text_color, {:rgb888, 0x00FF00}, {:rgb565, 0x0000}}]) ==
               {:ok, [{:set_text_color, 0x07E0, 0x0000}]}
    end

    test "normalizes named text datum values" do
      assert Command.normalize([{:set_text_datum, :middle_center}]) ==
               {:ok, [{:set_text_datum, 4}]}

      assert Command.normalize([{:set_text_datum, :br}]) ==
               {:ok, [{:set_text_datum, 8}]}
    end

    test "normalizes palette colors for render commands" do
      assert Command.normalize([{:set_palette_color, 1, :red}]) ==
               {:ok, [{:set_palette_color, 1, 0xFF0000}]}

      assert Command.normalize([{:set_palette_color, 2, {:rgb, 0, 128, 255}}]) ==
               {:ok, [{:set_palette_color, 2, 0x0080FF}]}
    end

    test "normalizes palette-mode sprite render commands" do
      assert Command.normalize([
               {:clear, {:index, 0}},
               {:fill_rect, 0, 0, 10, 10, {:index, 1}},
               {:set_text_color, {:index, 2}, {:index, 0}},
               {:push_sprite, 3, 10, 20, {:index, 0}}
             ]) ==
               {:ok,
                [
                  {:color_mode, :palette_index},
                  {:clear, 0},
                  {:fill_rect, 0, 0, 10, 10, 1},
                  {:set_text_color, {:index, 2}, {:index, 0}},
                  {:push_sprite, 3, 10, 20, {:index, 0}}
                ]}
    end

    test "switches primitive color modes only when required" do
      assert Command.normalize([
               {:clear, {:index, 0}},
               {:draw_pixel, 1, 1, {:index, 2}},
               {:draw_pixel, 2, 2, :red}
             ]) ==
               {:ok,
                [
                  {:color_mode, :palette_index},
                  {:clear, 0},
                  {:draw_pixel, 1, 1, 2},
                  {:color_mode, :rgb565},
                  {:draw_pixel, 2, 2, 0xF800}
                ]}
    end

    test "rejects invalid text datum names" do
      assert Command.normalize([{:set_text_datum, :not_a_datum}]) ==
               {:error, {:bad_text_datum, :not_a_datum}}
    end

    test "accepts both draw_string tuple orders" do
      assert Command.normalize([{:draw_string, "ok", 1, 2}, {:draw_string, 3, 4, "ng"}]) ==
               {:ok, [{:draw_string, "ok", 1, 2}, {:draw_string, "ng", 3, 4}]}
    end

    test "rejects invalid display option before command normalization" do
      assert Command.normalize([{:unknown, :command}], display: :yes) ==
               {:error, {:bad_render_display_option, :yes}}
    end

    test "rejects invalid commands" do
      assert Command.normalize([{:draw_rect, 0, 0, 0, 10, :white}]) ==
               {:error, {:bad_render_command, {:draw_rect, 0, 0, 0, 10, :white}}}

      assert Command.normalize([{:push_rotate_zoom_list, []}]) ==
               {:error, {:bad_render_command, {:push_rotate_zoom_list, []}}}
    end

    test "rejects invalid indexed primitive colors" do
      assert Command.normalize([{:fill_rect, 0, 0, 10, 10, {:index, 256}}]) ==
               {:error, {:bad_render_color, {:index, 256}}}
    end
  end
end
