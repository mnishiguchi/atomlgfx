<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# 実機性能簡易検証報告 - 2026-05-01

## 概要

本報告は、AtomLGFX v2 の圧縮バイナリーバッチ経路について、実機上で初めて正常に完了した性能簡易検証を記録するものです。

今回の測定により、現在の v2 設計について次の基準値を得ました。

- 通常のプロトコル呼び出しは、正しさと API 網羅性を重視する汎用経路として維持する
- 多数の小さな描画命令では、圧縮バイナリーバッチによって命令ごとの負荷が大幅に減る
- `submitBinaryBatch` は、同期実行される圧縮済みスカラー描画の高速経路として動作している
- プロトコルの機能報告で `CAP_BATCH` を正しく認識できるようになった

1回あたり120命令の実行時間だけを比較すると、圧縮バッチ経路は `fill_rect` の直接呼び出しより約 `26.6倍`、`draw_line` の直接呼び出しより約 `31.1倍` 高速でした。Elixir 側でのバッチ構築時間を含めても、約 `11.8倍` から `12.2倍` 高速でした。

## 背景

v2 の実装には、次の2種類の実行方式があります。

- 通常の LovyanGFX 形式の呼び出し
  - 1回の要求
  - 1回のネイティブ処理
  - 1回の応答
  - 幅広い API を対象とする

- 圧縮バイナリーによるスカラー描画バッチ
  - 対象は1つ
  - RGB565 の命令列を使用
  - 同期実行
  - 多数の小さな基本図形描画に最適化

バイナリーバッチ経路では、書き込み処理を開始する前に、ネイティブ側の振り分け処理が圧縮命令列全体を事前検証するよう強化しました。これにより、不正なバイト列や未対応の圧縮命令によって、表示内容が途中まで変更されることを防ぎます。

本報告で確認する点は限定的です。厳格な検証を追加した後も、実機上で圧縮経路が十分に高速かどうかを確認します。今回の結果は「はい」でした。

## 検証環境

確認した機器設定は次のとおりです。

```text
panel=ILI9488
size=320x480
selected_rotation=1
viewport=480x320
write_hz=20000000
read_hz=10000000
touch_driver=XPT2046
touch_attached=true
```

実行条件は次のとおりです。

```text
SAMPLE_APP_MODE=perf
rounds=120
```

取得したシリアル出力には基板名が含まれていません。そのため、この基準値は現時点では基板名ではなく、確認した表示器とバスの設定に対応します。今後の測定では、性能行とともに `board=`、`branch=`、`commit=` を明示的に記録します。

実行は正常に完了しました。

```text
perf_smoke done
perf_smoke ok
Return value: ok
```

## プロトコル機能に関する補足

`ProtocolSmoke` の修正前は、見本アプリケーションに次の表示がありました。

```text
protocol smoke note: unknown feature bits present (future caps): 32
```

この値は未知の将来機能ではありません。第5ビットです。

```text
1 << 5 = 32
```

v2 プロトコルでは、第5ビットは `CAP_BATCH` です。

`SampleApp.ProtocolSmoke` が `CAP_BATCH` を認識し、既知の機能マスクへ含めるように修正しました。修正後は警告が消えました。

```text
protocol smoke ok
protocol_smoke ok
```

測定した高速経路は `submitBinaryBatch` に依存し、`submitBinaryBatch` は `LGFX_CAP_BATCH` によって利用可否が決まるため、この修正は性能報告にも関係します。

## 性能結果

正常な実行を続けて2回取得しました。以下の2つ目の表が最新結果であり、本報告の比較基準です。

前回の正常実行:

| 経路 | 命令数 | バイト数 | 経過時間 us | 1命令あたり us | 1秒あたりの命令数 |
| --- | ---: | ---: | ---: | ---: | ---: |
| build_fill_rect_batch | 120 | 1320 | 252957 | 2107 | 474 |
| build_draw_line_batch | 120 | 1320 | 256148 | 2134 | 468 |
| direct_fill_rect | 120 | 0 | 5328655 | 44405 | 22 |
| binary_batch_fill_rect | 120 | 1320 | 199885 | 1665 | 600 |
| direct_draw_line | 120 | 0 | 5151062 | 42925 | 23 |
| binary_batch_draw_line | 120 | 1320 | 165972 | 1383 | 723 |

最新の正常実行:

| 経路 | 命令数 | バイト数 | 経過時間 us | 1命令あたり us | 1秒あたりの命令数 |
| --- | ---: | ---: | ---: | ---: | ---: |
| build_fill_rect_batch | 120 | 1320 | 253505 | 2112 | 473 |
| build_draw_line_batch | 120 | 1320 | 256656 | 2138 | 467 |
| direct_fill_rect | 120 | 0 | 5330118 | 44417 | 22 |
| binary_batch_fill_rect | 120 | 1320 | 200108 | 1667 | 599 |
| direct_draw_line | 120 | 0 | 5153652 | 42947 | 23 |
| binary_batch_draw_line | 120 | 1320 | 165932 | 1382 | 723 |

