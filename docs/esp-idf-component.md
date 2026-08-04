<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ESP-IDF 部品

このリポジトリは、LovyanGFX を背後に持つ AtomVM のネイティブポートドライバーを提供する ESP-IDF 部品を含みます。

この部品は、現在の AtomLGFX v3 API に対応するワイヤープロトコル v3 を実装します。Hex パッケージには含めず、このリポジトリから配布します。`atomlgfx` の Elixir 依存と同じ Git コミットから構築してください。

対象は ESP32 系基板上の AtomVM ファームウェアです。同じリポジトリにある Elixir パッケージが使用する `AtomLGFX` API のネイティブ側を実装します。

## 提供するもの

- ネイティブ `lgfx_port` AtomVM ポートドライバー
- 組プロトコルの要求解析と振り分け
- LovyanGFX を使う表示操作
- スプライト、パレット、画像、文字、タッチ対応
- 描画処理をまとめる明示的なバイナリーバッチ送信
- プロトコル単位の機能情報と診断情報

## 提供しないもの

- 高水準の Elixir 向け使いやすさ
- Elixir 側の検証補助
- あらゆる基板に対するアプリケーション用設定手順

Elixir 側の利用方法は [Elixir パッケージ](elixir-package.md) を参照してください。

## 関連するリポジトリ領域

- `CMakeLists.txt`
  - 部品の入口
- `include/lgfx_port/lgfx_port.h`
  - 公開ネイティブヘッダー
- `lgfx_port/`
  - AtomVM に面するプロトコル境界
  - 通常要求の処理
  - 明示的なバイナリーバッチ送信の解析と検証
  - フレーム命令列の振り分け
- `lgfx_device/`
  - LovyanGFX に面する装置適合層
- `third_party/LovyanGFX/`
  - 版を固定した LovyanGFX の副リポジトリ

## 構築準備

先に LovyanGFX の副リポジトリを初期化します。

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

## AtomVM ファームウェアの構築と書き込み

このリポジトリには、ネイティブドライバーを含む AtomVM ファームウェアを構築する補助スクリプトがあります。

```bash
./scripts/atomvm_esp32.exs install --target esp32s3 --port /dev/ttyACM0
```

対象と直列ポートは環境に合わせて変更してください。

## 設定

構築時の既定値は CMake 設定から導出し、生成されるネイティブ設定ヘッダーへ出力します。一部の値は、ホストアプリケーションがポートを開く際に上書きできます。

所有権、実行方式、設定の流れは [構成](architecture.md) を参照してください。

## プロトコルと機能

この部品は、`AtomLGFX` が使う組プロトコルのネイティブ側を実装します。

要求と応答の意味、検証規則、データ表現、バッチ処理は [プロトコル](protocol.md) を参照してください。生成された操作表、エラー理由、機能語彙は [プロトコル参照](protocol-reference.md) にあります。

## 内部設計文書

主にネイティブ層の保守者向けです。

- [ポート層](../lgfx_port/README.md)
- [装置適合層](../lgfx_device/README.md)
- [構成](architecture.md)
- [プロトコル](protocol.md)
- [プロトコル参照](protocol-reference.md)

## 変更時の注意

外部から見えるプロトコル動作を変更する場合は、次を行います。

- 必要に応じて `lgfx_port/include_internal/lgfx_port/ops.def` を更新する
- 必要に応じてハンドラー、バイナリーバッチ振り分け、装置コードを更新する
- 外部契約が変わる場合は `docs/protocol.md` を更新する
- 生成されたプロトコル表を同期する

```bash
elixir scripts/sync_lgfx_protocol_doc.exs
elixir scripts/sync_lgfx_protocol_doc.exs --check
```
