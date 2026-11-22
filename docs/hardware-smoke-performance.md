# 実機の動作確認と性能検証

新しいファームウェアや描画処理の退行を、シリアルログと画面で確認する手順です。
初回のツール準備・配線・書き込みは [Getting Started](getting-started.md) を参照してください。

## 実行する

カスタム AtomVM を書き込んだ後、リポジトリのルートで実行します。

```bash
just flash-app nif
just monitor
```

just を使わない場合:

```bash
cd examples/elixir
mix deps.get
mix clean
SAMPLE_APP_MODE=nif mix atomvm.esp32.flash --port /dev/ttyACM0
tio -b 115200 /dev/ttyACM0
```

必要なら接続後にリセットし、`nif_smoke ok viewport=480x320`、`nif_smoke close=:ok` と画面の描画を確認します。
`tio` は Ctrl+T、Q で終了します。ポートを解放してから別モードを書き込んでください。
手動実行時はモード変更ごとに `mix clean` が必要です。

## モードを選ぶ

| モード | 確認すること |
| --- | --- |
| `nif` | 直接 NIF、基本描画、画像転送、不正引数、事前符号化バッチ、120矩形の性能比較 |
| `dma` | 二重の書き込み区間を維持し、38,400 bytes のスプライトを直接・バッチで転送した直後に再描画・色深度変更・破棄・再作成を40回繰り返す |
| `face` | 顔のアニメーション。環境変数を省いた場合の既定モード |
| `all` | `smoke`、基本図形、文字、スプライト、sprite protocol |
| `smoke` | 起動、バッチ、図形・文字、切り抜き、画像、色・パレット、タッチ probe |
| `protocol` | 操作契約と初期化後の概要表示 |
| `boot` | 起動・初期化・回転と寸法 |
| `basic_shapes` / `text` | 図形または文字 |
| `sprites` / `sprite_protocol` | スプライト描画または操作契約 |
| `japanese_text` | 日本語文字描画 |
| `touch_calibrate` | 対応タッチ機器の較正。画面での操作が必要 |
| `moving_icons` | PSRAM スプライトを使うアニメーション |
| `perf` | 通常呼び出しとバッチの詳細な計時 |

最初は `nif`、通常のデモは `face`、広い機能確認は `all` を使います。
`nif` は `LGFX.init/0` の構築時設定、他のモードは主に `SampleApp` の実行時設定を使います。
PSRAM の有効化と別配線での設定は Getting Started にあります。
`dma` は PSRAM 有効構成で実行し、`dma_smoke ok` と正常終了を確認します。
API の完了と継続動作を調べるテストであり、画素の正しさは画面でも別途確認してください。

## ログの読み方

`nif` は符号化・個別 NIF・バッチ送信を別々に計測します。

```text
NIF_PERF label=<name> commands=<n> elapsed_us=<n> per_command_us=<n> commands_per_sec=<n>
nif_perf speedup_x100=<n>
```

`speedup_x100=269` はバッチ送信が個別 NIF の約2.69倍の速度だったことを示します。
符号化時間は送信時間に含まれません。再利用するバッチは描画ループの外で符号化してください。

`perf` は矩形・線ごとの構築費用と描画費用を出します。

```text
PERF label=<name> commands=<n> bytes=<n> elapsed_us=<n> per_command_us=<n> commands_per_sec=<n>
```

`moving_icons` は継続動作の統計を出します。

```text
moving_icons stats renderer=transformed_sprite_list obj_count=<n> fps=<n> target_fps=<n>
```

数値はパネル・SPI 周波数・PSRAM・描画内容で変わります。同じコミット、配線、回転、反復回数で比較し、
書き込み直後と再起動後の両方を確認してください。ちらつきや欠けは画面でも確認します。

## 記録するもの

- atomlgfx / AtomVM / LovyanGFX のコミット、ESP-IDF の版
- 基板、パネル、GPIO、回転後の寸法、SPI 周波数、PSRAM 設定
- ファームウェアサイズとパーティションの空き
- 実行モード、成功・失敗ログ、性能統計、継続確認した時間
- 画面やタッチを実際に確認した範囲と未確認事項

## 検証記録

- [2026-11-22 LovyanGFX 1.3.0 移行検証](worklog/20261122-lovyangfx-1.3.0-validation.md)
- [2026-11-22 導入手順の実機検証](worklog/20261122-getting-started-hardware-validation.md)
- [2026-08-26 公開前検証](worklog/20260826-release-validation-report.md)
- [2026-08-21 NIF-only 実機検証](worklog/20260821-nif-only-hardware-validation-report.md)
- [2026-08-04 v3 検証](worklog/20260804-v3-hardware-validation-report.md)
- [2026-05-01 性能確認](worklog/20260501-hardware-performance-smoke-report.md)

過去の結果は当時の構成での測定です。現在の性能の保証値としては扱いません。
