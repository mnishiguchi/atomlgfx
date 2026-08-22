<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
SPDX-License-Identifier: Apache-2.0
-->

# ADR 0018: AtomVM Port を削除し、ネイティブ実行を NIF に統一する

## 状態

採用

## 背景

ADR 0017 で、LovyanGFX を直接呼び出す AtomVM NIF API を追加した。

ESP32-S3 実機で確認した結果、NIF をネイティブ実行の主経路として利用できることを確認した。

- 基本描画、文字、RGB565 画像転送、エラー経路が動作する
- 事前符号化バッチは、120命令で個別 NIF 呼び出しの約2.68倍の命令処理速度を示した
- 旧 AtomVM Port を除外すると、ファームウェア画像が43,680バイト小さくなった
- スプライト、パレット、JPEG、タッチ、切り抜き、実行時設定なども NIF 経由へ移行できた

Port と NIF の両方を維持すると、同じ LovyanGFX 機能に対して入口、検証、エラー処理、構築構成、文書、実機確認が重複する。

また、旧実装に由来する次のディレクトリー名が残っていた。

    lgfx_port/
    lgfx_device/

`lgfx_port/` はすでに AtomVM Port を表さなくなっており、名称と責務が一致していなかった。

## 判断

ネイティブ実行経路を AtomVM NIF のみに統一する。

    LGFX / AtomLGFX
           |
           v
    AtomLGFX.Native
           |
           v
          NIF
           |
           +-- 直接 NIF 呼び出し
           |
           +-- バイナリーバッチ
                    |
                    v
             LovyanGFX 装置層
                    |
                    v
                LovyanGFX

次を削除する。

- AtomVM Port の入口
- メールボックス要求／応答処理
- 旧要求組の解析
- Port 用操作登録
- Port 用通常ハンドラー
- Port 専用の項変換と応答生成
- `LGFX_PORT_ENABLE_LEGACY_PORT` 構築分岐

次は維持する。

- `LGFX` の直接 NIF API
- `AtomLGFX` の高度な API
- 安定した操作番号
- バイナリーバッチ形式
- スプライト、パレット、JPEG、タッチ、切り抜き
- 実行時機器設定
- 診断情報

`ops.def` は transport 固有のハンドラー表ではなく、NIF とバイナリーバッチが共有する操作情報として扱う。

## ネイティブ実装の構成

C/C++ 実装を `native/` 配下へ集約する。

    native/
      nif.c
      open_config.c
      render_batch.cpp

      cmake/
        lgfx_port_config.h.in

      include/
        atom_lgfx/
          constants.h
          nif.inc
          nif_dispatch.inc
          open_config.h
          ops.def
          ops.h
          render_batch.h

      device/
        device.h
        device_internal.hpp
        state_runtime.hpp
        state.cpp
        state_runtime.cpp
        control.cpp
        primitives.cpp
        text.cpp
        images.cpp
        clip.cpp
        sprites.cpp
        fonts/
          generated/

内部の include namespace は、旧 `lgfx_port/...` ではなくライブラリー名を表す `atom_lgfx/...` とする。

LovyanGFX に面する装置処理は `native/device/` に置き、AtomVM の項変換や NIF 応答生成とは分離する。

## 構築設定名

この変更では、ファイル配置と実行方式の整理を優先する。

次の歴史的な構築設定名は当面維持する。

- `LGFX_PORT_*`
- `lgfx_port_*` CMake 関数
- `lgfx_port_config.h`

これらの名称変更は、必要に応じて別の変更として扱う。

## 理由

NIF-only 構成では、`LGFX` と `AtomLGFX` が同じネイティブ実装と LovyanGFX 装置層を利用できる。

これにより、旧 Port transport を維持するための解析、振り分け、エラー処理、構築分岐が不要になる。

高頻度描画では、個別 NIF 呼び出しとは別に事前符号化したバイナリーバッチを利用できるため、一般的な LovyanGFX API を保ちながら描画経路を高速化できる。

また、`native/` を C/C++ 実装の境界とすることで、過去の transport ではなく現在の責務をディレクトリー構成に反映できる。

## 影響

### 良い影響

- ネイティブ実行経路が1つになる
- Port 用解析・振り分けコードを削除できる
- ファームウェアを小さくできる
- `LGFX` と `AtomLGFX` が同じ装置層を利用する
- バイナリーバッチによる高速描画を維持できる
- `native/` が C/C++ 実装の明確な境界になる
- トップレベルのディレクトリー構成を単純化できる

### 悪い影響

- 旧 AtomVM Port を直接利用する下流コードとは互換性がなくなる
- NIF の同期処理は AtomVM スケジューラーを長時間占有しないよう注意が必要になる
- 下流で従来のネイティブソースパスを直接参照している場合は更新が必要になる
- `LGFX_PORT_*` など一部の歴史的な名称は残る

## 採用しなかった代替案

### Port と NIF を常に構築する

互換性は保ちやすいが、不要なコード、フラッシュ使用量、検証経路が残るため採用しない。

### Port を任意構築にして移行期間を長期化する

移行途中では有効だったが、必要な機能を NIF 側へ移行し実機確認できたため、最終構成としては採用しない。

### NIF とバイナリーバッチで描画処理を別々に実装する

処理の重複と動作差が生じるため採用しない。両方から同じ LovyanGFX 装置層を利用する。

## 確認

ESP32-S3 + ILI9488 + XPT2046 で確認した。

- `SAMPLE_APP_MODE=nif`
  - 直接 NIF、エラー経路、バイナリーバッチ、終了処理
- `SAMPLE_APP_MODE=all`
  - smoke、basic_shapes、text、sprites、sprite_protocol_smoke
- `SAMPLE_APP_MODE=moving_icons`
  - 目標 5 FPS を達成・維持
- `mix test`
  - 102 tests, 0 failures
- AtomVM / ESP-IDF の ESP32-S3 完全構築
  - `native/` への移動後も成功

## 関連

- [ADR 0017: AtomVM NIF による LovyanGFX 直接 API](0017-direct-atomvm-nif-api.md)
- [2026-08-21 NIF-only 実機検証報告](../worklog/20260821-nif-only-hardware-validation-report.md)
- `docs/20260822-atomvm-nif-oom-worklog.md`
