<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Elixir パッケージ

`atomlgfx` は AtomVM 上の Elixir から LovyanGFX を利用するための公開前パッケージです。
ネイティブ実行経路は AtomVM NIF のみです。

## 必要条件

同じ Git コミットから構築した AtomLGFX ESP-IDF 部品を AtomVM ファームウェアへ含めます。

Git 依存として使う場合:

```elixir
defp deps do
  [
    {:atomlgfx,
     git: "https://github.com/mnishiguchi/atomlgfx.git",
     ref: "FULL_GIT_COMMIT_SHA"}
  ]
end
```

## 基本 API

LovyanGFX の一般的なコード例を移植する場合は `LGFX` を使います。

```elixir
:ok = LGFX.init(panel_driver: :ili9488, width: 320, height: 480)

:ok = LGFX.fill_screen(:black)
:ok = LGFX.draw_rect(20, 20, 100, 60, :red)
:ok = LGFX.fill_circle(160, 120, 30, :blue)
:ok = LGFX.draw_string("Hello", 20, 100)
:ok = LGFX.display()
```

構築時設定だけでよい場合は `LGFX.init/0` を使えます。

## 高度な API

スプライト、パレット、タッチ、JPEG など既存の広い機能面には `AtomLGFX` を使います。
`AtomLGFX.open/1` が返す値は Elixir 側の設定を識別するハンドルであり、BEAM Port ではありません。

```elixir
{:ok, handle} = AtomLGFX.open(panel_driver: :ili9488, width: 320, height: 480)
:ok = AtomLGFX.init(handle)

:ok = AtomLGFX.create_sprite(handle, 120, 72, 16, 1)

:ok =
  AtomLGFX.render_sprite(handle, 1, [
    {:clear, :navy},
    {:draw_rect, 0, 0, 120, 72, :white},
    {:draw_string, "sprite", 8, 8}
  ])

:ok =
  AtomLGFX.render_lcd(handle, [
    {:fill_screen, :black},
    {:push_sprite, 1, 24, 32},
    :display
  ])
```

## 描画バッチ

複数命令を一度だけ実行する場合は `LGFX.batch/2` または `AtomLGFX.render/3` を使います。
同じ命令列を高頻度で繰り返す場合は、符号化をホットループの外へ出します。

```elixir
{:ok, batch} =
  LGFX.encode_batch([
    {:fill_rect, 10, 10, 100, 40, :red},
    {:draw_string, "Hello", 20, 20}
  ])

:ok = LGFX.submit_batch(batch)
:ok = LGFX.submit_batch(batch)
```

低水準の命令列を明示的に組み立てる場合は `AtomLGFX.BinaryBatch` を利用できます。

## 責務

Elixir 側は次を担当します。

- 公開 API
- 入力の検証と正規化
- 色や文字基準位置などの扱いやすい表現
- バイナリーバッチの符号化

NIF は AtomVM term の検証と `native/device/` 装置層の呼び出しを担当し、描画そのものは LovyanGFX に任せます。

## 関連文書

- [構成](architecture.md)
- [ESP-IDF 部品](esp-idf-component.md)
- [ネイティブ操作契約](protocol.md)
- [ネイティブ実装](../native/README.md)
- [装置適合層](../native/device/README.md)

## 互換性

Elixir とネイティブ部品は同じ Git コミットから取得してください。
`mix.exs` の `0.1.0` は Mix が要求する仮の値であり、公開済み版ではありません。
