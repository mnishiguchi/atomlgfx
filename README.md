<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# atomlgfx

`atomlgfx` は、ESP32 系基板で動く AtomVM から LovyanGFX を利用するためのライブラリです。

用途に応じて、2つの Elixir API を提供します。

- `LGFX`
  - LovyanGFX に近い、日常的な描画向け API
- `AtomLGFX`
  - スプライト、パレット、タッチ、JPEG、切り抜き、実行時設定などを含む広い API

どちらも AtomVM NIF を通じて同じ LovyanGFX 装置層を利用します。
旧 AtomVM Port、メールボックス通信、要求／応答組の解析経路はありません。

## できること

- 文字や基本図形の描画
- JPEG や RGB565 画像の描画
- スプライトとパレットの利用
- タッチ入力と較正
- 実行時のパネル・バス設定
- 複数の描画命令をまとめたバイナリーバッチ
- 高頻度処理向けの事前符号化バッチ

## `LGFX`

一般的な LCD 描画では `LGFX` を推奨します。

```elixir
:ok =
  LGFX.init(
    panel_driver: :ili9488,
    width: 320,
    height: 480
  )

:ok = LGFX.fill_screen(:black)
:ok = LGFX.draw_string("Hello AtomLGFX", 16, 16)

:ok =
  LGFX.batch([
    {:draw_line, 16, 48, 240, 48, :red},
    :display
  ])
```

構築時の既定設定だけを使う場合は `LGFX.init/0` も利用できます。

同じ命令列を高頻度で繰り返す場合は、符号化を描画処理の外へ出します。

```elixir
{:ok, batch} =
  LGFX.encode_batch([
    {:fill_rect, 10, 10, 100, 40, :red},
    {:draw_string, "Hello", 20, 20}
  ])

:ok = LGFX.submit_batch(batch)
:ok = LGFX.submit_batch(batch)
```

## `AtomLGFX`

スプライト、パレット、タッチ、JPEG などの高度な機能には `AtomLGFX` を使います。

```elixir
{:ok, handle} =
  AtomLGFX.open(
    panel_driver: :ili9488,
    width: 320,
    height: 480
  )

:ok = AtomLGFX.init(handle)

:ok =
  AtomLGFX.render_lcd(handle, [
    {:fill_screen, :black},
    {:set_text_color, :white},
    {:draw_string, "Hello AtomLGFX", 16, 16},
    {:draw_line, 16, 48, 240, 48, :red},
    :display
  ])
```

`AtomLGFX.open/1` は BEAM Port を開きません。
実行時設定と Elixir 側の状態を識別するハンドルを作成します。
設定と実行時キャッシュはプロセス辞書に保持されるため、ハンドルは作成した
プロセス内で使用してください。不明なハンドルでの初期化は
`{:error, :invalid_handle}` を返します。

ネイティブ側の LCD 装置は単一実体です。

## 全体像

```text
Elixir application
       |
       +-- LGFX
       |
       +-- AtomLGFX
              |
              v
       AtomLGFX.Native
              |
              v
          AtomVM NIF
              |
              +-- direct NIF calls
              |
              +-- binary batch
                       |
                       v
                  render_batch
                       |
                       v
                 native/device/
                       |
                       v
                   LovyanGFX
                       |
                       v
              LCD / touch device
```

直接 NIF 呼び出しとバイナリーバッチで描画処理を二重実装せず、同じ LovyanGFX 装置層を利用します。

## 導入

`atomlgfx` を利用するには、次の2つを同じ Git コミットから取得します。

- AtomLGFX ESP-IDF 部品を組み込んだ AtomVM ファームウェア
- Elixir 側の `atomlgfx` 依存

Git 依存として利用する場合:

```elixir
defp deps do
  [
    {:atomlgfx,
     git: "https://github.com/mnishiguchi/atomlgfx.git",
     ref: "FULL_GIT_COMMIT_SHA"}
  ]
end
```

AtomVM ファームウェアへの組み込み方法は
[ESP-IDF 部品](docs/esp-idf-component.md) を参照してください。

## 文書

- [文書案内](docs/README.md)
- [構成](docs/architecture.md)
- [Elixir パッケージ](docs/elixir-package.md)
- [ESP-IDF 部品](docs/esp-idf-component.md)
- [ネイティブ操作契約](docs/protocol.md)
- [M5Stack 基板](docs/boards/m5stack.md)
- [構成設計判断記録](docs/adr/README.md)
- [変更履歴](CHANGELOG.md)
