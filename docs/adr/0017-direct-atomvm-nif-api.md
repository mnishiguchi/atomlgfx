<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
SPDX-License-Identifier: Apache-2.0
-->

# ADR 0017: AtomVM NIF による LovyanGFX 直接 API

## 状態

置換済み

> `LGFX` の直接 NIF API は現在も採用していますが、NIF と旧 Port を併存させる移行構成は [ADR 0018](0018-nif-only-native-architecture.md) により置き換えられました。

## 背景

AtomLGFX は現在、AtomVM のポートドライバーを通じて LovyanGFX を利用している。
この構成は実績があり、実行時の機器設定や低メモリーのバイナリーバッチも備えている。

一方、一般的な LovyanGFX の使用例を Elixir へ移しやすくし、単純な描画操作では
AtomVM とネイティブコードの境界をより薄くしたい。

## 判断

LovyanGFX に近い平坦な `LGFX` API と、薄い内部 NIF 層を追加する。

```text
Elixir
  |
  v
LGFX
  |
  v
AtomLGFX.Native NIF
  |
  v
lgfx_device C ABI
  |
  v
LovyanGFX
```

NIF 側で描画処理を再実装しない。既存のポートドライバーと同じ `lgfx_device_*`
関数群を呼び出す。

例えば LovyanGFX の次のコードは、

```cpp
lcd.fillScreen(TFT_BLACK);
lcd.drawRect(20, 20, 100, 60, TFT_RED);
lcd.fillCircle(160, 120, 30, TFT_BLUE);
lcd.drawString("Hello", 20, 100);
```

Elixir では概ね次のように記述できるようにする。

```elixir
LGFX.fill_screen(:black)
LGFX.draw_rect(20, 20, 100, 60, :red)
LGFX.fill_circle(160, 120, 30, :blue)
LGFX.draw_string("Hello", 20, 100)
```

### 一括描画

多数の小さな描画では NIF 呼び出し回数を減らすため `LGFX.batch/2` を使用する。

```elixir
LGFX.batch([
  {:fill_screen, :black},
  {:fill_rect, 10, 10, 100, 40, :red},
  {:draw_string, "Hello", 20, 20}
])
```

一括描画は新しい命令形式を作らず、既存の描画命令符号化と
`lgfx_render_batch_dispatch_run` 実行系を再利用する。

同じ命令列を繰り返す高頻度経路では、`LGFX.encode_batch/2` で1回だけ符号化し、
準備したバイナリーを `LGFX.submit_batch/1` で送信する。`LGFX.batch/2` は
符号化と送信をまとめる1回限りの簡便な経路とする。

### 最初の範囲

最初の NIF API では次を対象とする。

- 初期化と終了
- 画面寸法
- 回転、明るさ、表示制御
- 点、線、四角形、円、楕円、円弧、三角形
- 基本的な文字設定と文字描画
- RGB565 画像転送
- バイナリーバッチ

色名など Elixir で扱いやすい値は `LGFX` で正規化し、NIF 境界では単純な数値と
バイナリーを受け取る。

### 既存ポート API

既存の `AtomLGFX` ポート API は直ちに削除しない。

最初の NIF `init/0` は構築時の機器設定を使用するため、実行時に詳細な機器設定が
必要な用途では引き続き `AtomLGFX.open/1` を使用できる。

NIF とポートは同じ単一機器モデルを共有するため、同時に同じ実機を所有することは
想定しない。

## 影響

### 良い影響

- LovyanGFX のコード例を Elixir へ移しやすくなる
- 単純な描画ではポート通信形式を経由せず直接 `lgfx_device_*` を呼べる
- 一括描画では既存の低メモリー実装を再利用できる
- ネイティブ描画処理の二重実装を避けられる

### 悪い影響

- 当面は NIF API と既存ポート API の2つの入口を保守する
- 最初の NIF `init/0` では実行時の詳細な機器設定を指定できない
- NIF は AtomVM と同じ実行空間で動くため、長時間処理の粒度に注意が必要になる

## 関連 ADR

- [ADR 0016: 描画を中心とする低メモリー API](0016-render-first-low-memory-api.md)
  - 既存ポート経路の描画命令形式とバイナリーバッチを引き続き再利用する
- [ADR 0015: AtomLGFX v3 を低メモリーの LovyanGFX 形式プロトコルとして設計する](0015-v3-low-memory-protocol.md)
  - 既存ポート経路と単一機器モデルの基礎となる
