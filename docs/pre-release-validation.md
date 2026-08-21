<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# 公開前検証の確認表

AtomLGFX は公開前です。Elixir と ESP-IDF 部品は同じ Git コミットから取得して検証します。

## Elixir

```bash
mix deps.get
mix format --check-formatted
mix compile --warnings-as-errors
mix test
mix docs --warnings-as-errors
mix hex.build
```

例示アプリケーションも別に確認します。

```bash
cd examples/elixir
mix deps.get
mix format --check-formatted
mix compile --warnings-as-errors
```

操作定義を変更した場合は生成物も確認します。

```bash
elixir scripts/sync_lgfx_protocol_doc.exs --check
```

## ネイティブ

固定した LovyanGFX を初期化し、NIF を含む AtomVM ファームウェアを完全構築します。

```bash
git submodule sync --recursive
git submodule update --init --recursive
./scripts/atomvm_esp32.exs build --target esp32s3 --component .
```

次を確認します。

- ネイティブコンパイラー警告がない
- `lgfx_port/nif.c` が構築される
- 旧 Port の入口や要求解析器が構築対象に存在しない
- 画像が対象パーティションへ収まる
- 生成設定が対象のパネル、バス、タッチ、PSRAM 設定と一致する

## 実機

新しく構築したネイティブファームウェアを書き込んだ上で、少なくとも次を確認します。

- `SAMPLE_APP_MODE=nif`
- `SAMPLE_APP_MODE=all`
- `SAMPLE_APP_MODE=moving_icons`

`nif` では次を必須とします。

- `LGFX.init/0` または `LGFX.init/1`
- 基本図形と文字描画
- RGB565 画像転送
- 個別 NIF 呼び出し
- 事前符号化バッチ
- 不正引数のエラー経路
- `LGFX.close/0`

`all` では `AtomLGFX` の高度な機能も NIF 経由で確認します。

- 実行時設定
- JPEG
- スプライトとパレット
- 切り抜き
- タッチと較正の利用可能性確認
- 機能情報と診断情報

記録する内容:

- Git コミット
- AtomVM リビジョンと ESP-IDF 版
- 基板、パネル、解像度、配線
- PSRAM の有無
- ファームウェア画像サイズ
- 動作確認結果
- 性能測定値と既知の制約

## 公開前モデル

`mix.exs` の `0.1.0` は Mix が要求する仮の値です。明示的な公開版方針を採用するまでは、
SemVer タグや Hex 公開版として扱いません。
