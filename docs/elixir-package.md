<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Elixir パッケージ

このリポジトリは、AtomVM 向けの公開前 Elixir ラッパー `atomlgfx` を提供します。現在の実装はワイヤープロトコル v3 を実装するため v3 と呼びますが、独立したパッケージ版や API 版はまだ設けていません。

このパッケージは、同じリポジトリにあるネイティブドライバー `lgfx_port` のラッパーです。共有ネイティブプロトコルの上に、Elixir 向け API、補助関数、入力の正規化を提供します。

現在のネイティブプロトコルは v3 です。

- 通常の描画や制御は、平坦な組による同期要求として実行する
- 一般的な描画命令は、通常 `AtomLGFX.render/3` でまとめる
- `AtomLGFX.render_lcd/3` と `AtomLGFX.render_sprite/4` は、同じ描画優先経路を使いながら対象を明示する
- スプライトへの描画は `AtomLGFX.render_sprite/4` で行い、LCD 側の描画命令で `{:push_sprite, ...}` を使える
- `AtomLGFX.render/3` は LovyanGFX 風の命令を既存の `AtomLGFX.BinaryBatch` プロトコルへ変換する
- `AtomLGFX.BinaryBatch` は、低水準試験、診断、調整済みフレーム命令列のため引き続き利用できる
- 初期設定、照会、確保、補正、スプライトのライフサイクル、パレット作成、タッチ操作は通常の呼び出し経路を使う

## 必要条件

このパッケージには、ネイティブ `lgfx_port` ドライバーを組み込んだ AtomVM ファームウェアが必要です。

ネイティブドライバーと Elixir ラッパーは同じリポジトリで一体として開発しています。必ず同じ Git コミットから取得してください。

## 導入

Git 依存として使う場合は、ネイティブドライバーの構築に使ったコミットへ固定します。

```elixir
defp deps do
  [
    {:atomlgfx,
     git: "https://github.com/mnishiguchi/atomlgfx.git",
     ref: "FULL_GIT_COMMIT_SHA"}
  ]
end
```

依存関係を取得します。

```bash
mix deps.get
```

## 基本的な使い方

```elixir
{:ok, port} = AtomLGFX.open(panel_driver: :ili9488, width: 320, height: 480)

:ok = AtomLGFX.init(port)
:ok = AtomLGFX.display(port)

:ok =
  AtomLGFX.render_lcd(port, [
    {:fill_screen, :black},
    {:set_text_font_preset, :jp},
    {:set_text_size, 2},
    {:set_text_color, :white, :black},
    {:set_text_datum, :top_left},
    {:draw_string, "こんにちは", 16, 16},
    {:draw_string, "日本語テキスト", 16, 56},
    :display
  ])
```

基板と表示装置に合わせて、設定値と関数呼び出しを調整してください。

## 描画バッチ

通常の描画では、まず `AtomLGFX.render/3` を使います。`AtomLGFX.render_lcd/3` と `AtomLGFX.render_sprite/4` は、同じ描画優先経路を使いながら対象を明示する薄い補助関数です。

描画命令は、名前付き色、`{:rgb, r, g, b}`、`{:rgb565, value}`、`{:rgb888, value}` などの一般的な色入力を正規化します。文字基準位置には `:top_left`、`:middle_center`、`:bottom_right` などの LovyanGFX 風アトムを指定できます。パレット付きスプライトの基本図形では `{:index, n}` を明示し、必要な低水準色モードの切り替えは描画正規化側が挿入します。

```elixir
:ok = AtomLGFX.create_sprite(port, 120, 72, 16, 1)

:ok =
  AtomLGFX.render_sprite(port, 1, [
    {:clear, :navy},
    {:draw_rect, 0, 0, 120, 72, :white},
    {:set_cursor, 8, 8},
    {:set_text_color, :white},
    {:println, "sprite"}
  ])

:ok =
  AtomLGFX.render_lcd(port, [
    {:fill_screen, :black},
    {:push_sprite, 1, 24, 32},
    :display
  ])
```

ワイヤー形式を厳密に制御する必要がある試験、性能測定、調整済みの例では、`AtomLGFX.BinaryBatch` を直接使います。これは安定した公開窓口であり、符号化、送信、検証、診断の詳細は用途別の内部モジュールへ分離しています。通常の利用者は、内部モジュールではなく `AtomLGFX.render/3` または公開 `AtomLGFX.BinaryBatch` を使用してください。

```elixir
frame = [
  AtomLGFX.BinaryBatch.target(0),
  AtomLGFX.BinaryBatch.fill_screen(0x0000),
  AtomLGFX.BinaryBatch.draw_line(0, 0, 319, 239, 0xFFFF),
  AtomLGFX.BinaryBatch.fill_rect(20, 20, 80, 40, 0x07E0),
  AtomLGFX.BinaryBatch.display()
]

:ok = AtomLGFX.BinaryBatch.render(port, frame)
```

送信成功は、命令列が同期的に解析・実行されたことを表します。不正なバイト列はプロトコルエラーとなり、未対応の描画命令は拒否されます。

## 高負荷アニメーション

メモリー不足の危険を下げるため、試験的な保持描画プログラム API は削除しました。通常の描画のまとまりには、使いやすい描画 API を使用してください。

MovingIcons のような計測対象の高負荷処理では、物体状態を Elixir 側に保持し、上級者向け `AtomLGFX.BinaryBatch.push_rotate_zoom_list/2` の利用を用途固有の描画器へ隔離します。これにより、一般 API を分かりやすく保ち、大きな隠れバッファや例固有の概念をプロトコルへ持ち込みません。

## 責務

このパッケージが担当する範囲は次のとおりです。

- Elixir 向け API の形
- 補助関数
- ラッパー側の検証と正規化
- ラッパー内の使いやすさ

ネイティブプロトコル契約や LovyanGFX の装置意味は、このパッケージでは定義しません。

関連文書:

- [構成](architecture.md)
- [プロトコル](protocol.md)
- [ESP-IDF 部品](esp-idf-component.md)
- [ポート層](../lgfx_port/README.md)
- [装置適合層](../lgfx_device/README.md)

## 互換性

v3 公開 API が、現在対応している公開前の操作面です。ネイティブ部品と Elixir ラッパーは必ず同じ Git コミットから取得してください。異なる開発リビジョンを混在させると、`bad_proto`、`bad_op`、機能不一致が生じる可能性があります。

`mix.exs` の `0.1.0` は Mix が要求する仮の記録値であり、公開済み AtomLGFX の版ではありません。
