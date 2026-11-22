# ESP-IDF 部品

`native/` の NIF と LovyanGFX を AtomVM ファームウェアへ組み込むための参照文書です。
初回は [Getting Started](getting-started.md) でツールと配線を準備してください。

## 固定する依存版

| 依存 | 版・コミット |
| --- | --- |
| LovyanGFX | 1.3.0、`dd228d15a03900ce658fd62e120dc7cc489915dd` |
| AtomVM | release-0.7、`e2cacc998f455ad66b1aa9e6391f7b32928cc38d` |
| ESP-IDF | EIM の v5.5.x |

LovyanGFX の更新時は C++ のオブジェクト構造も変わるため、古いオブジェクトを混在させず、
`build --fullclean` などで全ソースを再構築してください。
1.3.0 の hub と旧 I2C の生成 overlay は `native/cmake/lovyangfx.cmake` で管理します。
submodule のソースは直接編集しません。

## 構築と書き込み

リポジトリのルートで実行します。`ATOMVM_DIR` には専用の AtomVM checkout を指定してください。
構築補助スクリプトは EIM の ESP-IDF v5.5.x を使い、AtomVM を固定コミットへ合わせます。

```bash
export ATOMVM_DIR="$HOME/atomvm/AtomVM-atomlgfx"
git submodule update --init --recursive
./scripts/atomvm_esp32.exs build --target esp32s3 --component .
```

構築と書き込みをまとめる場合:

```bash
./scripts/atomvm_esp32.exs install --target esp32s3 --component . --port /dev/ttyACM0
```

`--component .` が NIF の組み込み指定です。既存のリンクに頼らず、毎回指定してください。
`just build-firmware` / `just install-firmware` も同じ操作を行います。

## 設定を変える

| 変えるもの | 方法 | 必要な作業 |
| --- | --- | --- |
| パネル、寸法、GPIO などの実行時設定 | `LGFX.init/1` または `AtomLGFX.open/1` → `init/1` | アプリを再構築・書き込み |
| `init/0` が使う既定値、組み込む機能 | CMake の `LGFX_PORT_*` | ファームウェアを再構築・書き込み |
| PSRAM、フラッシュ容量、ESP-IDF の設定 | 追加の SDK defaults ファイル | ファームウェアを再構築・書き込み |

実行時設定の例（GPIO は現在の構築時設定を使用）:

```elixir
:ok = LGFX.init(panel_driver: :ili9488, width: 320, height: 480)
```

構築時の LCD CS を変更する例:

```bash
./scripts/atomvm_esp32.exs install --target esp32s3 --component . \
  --cmake-define LGFX_PORT_LCD_CS_GPIO=10 --port /dev/ttyACM0
```

追加の ESP-IDF 設定は `--sdkconfig /path/to/board.defaults` で渡せます。
PSRAM の具体例は [Getting Started](getting-started.md#psram-を使う場合) を参照してください。
設定名と既定値の定義元は `CMakeLists.txt` です。`LGFX_PORT_*` という名前は残っていますが、実行経路は NIF のみです。

SPI 転送の切り分けには `--cmake-define LGFX_PORT_DISABLE_SPI_SCT=ON` を指定できます。
既定は SCT 有効です。変更後はファームウェアを再構築・書き込みします。

## 再構築が必要な変更

`native/`、`CMakeLists.txt`、LovyanGFX の版や SDK 設定を変更した場合は、ファームウェアを書き直します。
Elixir の描画処理だけを変えた場合は、アプリの `.avm` だけを書き直せます。
ネイティブと Elixir 側の操作契約を変更するときは、両者を同じコミットにそろえてください。

配布用の単一 `.img` が必要な場合:

```bash
./scripts/atomvm_esp32.exs mkimage --target esp32s3 --component . --out-dir ./build/atomvm-images
```

## 内部実装を変更するとき

NIF の登録は `native/nif.c`、描画は `native/device/`、バッチ実行は `native/render_batch.cpp` が担当します。
`native/include/atom_lgfx/ops.def` を変更したら生成物も同期します。

```bash
elixir scripts/sync_lgfx_protocol_doc.exs
elixir scripts/sync_lgfx_protocol_doc.exs --check
```

詳細は [構成](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/architecture.md)、
[操作契約](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/protocol.md)、
[ネイティブ実装](https://github.com/mnishiguchi/atomlgfx/blob/main/native/README.md) を参照してください。
