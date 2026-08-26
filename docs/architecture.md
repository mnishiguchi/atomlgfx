<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# 構成

`atomlgfx` は、AtomVM NIF から LovyanGFX を呼び出す単一のネイティブ実行経路を持ちます。

## 全体像

```text
Elixir
  |
  +-- LGFX
  |     LovyanGFX に近い日常向け API
  |
  +-- AtomLGFX
        スプライト、タッチ、JPEG などを含む広い API
        |
        v
  AtomLGFX.Native
        |
        v
     AtomVM NIF
        |
        +-- 直接操作 ------> lgfx_device_*
        |
        +-- バイナリーバッチ -> render_batch
                                  |
                                  v
                              lgfx_device_*
                                  |
                                  v
                              LovyanGFX
```

旧 AtomVM Port、メールボックス通信、要求／応答組の解析層はありません。

## Elixir API

### `LGFX`

通常の LCD 描画では `LGFX` を推奨します。

```elixir
:ok = LGFX.init(panel_driver: :ili9488, width: 320, height: 480)
:ok = LGFX.fill_screen(:black)
:ok = LGFX.draw_rect(20, 20, 100, 60, :red)
```

一般的な操作は薄い専用 NIF を通じて `lgfx_device_*` を直接呼び出します。

### `AtomLGFX`

`AtomLGFX` は、スプライト、パレット、タッチ、JPEG、切り抜きなどの広い機能面を提供します。

```elixir
{:ok, handle} = AtomLGFX.open(panel_driver: :ili9488)
:ok = AtomLGFX.init(handle)
{:ok, touch} = AtomLGFX.get_touch(handle)
```

`open/1` は BEAM Port を開きません。開始時設定と Elixir 側キャッシュを識別する参照を作るだけです。
通常操作は操作番号、対象、フラグ、引数を内部 NIF 呼び出しへ渡し、NIF 側で再検証してから `lgfx_device_*` を呼び出します。

## バイナリーバッチ

多数の小さな描画操作を繰り返す場合は、バイナリーバッチを使用します。

```elixir
{:ok, batch} =
  LGFX.encode_batch([
    {:fill_rect, 10, 10, 100, 40, :red},
    {:draw_string, "Hello", 20, 20}
  ])

:ok = LGFX.submit_batch(batch)
```

命令列は Elixir 側で一度符号化し、NIF 側の `render_batch` が同じ `lgfx_device_*` 装置境界へ振り分けます。
描画処理そのものを別実装にしません。

## 操作情報

操作番号と検証情報の定義元は次です。

```text
native/include/atom_lgfx/ops.def
```

ここには次だけを保持します。

- 安定した操作番号順
- 引数個数
- 使用可能なフラグ
- 対象規則
- 初期化状態規則
- 機能ビット
- バッチ実行情報

旧 Port ハンドラー名や要求組の情報は持ちません。

## ネイティブ層

### NIF 境界

`native/nif.c` と内部の NIF 実装は次を担当します。

- AtomVM 項の変換
- 引数、対象、フラグ、初期化状態の検証
- NIF 戻り値の作成
- `lgfx_device_*` またはバイナリーバッチ実行器の呼び出し

LovyanGFX の描画ロジックは持ちません。

### LovyanGFX 装置層

`native/device/` は LovyanGFX に面する C ABI / C++ 適合層です。

- LCD とスプライトの対象解決
- 機器初期化と実行時設定
- 基本図形と文字
- JPEG と生画像
- スプライトとパレット
- タッチ
- 切り抜き

AtomVM 項を解析しません。

## 所有権

ネイティブ表示機器は単一実体です。

- `LGFX.init/0` は構築時の既定設定を使用する
- `LGFX.init/1` は実行時設定を上書きする
- `AtomLGFX.open/1` は Elixir 側の設定ハンドルを作る
- 設定ハンドルと実行時キャッシュは作成したプロセスが所有する
- `AtomLGFX.init/1` はその設定で同じ NIF 所有機器を初期化する
- 不明なハンドルによる `AtomLGFX.init/1` は `{:error, :invalid_handle}` を返す
- `close` はネイティブ機器とスプライトを終了する

複数の独立した LCD 実体を仮想化しません。

## バイナリーデータ

文字列、JPEG、RGB565 画像、バッチ命令列は NIF 呼び出し中だけ AtomVM のバイナリーを借用します。
ネイティブ側は呼び出し終了後にそのポインターを保持しません。

## 構築時設定

構築時の既定値は CMake 保存変数から生成設定ヘッダーへ反映します。

```text
native/cmake/lgfx_port_config.h.in
  -> <build>/generated/atom_lgfx/lgfx_port_config.h
```

実行時に変更可能な値は `LGFX.init/1` または `AtomLGFX.open/1` + `AtomLGFX.init/1` で上書きできます。

## 次に読む文書

- [ESP-IDF 部品](esp-idf-component.md)
- [Elixir パッケージ](elixir-package.md)
- [内部操作契約](protocol.md)
- [内部操作参照](protocol-reference.md)
- [ADR 0018](adr/0018-nif-only-native-architecture.md)
