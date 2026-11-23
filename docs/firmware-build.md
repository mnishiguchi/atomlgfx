# カスタム AtomVM ファームウェアはどう作られる？

AtomLGFX は、AtomVM から LovyanGFX を使って LCD に描画するライブラリです。

LovyanGFX は C/C++ で実装されています。そのため、Elixir の依存関係を追加するだけでなく、**LovyanGFX と AtomLGFX の NIF を AtomVM ファームウェアに組み込む**必要があります。

この文書では、`scripts/atomvm_esp32.exs` が行っている処理を説明します。実際のセットアップ手順は [Getting Started](getting-started.md) を参照してください。

## 何を作るのか

```text
Elixir アプリ（.avm）
  AtomLGFX.LGFX
        │ NIF 呼び出し
        ▼
┌── カスタム AtomVM ファームウェア ──┐
│ AtomVM                         │
│ AtomLGFX NIF（C/C++）           │
│ LovyanGFX（C/C++）              │
└────────────────────────────────┘
        │ ESP-IDF の SPI など
        ▼
      LCD / タッチ
```

ファームウェアと Elixir アプリは別々に構築・書き込みします。

- **ファームウェア**：AtomVM と LCD 制御用のネイティブコードを含む。
- **Elixir アプリ**：描画内容や動作を記述した `.avm` ファイル。

Elixir の描画処理だけを変更した場合は、通常、アプリを再書き込みすれば十分です。NIF や LovyanGFX、ESP-IDF の設定を変更した場合は、ファームウェアの再構築が必要です。

## ビルドスクリプトの処理

次のコマンドを例に説明します。

```sh
./scripts/atomvm_esp32.exs install --target esp32s3 --component . --port /dev/ttyACM0
```

`install` は `build` と `flash` を順に実行します。

### 1. AtomVM のソースを用意する

スクリプトは AtomVM の Git リポジトリを取得し、既定では `release-0.7` 系の固定コミット `e2cacc998f455ad66b1aa9e6391f7b32928cc38d` に合わせます。

AtomVM は AtomLGFX と別のディレクトリで構築します。保存先は `ATOMVM_DIR` で指定でき、既定は `~/atomvm/AtomVM` です。
`ensure_atomvm_repo!/1` は未取得なら `release-0.7` を clone し、必要に応じてコミットを取得して detached HEAD に切り替えます。既存 checkout の追跡対象に変更がある場合は、既定では停止します。

### 2. AtomLGFX を ESP-IDF コンポーネントに登録する

`--component .` の `.` は AtomLGFX のリポジトリルートです。

`ensure_component_link!/2` はコンポーネントのディレクトリ名を使い、次の位置にシンボリックリンクを作ります（ディレクトリ名が `atomlgfx` の場合）。

```text
AtomVM/
└── src/platforms/esp32/
    └── components/
        └── atomlgfx -> /path/to/atomlgfx
```

ESP-IDF はこのディレクトリをコンポーネントとして扱います。**AtomLGFX のソースを AtomVM 側へコピーするわけではありません。**

### 3. 起動用 AVM を準備する

Elixir 対応の起動用 AVM（`elixir_esp32boot.avm`）が存在しない場合、スクリプトはホスト PC で CMake を使って AtomVM のホスト側ビルドを実行します。

これは ESP32 に書き込むファームウェアのビルドとは別の段階です。
`build_boot_avm_if_needed!/1` は `AtomVM/build/libs/esp32boot/elixir_esp32boot.avm` を確認し、なければホストビルドで生成します。ESP32 の構築時には `-DATOMVM_ELIXIR_SUPPORT=on` も指定します。この boot AVM は後述のユーザーアプリとは別です。

### 4. AtomLGFX と LovyanGFX をファームウェアに組み込む

スクリプトは EIM（ESP-IDF Installation Manager）経由で `idf.py` を実行します。

```text
eim run → idf.py set-target esp32s3
        → idf.py build
```

