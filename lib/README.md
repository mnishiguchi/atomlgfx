<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# AtomLGFX

`AtomLGFX` は、AtomVM 上から LovyanGFX 系の表示機能を扱うための Elixir 窓口です。

公開 API は、小さく扱いやすい LovyanGFX 風の形を目指しています。

- 文字倍率は `1`、`2`、`1.5` のような自然な数値で指定できます
- 回転角は度数法で指定できます
- 拡大率は `1.0`、`2.0`、`0.5` のような自然な数値で指定できます

通常の操作は同期的に使えます。重い描画をまとめたい場合だけ、明示的にバッチを使います。

## はじめに

基本的な流れは次のとおりです。

1. ドライバーを開く
2. 疎通を確認する
3. デバイスを初期化する
4. 描画する
5. 表示へ反映する
6. 必要なら終了する

`ili9488` を使う最小例です。

```elixir
{:ok, port} =
  AtomLGFX.open(
    panel_driver: :ili9488,
    width: 320,
    height: 480
  )

:ok = AtomLGFX.ping(port)
:ok = AtomLGFX.init(port)

:ok = AtomLGFX.fill_screen(port, 0x0000)
:ok = AtomLGFX.set_text_font_preset(port, :jp)
:ok = AtomLGFX.set_text_size(port, 2)
:ok = AtomLGFX.set_text_color(port, 0xFFFF, 0x0000)
:ok = AtomLGFX.draw_string(port, 16, 16, "こんにちは")
:ok = AtomLGFX.display(port)

:ok = AtomLGFX.close(port)
```

## 基本事項

### 対象

多くの関数は「対象」を扱います。

- `0`
  - LCD

- `1..254`
  - スプライト番号

対象引数を省略できる関数では、通常 `0`、つまり LCD が使われます。

```elixir
:ok = AtomLGFX.fill_screen(port, 0x0000)
:ok = AtomLGFX.fill_screen(port, 0x0000, 0)
:ok = AtomLGFX.fill_screen(port, 0x0000, 1)
```

公開 API から見た `0` は「LCD への描画先」です。内部でどのように表示準備が行われるかは、利用側が意識しなくてよい設計です。

### 色

#### 表示色

基本図形、文字色、非添字の透過色では RGB565 整数を使います。

```elixir
0x0000
0xFFFF
0xF81F
```

#### 添字色

一部の関数では、添字色も使えます。

```elixir
{:index, 3}
```

添字色は、パレット対応スプライトに対してのみ有効です。

#### パレット色

`set_palette_color/4` では、`0x00RRGGBB` 形式の RGB888 整数を使います。

```elixir
0x112233
0xFF0000
0x00FF00
```

#### スプライト転送時の透過色

スプライト転送では、透過色として次のどちらかを指定できます。

- RGB565 整数
- 添字色

```elixir
0x0000
{:index, 0}
```

添字色による透過指定は、パレット対応の元スプライトに対してのみ有効です。

#### 色補助

`AtomLGFX.Color` には、RGB565 表示色、RGB888 パレット色、添字色、RGB565 画素列を扱うための補助関数があります。

```elixir
AtomLGFX.Color.black()
AtomLGFX.Color.white()
AtomLGFX.Color.red()
AtomLGFX.Color.color565(255, 128, 0)
AtomLGFX.Color.color888(17, 34, 51)
AtomLGFX.Color.index(3)

AtomLGFX.Color.rgb565_le(0xF800)
AtomLGFX.Color.pixels_le([0xF800, 0x07E0, 0x001F])
```

### 返り値

代表的な返り値は次のとおりです。

- `:ok`
- `{:ok, value}`
- `{:error, reason}`

## 開始と終了

### `open/1`

ネイティブドライバーを開きます。

引数には開始時設定のキーワードリストまたはプロパティーリストを渡します。省略した項目には、ドライバーの組み込み既定値が使われます。

```elixir
options = [
  panel_driver: :ili9488,
  width: 320,
  height: 480,
  offset_rotation: 0,
  readable: false,
  invert: false,
  rgb_order: false,
  dlen_16bit: false,
  lcd_spi_host: :spi2_host,
  spi_sclk_gpio: 7,
  spi_mosi_gpio: 9,
  spi_miso_gpio: 8,
  lcd_cs_gpio: 43,
  lcd_dc_gpio: 3,
  lcd_rst_gpio: 2,
  touch_cs_gpio: 44,
  touch_irq_gpio: -1,
  touch_spi_host: :spi2_host,
  touch_spi_freq_hz: 1_000_000,
  lcd_spi_mode: 0,
  lcd_bus_shared: true,
  touch_bus_shared: true
]

{:ok, port} = AtomLGFX.open(options)
```

