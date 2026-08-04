<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# atomlgfx

`atomlgfx` は、ESP32 系基板で動く AtomVM から LovyanGFX を利用するためのライブラリです。

Elixir 向けの `AtomLGFX` API と、LovyanGFX を呼び出す AtomVM のネイティブポートドライバー `lgfx_port` を提供します。

## できること

- 文字や基本図形の描画
- JPEG や生画像の描画
- スプライトの作成と転送
- タッチ入力の取得
- 複数の描画命令をまとめた効率的な送信

## 基本的な使い方

```elixir
{:ok, port} = AtomLGFX.open(panel_driver: :ili9488, width: 320, height: 480)

:ok = AtomLGFX.init(port)

:ok =
  AtomLGFX.render_lcd(port, [
    {:fill_screen, :black},
    {:set_text_color, :white},
    {:draw_string, "Hello AtomLGFX", 16, 16},
    {:draw_line, 16, 48, 240, 48, :red},
    :display
  ])
```

複数の描画命令をまとめることで、AtomVM とネイティブドライバー間の呼び出し回数を抑えられます。

## 全体像

```text
Elixir アプリケーション
        |
        v
     AtomLGFX
        |
        v
     lgfx_port
        |
        v
     LovyanGFX
        |
        v
  液晶画面・タッチ装置
```

Elixir 側では扱いやすい API を提供し、ネイティブ側では AtomVM と LovyanGFX の橋渡しを行います。

## 導入

`atomlgfx` を利用するには、`lgfx_port` を組み込んだ AtomVM ファームウェアと、Elixir 側の `atomlgfx` 依存が必要です。両者は同じ Git コミットから取得してください。

- [Elixir パッケージの導入と使い方](docs/elixir-package.md)
- [AtomVM ファームウェアへの組み込み](docs/esp-idf-component.md)
- [M5Stack 向けの情報](docs/boards/m5stack.md)

## 詳しい文書

- [構成](docs/architecture.md)
- [プロトコル](docs/protocol.md)
- [変更履歴](CHANGELOG.md)
