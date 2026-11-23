# Elixir 例示アプリ

AtomLGFX API の動作確認用 Mix プロジェクトです。`{:atomlgfx, path: "../.."}` でローカルライブラリを使います。
**先に [Getting Started](../../docs/getting-started.md) で NIF を組み込んだ AtomVM ファームウェアを書き込んでください。**

## アプリを書き込む

このディレクトリで実行します（リポジトリのルートにいる場合は `cd examples/elixir`）。

```bash
mix deps.get
mix clean
SAMPLE_APP_MODE=nif mix atomvm.esp32.flash --port /dev/ttyACM0
```

リポジトリのルートからなら、代わりに `just flash-app nif` を使えます。
ポートを変える場合は `ATOMLGFX_PORT=/dev/ttyUSB0 just flash-app nif` とします。

`SAMPLE_APP_MODE` はコンパイル時に読み込まれます。モード変更時は必ず `mix clean` してください。
`just flash-app` は依存取得と clean も実行します。

| モード | 用途 |
| --- | --- |
| `nif` | 最初の確認。`AtomLGFX.LGFX` による基本描画、エラー経路、バッチ |
| `face` | 顔のデモ（環境変数を指定しない場合の既定値） |
| `all` | 描画・画像・スプライトなどをまとめて確認 |
| `dma` | PSRAM 転送後の再描画・破棄・再作成を、書き込み区間を閉じずに確認 |
| `moving_icons` | Elixir でオブジェクトを更新する6個のアイコンのデモ（5 FPS 上限） |
| `moving_icons_native` | C++ / FreeRTOS で50個のアイコンを時間制限なく描画するデモ |
| `boot` / `protocol` | 起動・回転・寸法、操作契約の概要 |
| `smoke` | 図形・文字・画像・色・タッチなどの確認 |
| `basic_shapes` / `text` / `japanese_text` | 図形、文字、日本語の表示 |
| `sprites` / `sprite_protocol` | スプライトの描画・操作契約 |
| `touch_calibrate` | タッチ較正（画面での操作が必要） |
| `perf` | 通常呼び出しとバッチの計時 |

期待するログと検査項目は [検証手順](../../docs/testing.md) にあります。
書き込み後は `tio -b 115200 /dev/ttyACM0`（ルートからは `just monitor`）でログを確認します。
モニターを終了してから、次の書き込みを行ってください。

## 配線と設定

既定は ESP32-S3 + ILI9488、回転1で480x320です。
`lib/sample_app.ex` の `@sample_open_options` が `face` などの実行時 GPIO 設定を持ちます。
`nif` は `AtomLGFX.LGFX.init/0` を呼び、ファームウェアの構築時設定を使うため、この変数の変更は反映されません。
変更方法と PSRAM の有効化は [Getting Started](../../docs/getting-started.md)、
M5Stack の例は [基板別設定](../../docs/boards/m5stack.md) を参照してください。

描画内容やモードの変更はアプリの再書き込みだけで試せます。
`native/`、`CMakeLists.txt`、LovyanGFX、SDK 設定の変更には、ファームウェアの再構築・書き込みも必要です。