`open/1` は、そのポート用の開始時設定を Elixir 側に記憶します。

### `normalize_open_config/1`

開始時設定を正規化します。ドライバーは開きません。

```elixir
{:ok, normalized} =
  AtomLGFX.normalize_open_config(
    panel_driver: :ili9488,
    width: 320,
    height: 480
  )
```

### `ping/1`

基本的な疎通を確認します。

```elixir
:ok = AtomLGFX.ping(port)
```

### `init/1`

そのポートに記憶されている開始時設定を使って、ネイティブデバイスを初期化します。

```elixir
:ok = AtomLGFX.init(port)
```

### `display/1`

現在の描画結果を画面へ反映します。

```elixir
:ok = AtomLGFX.display(port)
```

### `close/1`

そのポートが所有するネイティブ側のデバイス状態を終了します。

```elixir
:ok = AtomLGFX.close(port)
```

`close/1` は次の性質を持ちます。

- ネイティブ側のデバイス状態を終了します
- Elixir 側の実行時記憶を消します
- BEAM のポート自体は閉じません
- `open/1` で記憶した開始時設定は残ります

## 照会と補助

### `get_open_config/1`

そのポートに記憶されている開始時設定を返します。

```elixir
{:ok, options} = AtomLGFX.get_open_config(port)
```

### `get_caps/1`

ドライバーが通知する機能情報を返します。

```elixir
{:ok, caps} = AtomLGFX.get_caps(port)
```

### `get_last_error/1`

ドライバー側の直近の失敗情報を返します。

```elixir
{:ok, info} = AtomLGFX.get_last_error(port)
```

### `width/2` と `height/2`

対象の幅と高さを返します。

```elixir
{:ok, w} = AtomLGFX.width(port)
{:ok, h} = AtomLGFX.height(port)

{:ok, sprite_w} = AtomLGFX.width(port, 1)
{:ok, sprite_h} = AtomLGFX.height(port, 1)
```

### 機能確認

- `supports_sprite?/1`
- `supports_pushimage?/1`
- `supports_last_error?/1`
- `supports_touch?/1`
- `supports_palette?/1`

```elixir
{:ok, true} = AtomLGFX.supports_sprite?(port)
```

### `max_binary_bytes/1`

そのドライバー実体が受け付ける最大バイナリー長を返します。

```elixir
{:ok, max_bytes} = AtomLGFX.max_binary_bytes(port)
```

### `format_error/1`

Elixir 側または手続き層の失敗理由を読みやすい文字列に変換します。

```elixir
message = AtomLGFX.format_error({:bad_text_scale, -1})
```

### `raw_call/6`

素の手続き要求タプルを送ります。疎通確認や低水準の実験向けです。

```elixir
{:ok, reply} = AtomLGFX.raw_call(port, :ping, 0, 0, [])
```

通常利用では、まず公開 API を使うのが安全です。

## 明示的バッチ

### `batch/0`

空のバッチ器を返します。

```elixir
batch = AtomLGFX.batch()
```

### `submit_batch/2`

構築済みの明示的バッチを送ります。

```elixir
batch =
  AtomLGFX.batch()
  |> AtomLGFX.Batch.add(AtomLGFX.Batch.Command.new(:fillScreen, 0, 0, [0x0000]))
  |> AtomLGFX.Batch.add(AtomLGFX.Batch.Command.new(:drawLine, 0, 0, [0, 0, 100, 100, 0xFFFF]))

{:ok, _} = AtomLGFX.submit_batch(port, batch)
```

バッチは任意機能です。通常の関数呼び出しはそのまま直接使えます。

## 表示制御

### `set_rotation/2`

LCD の回転を設定します。

受け付け値は `0..7` です。

```elixir
:ok = AtomLGFX.set_rotation(port, 1)
```

### `set_brightness/2`

LCD の明るさを設定します。

```elixir
:ok = AtomLGFX.set_brightness(port, 128)
```

