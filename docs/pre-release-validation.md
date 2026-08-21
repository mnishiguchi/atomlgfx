<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# 公開前検証の確認表

AtomLGFX には、まだパッケージの公開版を割り当てていません。この確認表は、SemVer の公開版を示さずに開発上の節目を検証するためのものです。Elixir ラッパーとネイティブ ESP-IDF 部品は、同じ Git リビジョンから取得し、同じワイヤープロトコル版を実装する必要があります。

## 節目の準備

- 現在の API 節目とワイヤープロトコル版が文書化されていることを確認する
- 利用者向け変更は、変更履歴の `未公開` 見出しへ置く
- Mix が要求する `0.1.0` を、公開版ではなく仮の記録値として扱う
- 生成されたプロトコルファイルが同期していることを確認する
- 作業ツリーが空で、継続的検証が成功していることを確認する

## Elixir パッケージの検証

```bash
mix deps.get
mix format --check-formatted
mix compile --warnings-as-errors
mix test
mix docs --warnings-as-errors
mix hex.build
```

例示アプリケーションは独自の整形設定と依存関係を持つため、別に確認します。

```bash
cd examples/elixir
mix deps.get
mix format --check-formatted
mix compile --warnings-as-errors
```

包装の動作確認として、ローカルで構築した Hex 書庫を調べます。Elixir ソース、README、変更履歴、ライセンス、現在の利用者向け文書を含む必要があります。ネイティブ部品が書庫に内包される、またはパッケージがすでに公開済みであると誤解させてはいけません。

## ネイティブ部品の検証

固定した LovyanGFX ソースを初期化し、完全な AtomVM ファームウェア画像を構築します。ラッパーだけを含むアプリケーション書き込みでは、ネイティブ変更を検証できません。

```bash
git submodule sync --recursive
git submodule update --init --recursive
./scripts/atomvm_esp32.exs build --target esp32s3 --component .
```

補助スクリプトの既定値は、移動するブランチ先頭ではなく、AtomVM `release-0.7` の特定コミットです。AtomVM リビジョンを変更する場合は意図的に固定値を更新し、ネイティブと実機の全確認を再実行してください。

二核 Xtensa 対象では、ESP-IDF 5.5 環境でも AtomVM の SMP を有効に保つため、ESP-IDF のソフトウェア実装 `stdatomic` を選びます。部品の `sdkconfig.defaults` はスケジューラースタックを 8 KB へ増やします。確認済みの二核 ESP32-S3 では、ESP-IDF の既定値だとアプリケーション開始前に固定版 AtomVM がスタックを使い切ります。

検証機器に合う対象と設定を使います。ネイティブコンパイラーの警告を確認し、生成設定に想定するパネル、バス、タッチ、PSRAM の値が含まれることを確認してください。

## 実機の検証

例示アプリケーションを書き込む前に、新しく構築したネイティブファームウェアを書き込みます。少なくとも次を実行します。

- `SAMPLE_APP_MODE=all`
- `SAMPLE_APP_MODE=nif`
- `SAMPLE_APP_MODE=moving_icons`

記録する内容:

- 正確な Git コミット
- AtomVM リビジョンと ESP-IDF 版
- 基板、パネル、解像度、配線・設定の組み合わせ
- PSRAM の有無とネイティブスプライト PSRAM 対応の有効・無効
- 動作確認結果と、機能不足による省略
- MovingIcons の物体数、目標 FPS、実測 FPS、見た目の結果
- JPEG 復号器やパネル固有問題などの既知の制約

## 公開前モデルを維持する

すべての確認に成功した後、次を行います。

1. 変更履歴の `未公開` 見出しへ日付を付けず、検証報告を記録する
2. 必要に応じて開発ブランチを送信し、正確なコミットで継続的検証が成功することを確認する
3. ラッパーとネイティブ部品の Git 依存を同じコミットへ固定する
4. 明示的なパッケージ版方針を採用するまで、SemVer タグを作らず Hex へ公開しない

過去の日付タグは引き続きスナップショットを識別できますが、パッケージ公開版ではなく API 版も割り当てません。
