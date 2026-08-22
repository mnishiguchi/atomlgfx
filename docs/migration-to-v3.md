<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# v3 API への移行

この文書では、旧 v2 API から現在の描画優先 API への移行を説明します。開発中にこの API 世代を v3 と呼んでいましたが、現在のネイティブ実行経路は AtomVM NIF のみで、BEAM Port のワイヤープロトコルは使用しません。

## 二つの成果物を同時に更新する

Elixir ラッパーとネイティブ ESP-IDF 部品は、同じ Git コミットから取得してください。現在のラッパーを v1、v2、または別の開発リビジョンのネイティブドライバーと組み合わせないでください。

## 描画優先 API を使う

通常の描画命令は、一つの処理単位へまとめます。

```elixir
:ok =
  AtomLGFX.render_lcd(handle, [
    {:fill_screen, :black},
    {:set_text_color, :white},
    {:set_text_datum, :top_left},
    {:set_cursor, 8, 8},
    {:println, "Hello AtomLGFX"},
    {:draw_line, 0, 32, 160, 32, :red},
    :display
  ])
```

スプライトへ描画する場合は `AtomLGFX.render_sprite/4` を使います。初期化、照会、タッチ、スプライトのライフサイクル、JPEG 描画、生 RGB565 転送、単発操作には直接関数が適しています。

## 旧バッチ API を置き換える

旧来の組・一覧バッチ構築器、旧バッチモジュール、旧送信関数、保持型ネイティブ描画プログラムの利用を削除してください。

まず、使いやすい `AtomLGFX.render/3`、`render_lcd/3`、`render_sprite/4` を使います。`AtomLGFX.BinaryBatch` は、診断や計測対象の高負荷処理向け上級 API として引き続き利用できます。

## 正規の Elixir 名を使う

公開関数と低水準操作名には `snake_case` を使います。LovyanGFX C++ の `fillRect` は、Elixir では `fill_rect` に対応します。

描画命令では、`:black`、`:white`、`{:rgb, r, g, b}` などの扱いやすい色を指定できます。パレット付きスプライトでは `{:index, n}` を使います。文字基準位置には `:top_left`、`:middle_center`、`:bottom_right` などを指定できます。

## 大きなデータを明示的に扱う

JPEG と RGB565 の画像データは、意図的に描画バッチへ埋め込みません。確保とデータ所有権を見える状態に保つため、`AtomLGFX.draw_jpg/5` または `/11`、`AtomLGFX.push_image_rgb565/8` を引き続き使います。

## 移行確認表

- Elixir 依存とネイティブ部品を同じ Git コミットへ固定する
- 削除済みの組・一覧バッチと保持描画 API を置き換える
- 通常の基本図形と文字を描画優先関数でまとめる
- 初期設定、照会、確保、タッチ、JPEG、生画像転送は直接呼び出しのままにする
- 正規の `snake_case` 名を使う
- スプライト、パレット、タッチ、バイナリーバッチを使う前に任意機能の有無を確認する
- 更新後に対象機器で `SAMPLE_APP_MODE=all` を実行する
