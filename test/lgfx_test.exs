# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule LGFXTest do
  use ExUnit.Case, async: true

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
end
