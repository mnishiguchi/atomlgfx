<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# v3 実機検証報告 - 2026-08-04

## 結果

現在の AtomLGFX v3 は、接続したデュアルコア ESP32-S3 と3.5インチ ILI9488 表示器による実機検証を通過しました。見本アプリケーションを書き込む前に、基盤となる環境からネイティブファームウェアを完全に再構築しました。

標準の `all` 実演は、JPEG の直接描画と拡大縮小描画を含めて `Return value: ok` で完了しました。MovingIcons は、回転・拡大縮小する6個の物体を表示しながら、目標の 5 FPS を維持しました。目視比較では、重なりを考慮した消去により、ちらつきが大幅に減り、アイコンが交差するときも表示を保てることを確認しました。

今回の実行では、現在の API 節目を進めるうえで実機上の阻害要因は残っていません。

## リビジョン

| 項目 | リビジョン |
| --- | --- |
| プロジェクトの状態 | 公開前の v3 作業、通信プロトコル v3 |
| 検証時と同等のソース | `974b88d5621e0e732eb2e202d603824e34d114e3` |
| 中核 API とネイティブ実装 | `91fe8b3137907f9df131f3732d01fb8438bc95d1` |
| 見本アプリケーション | `b3a12f13946d77277c097d7b1023982f215373ff` |
| ファームウェア構築 | `974b88d5621e0e732eb2e202d603824e34d114e3` |
| AtomVM | `e2cacc998f455ad66b1aa9e6391f7b32928cc38d` |
| ESP-IDF | v5.5.4、`735507283d5b2f9fb363a1901172dbd9e847945d` |
| LovyanGFX | `6f6e9a052fc719ecb2b42640b0207215050d2560` |

AtomVM の補助処理では、変動する `release-0.7` 分岐の先頭ではなく、上記の SHA を正確に使用しました。

実機検証後に手元のコミットを整理しました。上記の整理後コミットは検証した実行ソースを維持しており、その後の変更は公開前 v3 の名称と文書だけを修正しています。

## 機器と表示器

確認した機器:

- ESP32-S3 QFN56 revision v0.2
- 240 MHz Xtensa デュアルコア
- `/dev/ttyACM0` 上の USB Serial/JTAG
- 80 MHz で初期化した内蔵 8 MB オクタル PSRAM
- アプリケーション書き込み処理が検出した 8 MB フラッシュ
- 3.5インチ ILI9488、物理解像度 320x480
- 回転 1、横向き表示領域 480x320
- XPT2046 タッチ制御器

表示器設定:

```text
lcd_spi_host=SPI2_HOST
touch_spi_host=SPI2_HOST
sclk=7 mosi=9 miso=8
lcd_cs=43 lcd_dc=3 lcd_rst=2
touch_cs=44 touch_irq=-1
lcd_freq_write_hz=60000000
lcd_dma_channel=SPI_DMA_CH_AUTO
lcd_bus_shared=true touch_bus_shared=true
```

## ネイティブファームウェアの検証

AtomVM / ESP-IDF の完全再構築では、次の順序で既定値を読み込みました。

1. AtomVM の ESP32 既定値
2. AtomLGFX に同梱した `sdkconfig.defaults`
3. 対象機器の PSRAM 検証設定

生成された設定は次の内容を示しました。

```text
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
# CONFIG_FREERTOS_UNICORE is not set
```

8 KB のスケジューラースタックにより、ESP-IDF の既定値 3584 バイトで再現していた pthread のスタックあふれを解消しました。デュアルコア Xtensa 構築では、ESP-IDF のソフトウェア実装 `stdatomic` を選択することで AtomVM の SMP を維持しました。PSRAM の初期化と起動時メモリー検査は成功しました。

AtomLGFX に関するコンパイラー警告なしで、ネイティブ構築が完了しました。

```text
atomvm-esp32.bin binary size 0x1a0390 bytes
smallest app partition 0x1c0000 bytes
0x1fc70 bytes (7%) free
```

両方のアプリケーション検証を行う前に、生成したファームウェアと起動用 AVM の書き込みが正常に完了しました。

## 標準 API の簡易検証

`SAMPLE_APP_MODE=all` により、通常の LovyanGFX 形式 API、描画経路、バイナリーバッチ経路、文字、切り抜き、画像転送、JPEG 復号、色、配色表、タッチ、スプライト、スプライトのプロトコルライフサイクルを実際に動かして確認しました。

以前 JPEG 検証を省略していた原因はネイティブ復号器ではなく、手書きした試験用画像の不備でした。有効な基準用 8x8 JPEG に置き換え、通常描画と拡大縮小描画の両方を必須検証にしました。

取得した結果:

```text
clip_rects ok
jpeg_paths ok
image_paths ok
colors_palette ok
touch_probe ok
smoke ok
basic_shapes ok
text ok
sprites ok
sprite protocol smoke ok
sprite_protocol_smoke ok
AtomLGFX closed
Return value: ok
```

## MovingIcons の簡易検証

最終的な高度描画設定は次のとおりです。

```text
renderer=transformed_sprite_list
erase_mode=overlap_aware
submit_mode=binary_batch
draw_mode=push_rotate_zoom_list
obj_count=6
target_fps=5
frame=480x320
```

実演は 5 FPS に到達し、観察中を通して継続的に 5 FPS を報告しました。再起動、スタックあふれ、割り当てエラー、物体数の減少は確認されませんでした。

接続した表示器での目視結果:

- 以前の消去方式と比べて、ちらつきが明確に減った
- アイコンが交差するときの表示が改善した
- 全画面バッファーや組み込み機器に不向きな過大処理を使わず、重なりを考慮する方式で改善できた

## 開発道具に関する補足

アプリケーションの書き込みは正常に完了しましたが、ExAtomVM は現在、非推奨となった esptool の選択肢表記を使用しています。これらの開発機上の警告は、生成される AVM や AtomLGFX の実行には影響しません。実機検証手順では、別途非推奨となったカンマ区切りの `mix do` 構文も使用していません。

## 結論

今回の実行により、意図した v3 の役割分担を確認できました。

- 一般的な LovyanGFX 操作は、安定した分かりやすい `AtomLGFX` の窓口から利用できる
- 描画補助とバイナリーバッチ補助により、少ないメモリーで現実的な高速化を行える
- MovingIcons は、使いやすい API にアニメーション専用命令を増やすのではなく、高度な見本アプリケーション固有の描画処理として維持する

本報告は、検証済みの開発時点を特定するものです。パッケージ版を割り当てたり、公開済み版であることを示したりするものではありません。