### `set_color_depth/3`

対象の色深度を設定します。

利用できる値は次のとおりです。

- `1`
- `2`
- `4`
- `8`
- `16`
- `24`

```elixir
:ok = AtomLGFX.set_color_depth(port, 16)
:ok = AtomLGFX.set_color_depth(port, 8, 1)
```

### `set_swap_bytes/3`

対象に対して LovyanGFX のバイト入れ替え設定を有効化または無効化します。

```elixir
:ok = AtomLGFX.set_swap_bytes(port, true)
:ok = AtomLGFX.set_swap_bytes(port, false, 1)
```

## クリップ

### `set_clip_rect/6`

対象に切り取り矩形を設定します。

```elixir
:ok = AtomLGFX.set_clip_rect(port, 10, 10, 100, 80)
```

### `clear_clip_rect/2`

設定されている切り取り矩形を解除します。

```elixir
:ok = AtomLGFX.clear_clip_rect(port)
```

LCD とスプライトは、それぞれ独立した切り取り状態を持ちます。

## 基本図形

### 対象全体への描画

- `fill_screen/3`
- `clear/3`

```elixir
:ok = AtomLGFX.fill_screen(port, 0x0000)
:ok = AtomLGFX.clear(port, 0x0000)
```

### 点と線

- `draw_pixel/5`
- `draw_fast_vline/6`
- `draw_fast_hline/6`
- `draw_line/7`

```elixir
:ok = AtomLGFX.draw_pixel(port, 20, 20, 0xFFFF)
:ok = AtomLGFX.draw_line(port, 0, 0, 100, 100, 0x07E0)
```

### 矩形、円、楕円、弧、曲線、三角形

- `draw_rect/7`
- `fill_rect/7`
- `draw_round_rect/8`
- `fill_round_rect/8`
- `draw_circle/6`
- `fill_circle/6`
- `draw_ellipse/7`
- `fill_ellipse/7`
- `draw_arc/9`
- `fill_arc/9`
- `draw_bezier/8`
- `draw_bezier/10`
- `draw_triangle/9`
- `fill_triangle/9`

```elixir
:ok = AtomLGFX.draw_rect(port, 20, 20, 120, 60, 0x07E0)
:ok = AtomLGFX.fill_circle(port, 220, 120, 24, 0xFD20)
:ok = AtomLGFX.draw_ellipse(port, 160, 120, 60, 30, 0xFFFF)
:ok = AtomLGFX.fill_arc(port, 160, 120, 30, 40, 0.0, 180.0, 0xF800)
```

## 文字

### 文字種と倍率

`AtomLGFX` では、文字種と文字倍率を別々に設定します。

使う関数は次のとおりです。

- `set_text_font_preset/3`
- `set_text_size/3`
- `set_text_size_xy/4`

利用できる文字プリセットは次の 2 つです。

- `:ascii`
- `:jp`

```elixir
:ok = AtomLGFX.set_text_font_preset(port, :jp)
:ok = AtomLGFX.set_text_size(port, 2)
```

倍率には `1`、`2`、`1.5` のような自然な値を使えます。

### 文字基準位置と折り返し

- `set_text_datum/3`
- `set_text_wrap/3`
- `set_text_wrap_xy/4`

`set_text_datum/3` は `0..255` を受け付けるそのまま渡しの API です。

`set_text_wrap/3` は LovyanGFX 互換の 1 引数形式です。

- `wrap_x = wrap`
- `wrap_y = false`

```elixir
:ok = AtomLGFX.set_text_wrap(port, true)
:ok = AtomLGFX.set_text_wrap_xy(port, true, true)
```

### 文字色

`set_text_color/4` では、前景色だけ、または前景色と背景色の両方を指定できます。

```elixir
:ok = AtomLGFX.set_text_color(port, 0xFFFF)
:ok = AtomLGFX.set_text_color(port, 0xFFFF, 0x0000)
```

### カーソルと書き込み

- `set_cursor/4`
- `get_cursor/2`
- `print/3`
- `println/3`

```elixir
:ok = AtomLGFX.set_cursor(port, 16, 48)
:ok = AtomLGFX.print(port, "Line 1")
:ok = AtomLGFX.println(port, " Line 2")
```

### 直接描画

- `draw_string/5`
- `draw_string_bg/8`

