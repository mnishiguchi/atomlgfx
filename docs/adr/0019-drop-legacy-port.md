<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
SPDX-License-Identifier: Apache-2.0
-->

# ADR 0019: AtomVM Port を削除して NIF へ統一する

## 状態

採用

## 背景

ADR 0017 で LovyanGFX を直接呼び出す NIF を追加し、ADR 0018 で旧 AtomVM Port を任意構築にした。
その後、ESP32-S3 実機で NIF-only 構築を繰り返し確認できた。

- 直接 NIF の基本描画、文字、RGB565 画像転送、エラー経路が動作した
- 事前符号化バッチは個別 NIF 呼び出しより約2.67倍高い命令処理速度を示した
- Port を除外するとファームウェア画像が43,680バイト小さくなった

残っていた機能差は、共通 NIF 振り分けと実行時初期化設定により
NIF 側へ移せる。

## 判断

ネイティブ実行経路を AtomVM NIF のみにする。

```text
LGFX / AtomLGFX
       |
       v
AtomLGFX.Native
       |
       v
      NIF
       |
       +-- lgfx_device_*
       |
       +-- render_batch_dispatch
```

次を削除する。

- AtomVM Port の入口
- メールボックス要求／応答処理
- 旧要求組の解析
- Port 用操作登録
- Port 用通常ハンドラー
- Port 専用 term 変換と応答生成
- `LGFX_PORT_ENABLE_LEGACY_PORT` 構築分岐

次は維持する。

- `LGFX` の直接 NIF API
- `AtomLGFX` の高度な API
- `lgfx_device` 装置適合層
- 安定した操作番号と検証情報
- バイナリーバッチ形式と実行系

`ops.def` は transport 固有のハンドラー表ではなく、NIF とバッチが共有する操作情報として扱う。

## 理由

Port と NIF の両方を維持すると、同じ LovyanGFX 機能に対して入口、検証、エラー処理、
構築構成、文書、実機確認が二重になる。

NIF-only が実機で成立し、必要な高度機能も共通 NIF 振り分けから `lgfx_device` へ直接到達できるため、
旧 transport を残す利点より複雑さとフラッシュ使用量の負担が大きい。

## 影響

### 良い影響

- ネイティブ実行経路が1つになる
- Port 用解析・振り分けコードを削除できる
- ファームウェアを小さくできる
- `LGFX` と `AtomLGFX` が同じ装置層を利用する
- 高頻度描画では既存の事前符号化バッチを維持できる

### 悪い影響

- 旧 Port を直接利用する下流コードとは互換性がなくなる
- NIF の同期処理は AtomVM スケジューラーを長時間占有しないよう注意が必要になる
- 既存の高度機能を NIF 経由で実機再確認する必要がある

## 実機確認

削除後は ESP32-S3 で少なくとも次を確認する。

- `SAMPLE_APP_MODE=nif`
- `SAMPLE_APP_MODE=all`
- `SAMPLE_APP_MODE=moving_icons`

特に `all` で、JPEG、スプライト、パレット、タッチ、切り抜き、実行時設定、診断情報が
旧 Port を経由せず動くことを確認する。

## 関連

- ADR 0018: 互換用 AtomVM ポートを構築時に任意化する（置換済み）
- [ADR 0017: AtomVM NIF による LovyanGFX 直接 API](0017-direct-atomvm-nif-api.md)
- [2026-08-21 NIF-only 実機検証報告](../worklog/20260821-nif-only-hardware-validation-report.md)
