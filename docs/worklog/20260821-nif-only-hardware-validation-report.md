<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# NIF-only 実機検証報告 - 2026-08-21

## 結果

直接 NIF を独立した翻訳単位へ分離し、互換用ポートを任意構築にした実装を、接続した
ESP32-S3 と ILI9488 表示器で検証した。

`LGFX_PORT_ENABLE_LEGACY_PORT=ON` と `OFF` の完全な AtomVM ファームウェア構築は
どちらも成功した。既定の `ON` 構築では既存のポート動作確認が完了し、`OFF` 構築では
NIF の API、エラー経路、個別描画、事前符号化バッチを3回の起動で確認した。すべて
異常終了なしで完了した。

NIF-only 構築は、同じ設定の互換用ポート付き画像より43,680バイト小さかった。
接続した機器は、最終的に NIF-only ファームウェアと `SAMPLE_APP_MODE=nif` を
書き込んだ状態に戻した。

## リビジョン

| 項目 | リビジョン |
| --- | --- |
| 検証した任意ポート実装 | `0dedd1d4d5dbb83fee5681ca64aa8de1802072e6` |
| 直接 NIF 基準 | `15a8f9a1cb906c78ae5c0c17b20e79489f04ad06` |
| AtomVM | `e2cacc998f455ad66b1aa9e6391f7b32928cc38d` |
| ESP-IDF | v5.5 |
| LovyanGFX | `6f6e9a052fc719ecb2b42640b0207215050d2560` |

実機へ影響しない本報告と参照リンクは、検証後の文書コミットに含める。

## 機器と表示器

確認した機器:

- ESP32-S3 QFN56 revision v0.2
- 240 MHz Xtensa デュアルコア
- `/dev/ttyACM0` 上の USB Serial/JTAG
- 8 MB フラッシュ
- 内蔵 8 MB PSRAM
- 3.5インチ ILI9488、物理解像度 320x480
- 回転 1、横向き表示領域 480x320
- XPT2046 タッチ制御器

## 構築境界

互換用ポート付き構築:

```bash
./scripts/atomvm_esp32.exs build --target esp32s3 --component . \
  --cmake-define LGFX_PORT_ENABLE_LEGACY_PORT=ON
```

NIF-only 構築:

```bash
./scripts/atomvm_esp32.exs build --target esp32s3 --component . \
  --cmake-define LGFX_PORT_ENABLE_LEGACY_PORT=OFF
```

`OFF` の構築グラフには `lgfx_port/nif.c`、共有バイナリーバッチ振り分け、
`lgfx_device/` 装置適合層が含まれた。次のポート専用ソースは含まれなかった。

- `lgfx_port.c`
- `atoms.c`
- `open_config.c`
- `op_registry.c`
- `proto_term.c`
- `handlers.c`

## ファームウェア画像

最小アプリ領域は 1,835,008 バイト（`0x1c0000`）だった。

| 構成 | 画像 | 空き | IDF 表示 |
| --- | ---: | ---: | ---: |
| 互換用ポート付き | 1,788,240 (`0x1b4950`) | 46,768 (`0xb6b0`) | 3% |
| NIF-only | 1,744,560 (`0x1a9eb0`) | 90,448 (`0x16150`) | 5% |
| 差 | -43,680 (`-0xaaa0`) | +43,680 | - |

NIF-only により、空き領域は約1.93倍になった。画像は引き続き最小アプリ領域へ収まるが、
ESP-IDF は両方の構成で残容量が少ないという警告を出した。

## 互換用ポートの実機確認

既定の `ON` 構築へ `SAMPLE_APP_MODE=smoke` を書き込み、次を確認した。

```text
ping ok
protocol_smoke ok
init ok
write_session_smoke ok
binary_batch_smoke ok
primitives_text ok
clip_rects ok
jpeg_paths ok
image_paths ok
colors_palette ok
touch_probe ok
smoke ok
AtomLGFX closed
Return value: ok
```

ポートから指定した 60 MHz の書き込み周波数と、480x320 の横向き表示領域もログで
確認した。NIF を別の翻訳単位へ移しても、既定のポート経路に退行は見られなかった。

## NIF-only の実機確認

`OFF` 構築へ `SAMPLE_APP_MODE=nif` を書き込み、書き込み直後、再起動後、ポート確認後に
NIF-only へ戻した最終起動の3回を確認した。各回は次を出力した。

```text
nif_api_smoke ok
nif_error_path ok
nif_smoke ok viewport=480x320
nif_smoke close=:ok
```

個別の基本図形・文字描画、RGB565 画像転送、バイナリーバッチ、意図したエラー結果、
機器終了が成功した。

## NIF 性能動作確認

各計測は同じ120個の矩形を使用した。

| 起動 | 符号化 | 個別 NIF | 事前符号化バッチ送信 | バッチ速度比 |
| --- | ---: | ---: | ---: | ---: |
| 1 | 1,085,801 us | 412,332 us | 153,907 us | 2.67x |
| 2 | 1,085,797 us | 412,304 us | 153,910 us | 2.67x |
| 最終 | 1,085,801 us | 412,332 us | 153,907 us | 2.67x |

命令処理速度は、個別 NIF が291命令/秒、事前符号化バッチが779命令/秒だった。
3回の値はほぼ同一で、構築境界の変更による不安定さは見られなかった。

2026-05-01 のポート性能報告では、120個の個別 `fill_rect` が約5.33秒、22命令/秒だった。
今回の個別 NIF は約0.412秒、291命令/秒である。これは NIF を主経路とする判断を支持する
大きな差だが、ソフトウェアの節目が異なるため、厳密に制御した同一コミット間の比較ではなく
方向性を示す過去記録との比較として扱う。

## 結論

今回の結果から、構築時の機器設定で足りる用途では NIF-only を実用構成として扱える。
NIF は単純な同期境界を保ち、事前符号化バッチにより反復描画の負荷も下げられる。

一方、ポート経路は実行時設定、タッチ、スプライト、パレット、JPEG などの機能差を埋める
移行経路として正常に動作した。したがって、現時点ではポートソースを削除せず、既定 `ON` の
任意構築にする。NIF の機能範囲と下流利用の移行を確認した後、完全削除を改めて判断する。