```elixir
:ok = AtomLGFX.draw_string(port, 16, 16, "日本語テキスト")
```

`draw_string_bg/8` は、必要に応じて文字色と倍率を整えてから文字列を描画する補助関数です。

### `reset_text_state/2`

Elixir 側が持っている文字状態の記憶を消します。

```elixir
:ok = AtomLGFX.reset_text_state(port)
```

## スプライト

### 作成と削除

スプライト番号には `1..254` を使います。

- `create_sprite/4`
- `create_sprite/5`
- `delete_sprite/2`

```elixir
:ok = AtomLGFX.create_sprite(port, 120, 80, 1)
:ok = AtomLGFX.create_sprite(port, 120, 80, 8, 2)
:ok = AtomLGFX.delete_sprite(port, 1)
```

### パレット

- `create_palette/2`
- `set_palette_color/4`

```elixir
:ok = AtomLGFX.create_palette(port, 1)
:ok = AtomLGFX.set_palette_color(port, 1, 0, 0x112233)
```

### 基準点

`set_pivot/4` は、回転や拡大縮小で使う基準点を設定します。

```elixir
:ok = AtomLGFX.set_pivot(port, 1, 60, 40)
```

### スプライト転送

- `push_sprite_to/5`
- `push_sprite_to/6`
- `push_sprite/4`
- `push_sprite/5`

```elixir
:ok = AtomLGFX.push_sprite(port, 1, 40, 30)
:ok = AtomLGFX.push_sprite(port, 1, 40, 30, 0x0000)
:ok = AtomLGFX.push_sprite_to(port, 1, 0, 40, 30)
```

### 回転と拡大縮小

- `push_rotate_zoom_to/7`
- `push_rotate_zoom_to/8`
- `push_rotate_zoom_to/9`

角度は度数法、倍率は自然な数値で指定します。

```elixir
:ok = AtomLGFX.push_rotate_zoom_to(port, 1, 0, 160, 120, 30.0, 1.5)
:ok = AtomLGFX.push_rotate_zoom_to(port, 1, 0, 160, 120, 30.0, 1.2, 1.5)
:ok = AtomLGFX.push_rotate_zoom_to(port, 1, 0, 160, 120, 30.0, 1.2, 1.5, 0x0000)
```

## 画像

### JPEG 描画

JPEG 関連の関数は次のとおりです。

- `draw_jpg/5`
- `draw_jpg/11`
- `draw_jpg_scaled/10`
- `draw_jpg_scaled/11`

```elixir
:ok = AtomLGFX.draw_jpg(port, 0, 0, jpeg_binary)
```

```elixir
:ok = AtomLGFX.draw_jpg(port, 0, 0, 320, 480, 0, 0, 1.0, 1.0, jpeg_binary)
```

```elixir
:ok = AtomLGFX.draw_jpg_scaled(port, 0, 0, 320, 480, 0, 0, 1.5, jpeg_binary)
```

### RGB565 画像転送

`push_image_rgb565/8` は、RGB565 の画素バイナリーを対象へ転送します。

この画素バイナリーは、通常の RGB565 データをリトルエンディアン 16 ビット語として並べたものです。

```elixir
:ok = AtomLGFX.push_image_rgb565(port, 0, 0, width, height, pixels)
```

```elixir
:ok = AtomLGFX.push_image_rgb565(port, 0, 0, width, height, pixels, stride_pixels)
```

必要に応じて、対象側の設定として `set_swap_bytes/3` を使います。

大きい画素バイナリーは、必要に応じて Elixir 側で行単位に分けて送られます。

## タッチ

### タッチ状態の取得

- `get_touch/1`
- `get_touch_raw/1`

どちらも返り値は次のいずれかです。

- `{:ok, :none}`
- `{:ok, {x, y, size}}`

```elixir
case AtomLGFX.get_touch(port) do
  {:ok, :none} ->
    :noop

  {:ok, {x, y, size}} ->
    IO.inspect({x, y, size})

  {:error, reason} ->
    IO.inspect(reason)
end
```

### タッチ補正

- `set_touch_calibrate/2`
- `calibrate_touch/1`

```elixir
:ok = AtomLGFX.set_touch_calibrate(port, {1, 2, 3, 4, 5, 6, 7, 8})
```

`calibrate_touch/1` は対話的な補正を実行し、結果の 8 要素タプルを返します。