## 再現性

2回の正常実行は非常に安定しています。

| 経路 | 前回の経過時間 us | 最新の経過時間 us | 差 |
| --- | ---: | ---: | ---: |
| direct_fill_rect | 5328655 | 5330118 | +0.03% |
| binary_batch_fill_rect | 199885 | 200108 | +0.11% |
| direct_draw_line | 5151062 | 5153652 | +0.05% |
| binary_batch_draw_line | 165972 | 165932 | -0.02% |

実機の簡易性能検証としては十分な安定性です。完全な性能試験一式ではありませんが、分岐間や基板間の性能退行を確認する基準として利用できます。

## おおよその高速化率

実行時間だけの比較:

| 操作 | 直接実行 us | バイナリーバッチ us | おおよその高速化率 |
| --- | ---: | ---: | ---: |
| fill_rect | 5330118 | 200108 | 26.6倍 |
| draw_line | 5153652 | 165932 | 31.1倍 |

Elixir 側でのバッチ構築時間を含む比較:

| 操作 | 直接実行 us | 構築 + バッチ実行 us | おおよその高速化率 |
| --- | ---: | ---: | ---: |
| fill_rect | 5330118 | 453613 | 11.8倍 |
| draw_line | 5153652 | 422588 | 12.2倍 |

Elixir 側での命令構築を含めても、圧縮バイナリー経路は大幅に高速です。測定された構築負荷は、圧縮経路によって除かれる直接呼び出しの負荷と比べて小さいことが分かります。

## 考察

結果は v2 の構成を支持しています。

- 通常呼び出しは、初期設定、文字、画像、スプライト、低頻度の操作に適している
- 多数の小さなスカラー描画では、圧縮バイナリーバッチが明確に優れている
- バイナリーバッチの対象を意図的に限定する判断は、確認された性能差から妥当である
- 現在の厳格な事前検証を行っても、性能上の利点は失われない

ただし、結果は控えめに解釈する必要があります。本報告は、1つの実行方式、1つの繰り返し回数、1組の表示器とバス設定だけを対象としています。他の基板でも同じ比率になるか、また表示器自体の処理時間が支配的になりやすい塗りつぶし中心の場面でも同様かは未確認です。

## 既知の兼ね合い

ネイティブ側のバイナリーバッチ振り分け処理は、現在、命令列を2回読み取ります。

- 1回目: 検証のみを行う事前確認
- 2回目: 実行

これは堅牢性を高めるための意図的な設計です。不正な命令列によって表示内容が途中まで変更されることを防ぎます。

将来、さらに速度が必要になった場合は、次を比較します。

- 厳格な事前確認方式
- 最初のエラーで停止する1回読み取り方式

見た目の簡潔さだけを理由に事前確認を削除せず、先に測定します。

## 生の出力

上の表は、次の正常実行から作成しました。

```text
protocol smoke ok
protocol_smoke ok
perf_smoke start viewport=480x320 rounds=120
PERF label=build_fill_rect_batch commands=120 bytes=1320 elapsed_us=253505 per_command_us=2112 commands_per_sec=473
PERF label=build_draw_line_batch commands=120 bytes=1320 elapsed_us=256656 per_command_us=2138 commands_per_sec=467
PERF label=direct_fill_rect commands=120 bytes=0 elapsed_us=5330118 per_command_us=44417 commands_per_sec=22
PERF label=binary_batch_fill_rect commands=120 bytes=1320 elapsed_us=200108 per_command_us=1667 commands_per_sec=599
PERF label=direct_draw_line commands=120 bytes=0 elapsed_us=5153652 per_command_us=42947 commands_per_sec=23
PERF label=binary_batch_draw_line commands=120 bytes=1320 elapsed_us=165932 per_command_us=1382 commands_per_sec=723
perf_smoke done
perf_smoke ok
Return value: ok
```

## 今後の作業

- ESP32 DevKit で同じ簡易検証を実行する
- 利用できる場合は XIAO ESP32C3、C5、C6 でも同じ簡易検証を実行する
- 各報告に `board=`、`branch=`、`commit=` を記録する
- コンパイル時または実行時に指定できる性能測定の繰り返し回数を追加する
- Elixir 側のバッチ構築で割り当てを減らせるか調査する
- `lgfx_binary_batch_dispatch_validate` のネイティブ単体検証を検討する
- `ProtocolSmoke` の機能定数を `protocol.h` と一致させ続ける
