<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# v1 から v2 への移行

> 過去資料: この文書は、すでに置き換えられた v2 API を説明する履歴資料です。現在の実装については [構成](architecture.md) と [Elixir パッケージ](elixir-package.md) を参照してください。

この文書は、`v1` ブランチの v1 から、当時 `main` にあった v2 へ移行する際の実務上の変更をまとめたものです。

v2 も、ESP32 系基板上で AtomVM から使う LovyanGFX ラッパーという目的は同じです。主な変更点はプロトコルとバッチ方式です。

## 概要

v1 は組プロトコルと、高水準の組・一覧バッチ経路を使っていました。

v2 は次を使います。

- 通常操作には同期的な組呼び出し
- 公開低水準 Elixir 呼び出しには正規の `snake_case` 操作名
- 描画処理をまとめる明示的なバイナリーバッチ経路
- ネイティブプロトコル、Elixir ラッパー、文書を同期する生成情報

ネイティブ部品と Elixir パッケージは同時に更新します。

## ネイティブ部品と Elixir ラッパーの互換性

ESP-IDF 部品と Elixir パッケージを同時に更新してください。

次の組み合わせは使用しません。

- v1 ネイティブドライバーと v2 Elixir ラッパー
- v2 ネイティブドライバーと v1 Elixir ラッパー

公開前期間は、プロトコル面、操作情報、機能ビットを一体として変更します。

## 通常の呼び出し

一般的な描画と制御は、公開 `AtomLGFX` API を使います。

```elixir
{:ok, port} = AtomLGFX.open(panel_driver: :ili9488, width: 320, height: 480)

:ok = AtomLGFX.ping(port)
:ok = AtomLGFX.init(port)

:ok = AtomLGFX.fill_screen(port, 0x0000)
:ok = AtomLGFX.set_text_font_preset(port, :jp)
:ok = AtomLGFX.set_text_size(port, 2)
:ok = AtomLGFX.set_text_color(port, 0xFFFF, 0x0000)
:ok = AtomLGFX.draw_string(port, 16, 16, "こんにちは")
:ok = AtomLGFX.display(port)
```

低水準の公開 Elixir 呼び出しでは、正規の `snake_case` 操作アトムを使います。

```elixir
AtomLGFX.call(port, :fill_rect, [20, 20, 80, 40, 0x07E0])
```

LovyanGFX 風の `:fillRect` などの `camelCase` アトムは v2 では対応しません。アプリケーションと生呼び出しでは `snake_case` を使います。

## バッチの移行

最も大きいソース変更はバッチ利用です。

### v1 の書き方

```elixir
batch =
  AtomLGFX.batch()
  |> AtomLGFX.Batch.add(AtomLGFX.Batch.Command.clear(0x0000))
  |> AtomLGFX.Batch.add(AtomLGFX.Batch.Command.draw_rect(8, 8, 120, 80, 0xFFFF))
  |> AtomLGFX.Batch.add(AtomLGFX.Batch.Command.line(8, 8, 127, 87, 0x07E0))

{:ok, _} = AtomLGFX.submit_batch(port, batch)
```

### v2 の書き方

v2 では、`AtomLGFX.BinaryBatch` で一つの明示的な描画命令列を構築します。

```elixir
frame = [
  AtomLGFX.BinaryBatch.target(0),
  AtomLGFX.BinaryBatch.fill_screen(0x0000),
  AtomLGFX.BinaryBatch.draw_rect(8, 8, 120, 80, 0xFFFF),
  AtomLGFX.BinaryBatch.draw_line(8, 8, 127, 87, 0x07E0),
  AtomLGFX.BinaryBatch.display()
]

:ok = AtomLGFX.BinaryBatch.render(port, frame)
```

## バッチの範囲

v2 のバイナリーバッチ経路は意図的に明示します。一つの小さなフレーム命令列を同期実行する用途に適しています。

`AtomLGFX.BinaryBatch` が構築器を提供する場合、文字、スプライト転送、回転・拡大縮小、パレット色書き込みなどの描画時命令を含められます。JPEG 描画と RGB565 画像転送は、意図的にバイナリーバッチ形式から除外しているため、通常 API に残します。

次は通常呼び出し経路へ残します。

- 初期設定、照会、確保、補正
- スプライトのライフサイクル
- パレット作成
- タッチ操作

高負荷経路を明示したまま、汎用の遅延 LovyanGFX API へ不用意に拡大することを防ぎます。

## 機能確認

任意機能へ依存する前に、対応状況を確認します。

```elixir
{:ok, true} = AtomLGFX.supports_batch?(port)
{:ok, max_bytes} = AtomLGFX.max_binary_bytes(port)
```

バイナリーバッチに対応していない場合は、通常の描画呼び出しへ切り替えます。

## エラー処理

v2 の通常操作は、成功または失敗を直ちに返します。

代表的な返り値:

- `:ok`
- `{:ok, value}`
- `{:error, reason}`

バイナリーバッチでは次の意味になります。

- 成功は、フレーム命令列が同期的に解析・実行されたことを示す
- 不正な描画命令は `bad_args` などのプロトコルエラーを返す
- 未対応の描画命令番号は `bad_op` を返す

エラー理由はプロトコル単位の値として扱い、プロトコル文書で明示されていない内部詳細へ過度に一致させないでください。

## 移行確認表

- ネイティブ ESP-IDF 部品を v2 へ更新する
- `AtomLGFX.batch/0`、`AtomLGFX.Batch`、`AtomLGFX.submit_batch/2` を置き換える
- `AtomLGFX.BinaryBatch` のフレーム構築器と `AtomLGFX.submit_binary_batch/2` または `AtomLGFX.BinaryBatch.render/2` を使う
- 通常操作には公開 `AtomLGFX` ラッパーを使う
- 低水準 `AtomLGFX.call/4` では `snake_case` アトムを使う
- 初期設定、照会、確保、補正、スプライトのライフサイクル、パレット作成、タッチ操作をバイナリーバッチへ入れない
- 実機でプロトコル動作確認を行う
- アプリケーション上重要な描画経路で性能動作確認を行う
