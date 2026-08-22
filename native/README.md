<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ネイティブ実装

このディレクトリーには AtomVM NIF と LovyanGFX 装置層からなるネイティブ実装を置きます。

## 主な構成

- `nif.c`
  - AtomVM NIF 集合の入口
- `include/atom_lgfx/nif.inc`
  - よく使う直接 NIF と項変換
- `include/atom_lgfx/nif_dispatch.inc`
  - `AtomLGFX.Native.call/4` の情報駆動振り分け
- `open_config.c`
  - 実行時機器設定の解析
- `render_batch.cpp`
  - 事前符号化した描画命令列の実行
- `include/atom_lgfx/ops.def`
  - 安定した操作番号と検証情報の定義元
- `include/atom_lgfx/constants.h`
  - 機能ビット、操作フラグ、バッチ定数
- `device/`
  - LovyanGFX に面する装置適合層

## 境界規則

- NIF 層では AtomVM 項の変換と検証を行う
- 描画処理は `lgfx_device_*` に任せる
- 同じ描画処理を直接 NIF とバッチで二重実装しない
- NIF 呼び出し終了後に AtomVM バイナリーへのポインターを保持しない
- 操作番号を変更する場合は `ops.def` を定義元とする

## 変更後の確認

- `mix format --check-formatted`
- `mix compile --warnings-as-errors`
- `mix test`
- `mix docs --warnings-as-errors`
- `elixir scripts/sync_lgfx_protocol_doc.exs --check`

ネイティブ変更では、AtomVM / ESP-IDF の完全構築と実機確認も行います。
