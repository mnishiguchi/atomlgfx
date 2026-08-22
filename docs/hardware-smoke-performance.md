<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# 実機の動作確認と性能検証

この文書は、AtomLGFX の LovyanGFX AtomVM NIF に対して実機で行う最小限の確認手順を記録します。

精密な性能測定一式を作ることが目的ではありません。明らかな退行を見つけ、実機上で通常の呼び出し経路とバイナリーバッチ経路を比較することが目的です。

既存の報告:

- [2026-08-21 NIF-only 実機検証報告](worklog/20260821-nif-only-hardware-validation-report.md)
- [2026-08-04 v3 実機検証報告](worklog/20260804-v3-hardware-validation-report.md)
- [2026-05-01 実機性能動作確認報告](worklog/20260501-hardware-performance-smoke-report.md)

## 確認する内容

- ドライバーが起動し、パネルを初期化できる
- 基本的な直接描画が引き続き動く
- `AtomLGFX.submit_binary_batch/2` が正しい描画命令列を受け入れる
- 対応基板では、スプライトとパレット経路を個別に確認できる
- 多数の通常 NIF 呼び出しと描画バッチの性能差を実機で確認できる
- ブランチや基板間で比較できる程度に安定した数値を得られる

## 例示アプリケーションのモード

現在の既定値は `face` です。低メモリーで実用的なアプリケーション動作確認として使います。`moving_icons` は独立した上級者向け描画器を使い、実機アニメーションの高負荷経路を確認する主なモードです。

通常呼び出しとバイナリーバッチの小さな退行確認には `smoke` を使います。起動、バイナリーバッチ、基本図形、文字、クリップ矩形、RGB565 画像転送、可能な範囲での JPEG 描画、色補助、対応時のパレットスプライト、対応時のタッチ確認を実行します。

利用可能なモード:

- `smoke`
- `protocol`
- `boot`
- `basic_shapes`
- `text`
- `nif`
- `perf`
- `face`
- `japanese_text`
- `moving_icons`
- `sprites`
- `sprite_protocol`
- `touch_calibrate`
- `all`

既定の動作確認にスプライトプロトコル確認を加える場合は `all`、計時値を収集する場合だけ `perf` を使います。

直接 NIF API の変更を確認する場合は `nif` を使います。このモードは `LGFX.init/0` から機器を所有し、直接描画、画像転送、バイナリーバッチ、エラー結果の生成を確認します。また、同じ120個の矩形について、バッチの符号化、個別 NIF 呼び出し、事前に符号化したバッチの送信を別々に計測し、`NIF_PERF` 行を出力します。

実機アニメーションの検証には `moving_icons` を使います。現在の例では次を確認します。

- アイコン用スプライトへの画像転送
- Elixir が所有する物体状態の更新
- 1フレームにつき一つの小さな変形スプライト一覧
- 全画面スプライトを使わない動的な変更領域消去
- 単独アイコンの即時消去・再描画と、交差アイコンの安全なグループ化

ログ例:

```text
moving_icons stats renderer=transformed_sprite_list obj_count=<n> fps=<n> target_fps=<n>
```

接続した 480x320 の ESP32-S3 では、回転と拡大縮小を行う6個のアイコンが、目標5 FPSに対して4〜5 FPSを維持しました。目視比較では、重なりを考慮した即時再描画により、ちらつきが大きく減り、交差するアイコンも欠けませんでした。

描画優先の短冊経路は、AtomVM のヒープ枯渇を避けるため 480x20 のバッファを必要とし、約0.2 FPSでした。直接操作の 480x40 短冊経路は安定していましたが、同様に低速でした。

## 性能動作確認モード

`SampleApp.PerfSmoke` は、次の1行形式を出力します。

```text
PERF label=<name> commands=<n> bytes=<n> elapsed_us=<n> per_command_us=<n> commands_per_sec=<n>
```

現在の測定項目:

- `build_fill_rect_binary_batch`
  - Elixir 側で矩形塗りつぶし命令のバイナリーを構築する費用
- `build_draw_line_binary_batch`
  - Elixir 側で線描画命令のバイナリーを構築する費用
- `direct_fill_rect`
  - 多数の通常 `fill_rect` 呼び出し
- `binary_batch_fill_rect`
  - 同等の `fill_rect` 命令を一つのバイナリーバッチへまとめた場合
- `direct_draw_line`
  - 多数の通常 `draw_line` 呼び出し
- `binary_batch_draw_line`
  - 同等の `draw_line` 命令を一つのバイナリーバッチへまとめた場合

## ESP32 での実行

`examples/elixir` から実行します。

```sh
mix clean
mix atomvm.esp32.flash --port /dev/ttyACM0
```

基板に合う直列ポートを指定します。

```sh
SAMPLE_APP_MODE=perf mix clean
SAMPLE_APP_MODE=perf mix atomvm.esp32.flash --port /dev/ttyUSB0
```

例示アプリケーションは構築時に `SAMPLE_APP_MODE` を読み取ります。モードを変更する場合は `mix clean` を再実行してください。

## 任意の繰り返し回数

既定値は、繰り返し動作確認に使いやすい小さな値です。

一時的な実験で回数を変える場合は、性能モジュールを手動実行する前にプロセス辞書へ設定します。

```elixir
:erlang.put(:sample_app_perf_rounds, 300)
SampleApp.start(:perf)
```

通常のファームウェア書き込みでは、まず既定値を使います。基本動作確認に成功してから回数を増やしてください。

## 推奨する基板一覧

各基板で同じ出力を記録します。

| 基板 | ポート | 備考 |
| --- | --- | --- |
| XIAO ESP32S3 | `/dev/ttyACM0` または `/dev/ttyUSB0` | 現在の動作確認済み基準 |
| ESP32 DevKit | `/dev/ttyUSB0` | S3 結果との比較 |
| XIAO ESP32C3 | 未定 | 低価格機との比較 |
| XIAO ESP32C5 | 未定 | Wi-Fi 6 世代との比較 |
| XIAO ESP32C6 | 未定 | RISC-V 機との比較 |

## 報告へ記録する内容

```text
board=
panel=
branch=
commit=
SAMPLE_APP_MODE=perf
PERF label=build_fill_rect_batch ...
PERF label=build_draw_line_batch ...
PERF label=direct_fill_rect ...
PERF label=binary_batch_fill_rect ...
PERF label=direct_draw_line ...
PERF label=binary_batch_draw_line ...
```

## 結果の読み方

期待する傾向:

- 小さな基本図形を多数送る場合、通常の直接呼び出しは遅くなる
- 詰め込んだバイナリーバッチは、命令ごとの負担を減らす
- 構築費用は観測できるが、多数の個別 NIF 呼び出しより通常は十分小さい
- バイナリーバッチが速くない場合は、次を確認する
  - 厳格な事前検証による二重解析
  - SPI または表示更新の動作
  - NIF 呼び出し負担ではなく描画そのものが支配的になっていないか
  - 基板間でパネルやバス設定が異なっていないか

## 公平に比較するための条件

- 同じブランチとコミットを使う
- 同じパネル設定を使う
- 同じ表示回転を使う
- 新しく書き込んだ直後に1回、再起動後にもう1回実行する
- 見た目の滑らかさだけでなく、整数の出力行を比較する
