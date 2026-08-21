# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule LGFXTest do
  use ExUnit.Case, async: true

  test "実行時設定を正規化して NIF 初期化へ渡す" do
    assert LGFX.init(panel_driver: :ili9488, width: 320, height: 480) == :nif_not_loaded

    assert LGFX.init(panel_driver: :unknown) ==
             {:error,
              {:bad_open_option_value, :panel_driver, :unknown,
               ":ili9488, :ili9341, :ili9341_2, :st7789, or :ili9342c"}}
  end

  test "名前付き色を受け付ける" do
    assert LGFX.fill_screen(:red) == :nif_not_loaded
    assert LGFX.draw_rect(10, 20, 30, 40, :white) == :nif_not_loaded
  end

  test "不明な名前付き色を拒否する" do
    assert LGFX.fill_screen(:not_a_color) ==
             {:error, {:unknown_color_name, :not_a_color}}
  end

  test "名前付き文字基準位置を受け付ける" do
    assert LGFX.set_text_datum(:middle_center) == :nif_not_loaded
  end

  test "描画命令一覧を既存のバイナリーバッチ形式へ符号化する" do
    assert LGFX.batch([
             {:fill_screen, :black},
             {:fill_rect, 10, 10, 100, 40, :red},
             {:draw_string, "Hello", 20, 20}
           ]) == :nif_not_loaded
  end

  test "繰り返し送信するバッチを事前に符号化する" do
    assert {:ok, command_binary} =
             LGFX.encode_batch([
               {:fill_rect, 10, 10, 100, 40, :red},
               {:draw_string, "Hello", 20, 20}
             ])

    assert is_binary(command_binary)
    assert LGFX.submit_batch(command_binary) == :nif_not_loaded

    assert LGFX.submit_batch(:not_a_binary) ==
             {:error, {:bad_render_batch, :not_a_binary}}
  end
end
