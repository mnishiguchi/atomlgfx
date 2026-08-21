# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.RenderApiTest do
  use ExUnit.Case, async: true

  describe "render target helpers" do
    test "rejects invalid explicit render targets before NIF submission" do
      assert AtomLGFX.render_to(:not_a_handle, 255, []) == {:error, {:bad_render_target, 255}}

      assert AtomLGFX.render_to(:not_a_handle, :unknown, []) ==
               {:error, {:bad_render_target, :unknown}}
    end

    test "rejects invalid sprite targets before NIF submission" do
      assert AtomLGFX.render_sprite(:not_a_handle, 0, []) == {:error, {:bad_sprite_target, 0}}
      assert AtomLGFX.render_sprite(:not_a_handle, 255, []) == {:error, {:bad_sprite_target, 255}}
    end

    test "rejects invalid helper options before NIF submission" do
      assert AtomLGFX.render_to(:not_a_handle, :lcd, [], :bad_opts) ==
               {:error, {:bad_render_options, :bad_opts}}

      assert AtomLGFX.render_sprite(:not_a_handle, 1, [], :bad_opts) ==
               {:error, {:bad_render_options, :bad_opts}}
    end

    test "rejects target overrides in single-target helpers" do
      assert AtomLGFX.render_lcd(:not_a_handle, [{:target, 1}, {:clear, :black}]) ==
               {:error, {:render_target_override, 1}}

      assert AtomLGFX.render_sprite(:not_a_handle, 1, [{:target, :lcd}, {:clear, :black}]) ==
               {:error, {:render_target_override, :lcd}}
    end

    test "reports invalid commands without misclassifying valid targets" do
      assert AtomLGFX.render_to(:not_a_handle, 0, :bad_commands) ==
               {:error, {:bad_render_commands, :bad_commands}}

      assert AtomLGFX.render_sprite(:not_a_handle, 1, :bad_commands) ==
               {:error, {:bad_render_commands, :bad_commands}}
    end
  end

  describe "common LovyanGFX facade" do
    test "exports direct draw_pixel for occasional scalar drawing" do
      functions = AtomLGFX.__info__(:functions)

      assert {:draw_pixel, 4} in functions
      assert {:draw_pixel, 5} in functions
    end

    test "rejects unknown text datum atoms before NIF submission" do
      assert AtomLGFX.set_text_datum(:not_a_handle, :unknown_datum) ==
               {:error, {:bad_text_datum, :unknown_datum}}
    end
  end
end