`CMakeLists.txt` の `idf_component_register` が AtomLGFX のソースと AtomVM などの依存を登録し、`native/cmake/lovyangfx.cmake` が選んだ LovyanGFX のソースを同じコンポーネントに追加します。`WHOLE_ARCHIVE` により NIF の登録コードもリンク対象に含めます。

| ソース | 役割 |
| --- | --- |
| `native/nif.c`、`native/include/atom_lgfx/nif.inc` | AtomVM に公開する NIF を登録する |
| `native/device/` | LCD、タッチ、スプライトなどの操作を実装する |
| `native/render_batch.cpp` | 複数の描画命令をまとめて実行する |
| `third_party/LovyanGFX/` | LCD・タッチの C/C++ ドライバー |

LovyanGFX のソースは Git submodule として管理しています。取得には `git submodule update --init --recursive` を使います。

パネルや GPIO などの構築時設定は、`native/cmake/lgfx_port_config.h.in` から `lgfx_port_config.h` に生成されます。

この処理では、AtomLGFX の NIF と LovyanGFX の実装が**カスタム AtomVM ファームウェアへリンク**されます。PC 用の `.so` ファイルを後から読み込む方式ではありません。

### 5. ファームウェアを書き込む

ビルド成功後、`idf.py flash` が ESP32 にファームウェアを書き込みます。

`--port /dev/ttyACM0` はシリアルポートの指定です。`build` コマンドだけなら書き込みません。

### 6. Elixir アプリを別に書き込む

ファームウェアができたら、アプリを `.avm` にして書き込みます。

```sh
cd examples/elixir
mix deps.get
SAMPLE_APP_MODE=nif mix atomvm.esp32.flash --port /dev/ttyACM0
```

`examples/elixir/mix.exs` は AtomLGFX と ExAtomVM を依存に指定し、アプリの書き込み先を `0x250000` に設定しています。ExAtomVM の Mix タスクが `sample_app.avm` を構築し、その領域に書き込みます。

実行時は Elixir → AtomVM NIF → LovyanGFX → LCD の順に処理が進みます。通常の AtomVM ファームウェアには AtomLGFX NIF がないため、NIF の代わりに `lib/atom_lgfx/native.ex` の定義が使われ、呼び出しは `:nif_not_loaded` を返します。

## 何が行われるか観察する

```sh
# LovyanGFX の固定コミットを確認
git submodule status third_party/LovyanGFX

# AtomVM、ESP-IDF、ターゲット、ビルド先を表示
./scripts/atomvm_esp32.exs info --target esp32s3 --component .

# AtomVM 側にあるコンポーネントへのリンクを表示
./scripts/atomvm_esp32.exs component-status --component .

# ESP32 に書き込まず、ファームウェアだけを構築
./scripts/atomvm_esp32.exs build --target esp32s3 --component .
```

ビルドログの `+ eim run idf.py ...` が、実際に実行される ESP-IDF のコマンドです。まずログを見てから、`scripts/atomvm_esp32.exs` の `build_cmd/1`、`build_platform_tree!/3`、`run_idf!/3` を読むと、スクリプトの処理を追いやすくなります。

## よく使うオプション

| オプション | 役割 |
| --- | --- |
| `--target esp32s3` | 対象チップを選ぶ |
| `--component .` | AtomLGFX をビルドに組み込む |
| `--port /dev/ttyACM0` | 書き込み先を指定する |
| `--atomvm-dir PATH` | AtomVM のソースとビルド先を指定する |
| `--sdkconfig PATH` | PSRAM などの ESP-IDF 設定を追加する |
| `--cmake-define KEY=VALUE` | パネル・GPIO などの構築時設定を変更する |
| `--fullclean` | ESP-IDF のビルドをクリーンにして再構築する |

設定の詳細は [ESP-IDF 部品](esp-idf-component.md)、NIF の内部構造は [アーキテクチャ](https://github.com/mnishiguchi/atomlgfx/blob/v0.1.0/docs/architecture.md) を参照してください。
