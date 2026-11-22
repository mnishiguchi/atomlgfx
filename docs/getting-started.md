# Getting Started

ESP32-S3 + ILI9488 で、カスタム AtomVM ファームウェアを構築し、Elixir アプリを動かします。
**ファームウェア → アプリの順に、別々に書き込みます。**
`mix deps.get` だけでは LCD を動かす NIF は入りません。

LovyanGFX 1.3.0、AtomVM release-0.7 の固定コミット、ESP-IDF v5.5.x を使います。
依存版の詳細と更新時の再構築方法は [ESP-IDF 部品](esp-idf-component.md) を参照してください。

## 1. 用意するもの

- ESP32-S3、ILI9488 SPI LCD、USB データケーブル
- Linux または macOS の開発 PC（以下は Linux の `/dev/ttyACM0` を使用）
- Git、C/C++ コンパイラー、CMake、Make、gperf、zlib と Mbed TLS の開発ライブラリ
- Erlang/OTP と Elixir。このリポジトリの `.tool-versions` は OTP 28 / Elixir 1.19.5 を指定
- [ESP-IDF Installation Manager（EIM）](https://docs.espressif.com/projects/idf-im-ui/en/latest/) の `eim`
- アプリ書き込み用の `esptool.py` と、ログ確認用のシリアルモニター（以下は `tio`）

ホスト側の AtomVM も構築するため、ESP-IDF 以外の開発ライブラリが必要です。
詳細は [AtomVM の構築要件](https://github.com/atomvm/AtomVM/blob/release-0.7/doc/src/build-instructions.md#generic-unix-build-requirements) を参照してください。
この手順で使う ExAtomVM は `examples/elixir/mix.exs` に依存登録済みです。

## 2. ソースとツールを準備する

```bash
git clone --recursive https://github.com/mnishiguchi/atomlgfx.git
cd atomlgfx
mix local.hex --force
mix local.rebar --force
eim install -i v5.5
eim select v5.5
```

clone 済みの場合は、代わりにルートで `git submodule update --init --recursive` を実行してください。
LovyanGFX は `third_party/LovyanGFX/` に入ります。

既存の AtomVM 開発ツリーを変更しないよう、専用ディレクトリを指定します。
**以下は同じシェルで続けてください。**

```bash
export ATOMVM_DIR="$HOME/atomvm/AtomVM-atomlgfx"
./scripts/atomvm_esp32.exs info --target esp32s3 --component .
```

`info` で ESP-IDF v5.5.x が選ばれていることを確認します。
AtomVM が `missing` でも、次の手順で固定コミットを取得するので問題ありません。
スクリプトは EIM 経由で `idf.py` を実行します。`export.sh` の手動読み込みは不要です。

## 3. LCD を接続する

電源を切って配線します。番号は ESP32-S3 の GPIO です。
電源とバックライトは LCD 基板の仕様に合わせ、GND を共通にしてください。

| 信号 | GPIO |
| --- | --- |
| SPI2 SCLK | 7 |
| MOSI | 9 |
| MISO | 8 |
| LCD CS | 43 |
| LCD DC | 3 |
| LCD RST | 2 |
| XPT2046 タッチ CS | 44 |
| タッチ IRQ | 未接続（`-1`） |

パネル寸法は **320x480**。例示アプリは回転1で **480x320** にします。
別のパネルや配線を使う場合は、下の「別のパネル・配線を使う場合」も確認してください。

USB 接続後にポートを確認します。Linux では `ls /dev/serial/by-id/` が目安になります。
以下の `/dev/ttyACM0` は自分の環境に読み替えてください。

## 4. カスタム AtomVM を構築・書き込む

リポジトリのルートで実行します。既存のファームウェアが上書きされるため、対象ポートを確認してください。

```bash
./scripts/atomvm_esp32.exs install --target esp32s3 --component . --port /dev/ttyACM0
```

**`--component .` が AtomLGFX NIF の組み込み指定です。**
初回は AtomVM の取得、ホストツール、Elixir 用 boot AVM、ESP32-S3 ファームウェアを構築します。
`install complete` が出れば次へ進めます。まだ Elixir の例示アプリは書き込まれていません。

構築だけなら `install` の代わりに `build` を使い、`--port` を省きます。
`component-status --component .` で部品のリンクも確認できます。

## 5. Elixir アプリを書き込む

```bash
cd examples/elixir
mix deps.get
mix clean
SAMPLE_APP_MODE=nif mix atomvm.esp32.flash --port /dev/ttyACM0
```

このコマンドは `sample_app.avm` を作り、アプリ用領域（`0x250000`）へ書き込みます。
ファームウェアを再構築する操作ではありません。
`Hash of data verified` を確認してから、シリアルログを見ます。

```bash
tio -b 115200 /dev/ttyACM0
```

必要ならモニター接続後に基板をリセットしてください。成功時の主なログ:

```text
nif_api_smoke ok
nif_error_path ok
nif_smoke ok viewport=480x320
nif_smoke close=:ok
```

NIF の呼び出しと描画処理の完了を示します。LCD 上でも図形や矩形を確認してください。
`tio` は **Ctrl+T、Q** の順で終了します。次の書き込み前にポートを解放してください。

## 6. 顔のデモを試す

引き続き `examples/elixir` で実行します。

```bash
mix clean
SAMPLE_APP_MODE=face mix atomvm.esp32.flash --port /dev/ttyACM0
tio -b 115200 /dev/ttyACM0
```

`selected rotation=1 viewport=480x320`、`face stats` と顔の描画を確認します。
`SAMPLE_APP_MODE` はコンパイル時に読むので、**モード変更時には `mix clean` が必要**です。
他のモードは [例示アプリ](https://github.com/mnishiguchi/atomlgfx/blob/main/examples/elixir/README.md) を参照してください。

自分のアプリを作る準備ができたら、[Elixir パッケージ](elixir-package.md) へ進んでください。

## just で短く実行する場合

[just](https://github.com/casey/just) は任意です。上記のツール準備・配線後、ルートから使えます。

```bash
just install-firmware
just flash-app nif
just monitor
# モニターを終了してから次のアプリを書き込む
just flash-app face
```

`just flash-app` は依存取得・clean も行います。
ポートを変える例: `ATOMLGFX_PORT=/dev/ttyUSB0 just flash-app nif`。
対象チップは `ATOMLGFX_TARGET` で指定できます。`just --list` で全コマンドを確認できます。
`just setup` は submodule の取得だけで、EIM や Elixir はインストールしません。

## 別のパネル・配線を使う場合

- `face` など: `examples/elixir/lib/sample_app.ex` の `@sample_open_options` を変更し、アプリを再構築します。
- `nif`: `LGFX.init/0` がファームウェアの既定値を使います。`@sample_open_options` は反映されません。
  `nif_smoke.ex` を `LGFX.init/1` に変えるか、構築時設定を変えてファームウェアを書き直します。

パネル種別、寸法、GPIO、SPI host を実機に合わせてください。
詳しくは [ESP-IDF 部品](esp-idf-component.md)、M5Stack は [基板別設定](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/boards/m5stack.md) を参照してください。

## PSRAM を使う場合

既定の構築では PSRAM は無効です。`moving_icons` などを試す場合は、基板に合う SDK 設定を渡します。
以下は検証用の **8 MB flash + 8 MB octal PSRAM** 基板の例です。PSRAM のない基板や quad PSRAM 基板には使わないでください。

ルートに戻って実行します（`examples/elixir` にいる場合は `cd ../..`）。

```bash
cat > /tmp/atomlgfx-esp32s3.defaults <<'EOF'
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
EOF

./scripts/atomvm_esp32.exs install --target esp32s3 --component . \
  --sdkconfig /tmp/atomlgfx-esp32s3.defaults --port /dev/ttyACM0
```

起動ログで PSRAM 検出と `SPI SRAM memory test OK` を確認します。
SDK 設定の変更にはファームウェアの再構築が必要です。

## 困ったとき

| 症状 | 確認すること |
| --- | --- |
| `:nif_not_loaded` | 手順4を `--component .` 付きで実行したか。標準 AtomVM ではなくカスタム版が必要 |
| LovyanGFX のソースがない | ルートで `git submodule update --init --recursive` |
| EIM / ESP-IDF エラー | `eim list` と `eim select v5.5`。構築エラーは最初のエラー行を確認 |
| CMake がホストライブラリを見つけない | 手順1の開発ライブラリと AtomVM の構築要件を確認 |
| 書き込めない | ポート、OS のアクセス権、`esptool.py` が PATH にあるか、モニターを終了したか |
| ログがない | ポートと115200 baudを確認し、モニター接続後にリセット |
| 画面が黒い | 成功ログの有無、LCD 電源・バックライト・パネル種別・GPIO を確認 |
| モード変更が反映されない | `mix clean` 後、同じコマンドに `SAMPLE_APP_MODE` を付けて再書き込み |
| スプライト確保に失敗する | 基板の PSRAM と起動ログを確認。上記 SDK 設定で有効化 |
| `monitor` が TTY エラー | スクリプト経由の EIM monitor は失敗する場合がある。`tio` または `just monitor` を使う |

実機で確認した範囲と制約は [2026-11-22 検証報告](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/worklog/20261122-getting-started-hardware-validation.md) にあります。
