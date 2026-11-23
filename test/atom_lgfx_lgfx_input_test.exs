# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.LGFXInputTest do
  use ExUnit.Case, async: true

  test "初期化・描画・照会・終了はホスト上で NIF 境界へ到達する" do
    calls = [
      {:init, []},
      {:init, [[panel_driver: :ili9488, width: 320, height: 480]]},
      {:width, []},
      {:height, []},
      {:start_write, []},
      {:fill_screen, [:black]},
      {:draw_rect, [10, 20, 30, 40, :red]},
      {:set_text_datum, [:middle_center]},
      {:set_text_color, [:white]},
      {:set_text_color, [:white, :black]},
      {:draw_string, ["Hello", 16, 16]},
      {:println, []},
      {:println, ["Hello"]},
      {:push_image, [0, 0, 1, 1, <<0x00, 0xF8>>]},
      {:end_write, []},
      {:display, []},
      {:close, []}
    ]

    for {name, args} <- calls do
      assert apply(AtomLGFX.LGFX, name, args) == :nif_not_loaded
    end
  end

  test "既定値と target / display を含むバッチを符号化する" do
    commands = [
      {:fill_screen, :black},
      {:set_text_color, :white, :navy},
      {:fill_rect, 10, 20, 30, 40, :red},
      {:draw_string, "Hello", 16, 16}
    ]

    assert AtomLGFX.LGFX.encode_batch(commands) == AtomLGFX.LGFX.encode_batch(commands, [])

    for opts <- [[], [target: :lcd], [display: true], [target: 1, display: true]] do
      assert {:ok, binary} = AtomLGFX.LGFX.encode_batch(commands, opts)
      assert AtomLGFX.LGFX.submit_batch(binary) == :nif_not_loaded
      assert AtomLGFX.LGFX.batch(commands, opts) == :nif_not_loaded
    end

    assert AtomLGFX.LGFX.batch(commands) == AtomLGFX.LGFX.batch(commands, [])

    refute AtomLGFX.LGFX.encode_batch(commands) ==
             AtomLGFX.LGFX.encode_batch(commands, display: true)
  end

  test "設定・色・文字配置・バッチの入力エラーを返す" do
    calls = [
      {:init, [:invalid]},
      {:init, [[panel_driver: :unknown]]},
      {:fill_screen, [:unknown]},
      {:draw_rect, [0, 0, 10, 20, :unknown]},
      {:set_text_color, [:unknown]},
      {:set_text_color, [:white, :unknown]},
      {:set_text_datum, [:unknown]},
      {:submit_batch, [:invalid]},
      {:batch, [:invalid]},
      {:batch, [[{:unknown, 1}]]},
      {:encode_batch, [[{:fill_screen, :unknown}]]},
      {:encode_batch, [[], :invalid]},
      {:encode_batch, [[], [target: :unknown]]},
      {:batch, [[], [display: :invalid]]}
    ]

    for {name, args} <- calls do
      assert {:error, _reason} = apply(AtomLGFX.LGFX, name, args)
    end
  end
end
