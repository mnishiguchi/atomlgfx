# 対応環境

Hex の `atomlgfx 0.1.0` と、Git タグ `v0.1.0` の NIF を組み込んだ AtomVM を使用してください。
以下は既存の実機検証結果です。すべての基板・機能の動作を保証するものではありません。

## ソフトウェア

| 項目 | 検証した版 |
| --- | --- |
| LovyanGFX | 1.3.0 |
| AtomVM | release-0.7 / `e2cacc998f455ad66b1aa9e6391f7b32928cc38d` |
| ESP-IDF | v5.5 |
| Elixir / OTP | 1.19.5 / 28 |

固定コミットと再構築の方法は [ESP-IDF 部品](esp-idf-component.md) を参照してください。

## 実機

| 基板 | LCD / タッチ | 確認した範囲 |
| --- | --- | --- |
| XIAO ESP32S3（8 MB flash / 8 MB octal PSRAM） | ILI9488 320x480、XPT2046 | NIF、図形、RGB565、JPEG、バッチ、スプライト、PSRAM/DMA、顔デモ、moving_icons |
| M5Stack Core2 | ILI9342C 320x240、FT6336U | 基板初期化、描画、スプライト、タッチ取得、顔デモ |
| M5Stack CoreS3 | ILI9342C 320x240、FT6336U | 基板初期化、描画、スプライト、タッチ取得、顔デモ |

XIAO は回転1で480x320です。M5Stack の確認時は PSRAM を無効にしています。
物理タッチの精度、PSRAM のピーク使用量、長時間動作は未検証です。
ファームウェアの空き領域が少ないため、機能を追加する場合は画像サイズも確認してください。

配線は [Getting Started](getting-started.md)、M5Stack の設定は
[基板別ガイド](https://github.com/mnishiguchi/atomlgfx/blob/v0.1.0/docs/boards/m5stack.md) を参照してください。
構築サイズ・設定・測定結果・制約の詳細は
[実機の確認結果](https://github.com/mnishiguchi/atomlgfx/blob/v0.1.0/docs/validation.md) にあります。
