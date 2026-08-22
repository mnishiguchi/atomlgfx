<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ESP-IDF 部品

このリポジトリは、LovyanGFX を AtomVM から利用するための NIF を提供する ESP-IDF 部品を含みます。
Elixir 側とネイティブ側は同じ Git コミットから取得してください。

## 提供するもの

- `LGFX` と `AtomLGFX` が利用する AtomVM NIF
- LovyanGFX を包む `lgfx_device` 装置適合層
- 基本図形、文字、画像、スプライト、パレット、タッチ操作
- 実行時のパネル・バス設定
- 高頻度描画向けのバイナリーバッチ実行
- 操作情報と診断情報

旧 AtomVM Port、メールボックス通信、要求／応答組の解析器は構築しません。

## 構成

```text
Elixir
  |
  v
AtomLGFX.Native
  |
  v
AtomVM NIF
  |
  +-- lgfx_device_*
  |
  +-- render_batch
          |
          v
       LovyanGFX
```

主な領域:

- `CMakeLists.txt`
  - ESP-IDF 部品の構築設定
- `native/nif.c`
  - AtomVM NIF 登録
- `native/include/atom_lgfx/nif.inc`
  - 直接 `LGFX` NIF
- `native/include/atom_lgfx/nif_dispatch.inc`
  - `AtomLGFX` の共通 NIF 呼び出し
- `native/render_batch.cpp`
  - バイナリーバッチ実行
- `native/device/`
  - LovyanGFX に面する装置適合層
- `third_party/LovyanGFX/`
  - 版を固定した LovyanGFX

`lgfx_port` は歴史的なディレクトリー名です。現在の実行経路は NIF のみです。

## 構築準備

LovyanGFX の副リポジトリを初期化します。

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

## AtomVM ファームウェアの構築

```bash
./scripts/atomvm_esp32.exs build --target esp32s3 --component .
```

構築と書き込みを続けて行う場合:

```bash
./scripts/atomvm_esp32.exs install --target esp32s3 --port /dev/ttyACM0
```

対象と直列ポートは環境に合わせて変更してください。

## 設定

構築時の既定値は CMake 設定から生成するネイティブ設定ヘッダーへ出力します。
実行時に変更する必要がある値は `LGFX.init/1` または `AtomLGFX.open/1` と
`AtomLGFX.init/1` から NIF へ渡せます。

```elixir
LGFX.init(
  panel_driver: :ili9488,
  width: 320,
  height: 480
)
```

所有権と設定の流れは [構成](architecture.md) を参照してください。

## 操作契約

`native/include/atom_lgfx/ops.def` は、NIF の共通呼び出しとバイナリーバッチで共有する
安定した操作番号と検証情報を定義します。

契約の説明は [ネイティブ操作契約](protocol.md)、生成された一覧は
[操作参照](protocol-reference.md) を参照してください。

変更した場合は生成物を同期します。

```bash
elixir scripts/sync_lgfx_protocol_doc.exs
elixir scripts/sync_lgfx_protocol_doc.exs --check
```

## 関連文書

- [構成](architecture.md)
- [ネイティブ実装](../native/README.md)
- [装置適合層](../native/device/README.md)
- [ネイティブ操作契約](protocol.md)
- [操作参照](protocol-reference.md)
