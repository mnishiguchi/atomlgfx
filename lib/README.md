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
- `supports_batch?/1`

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

既知の操作名を数値 opcode に変換して、v2 の低水準 call 要求を送ります。疎通確認や低水準の実験向けです。

```elixir
{:ok, reply} = AtomLGFX.raw_call(port, :ping, 0, 0, [])
```

通常利用では、まず公開 API を使うのが安全です。

より明示的な escape hatch として `AtomLGFX.Raw.call/4` も使えます。

## 明示的バッチ

通常の API は、その場で同期的に実行されます。

明示的バッチは、1 フレーム分の描画を 1 つの render script として送るための仕組みです。重い描画をまとめたい場合だけ使います。

### `AtomLGFX.BinaryBatch`

`AtomLGFX.BinaryBatch` には、render script 向けの命令ビルダーがあります。

v2 MVP では、保守しやすさを優先して BinaryBatch の面を小さく保ちます。通常の描画は scalar 命令を 1 つの batch に並べ、測定済みの高負荷経路だけ compact な一覧命令または native frame 命令として残します。

主なビルダーは次のとおりです。

- 描画先と状態: `target/1`, `color_mode/1`, `begin_strip/1`, `present_strip/0`, `display/0`
- 基本描画: `fill_screen/1`, `clear/1`, `draw_pixel/3`, `draw_line/5`, `draw_rect/5`, `fill_rect/5`, `draw_circle/4`, `fill_circle/4`
- 切り取り: `set_clip_rect/4`, `clear_clip_rect/0`
- 文字: `set_text_font_preset/1`, `set_text_size/1`, `set_text_color/2`, `set_cursor/2`, `draw_string/3`, `print/1`, `println/1`
- スプライト: `set_pivot/2`, `push_sprite/3`, `push_sprite/4`, `push_rotate_zoom/5`, `push_rotate_zoom/6`, `push_rotate_zoom/7`
- 測定済みの高負荷経路: `push_rotate_zoom_list/2`, `push_rotate_zoom_frame_strips/2`

各ビルダーは 1 命令分の binary を返します。複数命令を送る場合は iodata として組み立て、`batch/1` または `render/2` に渡します。

```elixir
frame = [
  AtomLGFX.BinaryBatch.target(0),
  AtomLGFX.BinaryBatch.fill_screen(0x0000),
  AtomLGFX.BinaryBatch.draw_fast_hline(0, 20, 100, 0xFFFF),
  AtomLGFX.BinaryBatch.draw_fast_vline(40, 0, 80, 0x07E0),
  AtomLGFX.BinaryBatch.draw_line(0, 0, 100, 80, 0xF800),
  AtomLGFX.BinaryBatch.display()
]
```

### `submitBinaryBatch`

`submitBinaryBatch` は、アニメーション向けの複数対象 render script 経路です。

次のような命令を 1 つの frame batch として組み立て、1 回の native 呼び出しで実行します。

- `target/1`
- `color_mode/1`
- `clear/1`
- `fill_screen/1`
- 基本図形命令（例: `draw_pixel/3`, `draw_rect/5`, `fill_rect/5`, `draw_line/5`, `draw_circle/4`, `fill_circle/4`）
- 軽い文字命令（例: `set_text_font_preset/1`, `set_text_size/1`, `set_text_color/2`, `draw_string/3`）
- `set_pivot/2`
- `push_sprite/3`
- `push_sprite/4`
- `push_rotate_zoom/5`, `push_rotate_zoom/6`, `push_rotate_zoom/7`
- `push_rotate_zoom_list/2`
- `push_rotate_zoom_frame_strips/2`
- `display/0`

```elixir
frame = [
  AtomLGFX.BinaryBatch.target(1),
  AtomLGFX.BinaryBatch.clear(0x0000),
  AtomLGFX.BinaryBatch.set_clip_rect(0, 0, 160, 120),
  AtomLGFX.BinaryBatch.push_rotate_zoom_list(instances,
    transparent: 0x0000,
    y_offset: 0,
    approx_cull: true
  ),
  AtomLGFX.BinaryBatch.push_sprite(1, 0, 0, 0x0000),
  AtomLGFX.BinaryBatch.target(0),
  AtomLGFX.BinaryBatch.set_text_font_preset(:ascii),
  AtomLGFX.BinaryBatch.set_text_size(1),
  AtomLGFX.BinaryBatch.set_text_color(0xFFFF, 0x0000),
  AtomLGFX.BinaryBatch.draw_string(0, 0, "fps:60"),
  AtomLGFX.BinaryBatch.display()
]

:ok = AtomLGFX.BinaryBatch.render(port, frame)
```

`AtomLGFX.BinaryBatch.render/2` は iodata を受け取ります。呼び出し側は、背景、スプライト、表示反映などの断片を安く連結できます。

生成した描画指示列の確認には、後述の `decode/1`、`summary/1`、`compare/2`、`check_budget/2`、`diagnose/1` を使えます。

`push_rotate_zoom_list/2` は、同じ描画先へ複数の変換スプライトを描くための compact な命令です。MovingIcons のように同種の変換描画が多い場合に使います。

`push_rotate_zoom_frame_strips/2` は、変換スプライト一覧をネイティブ側のストリップループで描画する高負荷アニメーション向け命令です。Elixir 側はオブジェクト状態を持ち、ネイティブ側はストリップの消去、描画、転送をまとめて実行します。通常の描画ではなく、測定済みの熱い描画経路に限定して使います。

`draw_jpg` と `push_image_rgb565` は、MVP の BinaryBatch には含めません。画像のような大きな payload は通常の scalar API で扱う方針です。

render-private opcode は `protocol.h` に集約し、Elixir 側では一覧を `AtomLGFX.BinaryBatch` に集約しています。テストでは両者を照合し、MVP の batch 面が不用意に広がらないようにします。

生成した render script を確認したい場合は、`AtomLGFX.BinaryBatch.decode/1` を使えます。ネイティブ側の `{:batch_failed, index, opcode, reason}` と照合しやすいように、各命令は `index` と `opcode` を含む map として返ります。

```elixir
{:ok, decoded_commands} = AtomLGFX.BinaryBatch.decode(frame)
```

ログや比較用に命令数やバイト効率を見たい場合は、`AtomLGFX.BinaryBatch.summary/1` が使えます。`batch_bytes`、命令数、図形命令数、可変長データ量、固定部分のバイト数、変換スプライト一覧命令の record バイト数、文字命令数、スプライト転送数、`push_rotate_zoom`、`push_rotate_zoom_list`、`push_rotate_zoom_frame_strips` の instance 数などを map で返します。割合は小数ではなく x1000 の整数値として返すため、記録や比較に使いやすくしています。

```elixir
{:ok, summary} = AtomLGFX.BinaryBatch.summary(frame)

IO.inspect(summary.batch_bytes, label: "batch_bytes")
IO.inspect(summary.bytes_per_logical_scalar_x1000, label: "bytes_per_logical_scalar_x1000")
IO.inspect(summary.push_rotate_zoom_instance_count, label: "transform_instances")
```

繰り返しの通常命令と compact な変換スプライト一覧命令を比べたい場合は、`AtomLGFX.BinaryBatch.compare/2` が使えます。第 1 引数を基準、第 2 引数を候補として `summary/1` の値を比較し、差分を `delta` として返します。多くの差分値は「候補 - 基準」ですが、`batch_bytes_savings` は「基準 - 候補」なので、バイト数をどれだけ減らせたかをそのまま確認できます。

```elixir
{:ok, comparison} = AtomLGFX.BinaryBatch.compare(baseline_frame, candidate_frame)

IO.inspect(comparison.delta.batch_bytes_savings, label: "batch_bytes_savings")
IO.inspect(comparison.delta.command_count, label: "command_count_delta")
IO.inspect(comparison.delta.batch_bytes_savings_ratio_x1000, label: "savings_ratio_x1000")
```

生成した描画指示列に上限を設けたい場合は、`AtomLGFX.BinaryBatch.check_budget/2` が使えます。`summary/1` と同じ集計値に対して、バイト数、命令数、可変長データ量、変換スプライト一覧命令の利用数などを確認できます。継続的な検査や生成処理の退行検出に向いています。

```elixir
{:ok, report} =
  AtomLGFX.BinaryBatch.check_budget(frame,
    max_batch_bytes: 4096,
    max_command_count: 64,
    min_packed_list_count: 1
  )

IO.inspect(report.summary.batch_bytes, label: "batch_bytes")
```

不正な render script の場所を調べたい場合は、`AtomLGFX.BinaryBatch.diagnose/1` が使えます。成功時は `summary/1` と同じ集計値に `valid?: true` を加えて返し、失敗時は失敗した命令番号、opcode、推定される命令名、直前まで正常に読み取れた命令、部分的な集計値を返します。生成した frame script の検査や失敗時の記録に向いています。

```elixir
case AtomLGFX.BinaryBatch.diagnose(frame) do
  {:ok, diagnosis} ->
    IO.inspect(diagnosis.batch_bytes, label: "batch_bytes")

  {:error, diagnosis} ->
    IO.inspect(diagnosis.message, label: "binary_batch_error")
    IO.inspect(diagnosis.last_decoded_command, label: "last_decoded_command")
end
```

### v2 MVP 完了条件

v2 MVP は、次の条件を満たした時点で完了とみなせます。

- `mix test` が通る
- `elixir scripts/sync_lgfx_protocol_doc.exs --check` が通る
- 実機で MovingIcons が期待どおり動く
- 1 frame の描画は `BinaryBatch.render/2` で 1 つの binary として送れる
- `ops.def` の batch metadata、生成された Elixir metadata、native の render-private opcode、protocol freeze test が一致している
- `0xF0..0xFF` の render-private opcode 範囲を不用意に広げない
- スプライト生成、削除、照会、タッチ操作、補正、低頻度の制御は通常の API に残す

ここまで満たせていれば、v2 MVP としては十分です。以後は wire format を増やすよりも、Stack-chan などの実例で測定し、`summary/1`、`compare/2`、`check_budget/2` を使って frame script を調整するのが安全です。

### 使い分け

- 設定、初期化、単発の描画なら通常の API を使う
- アニメーションの 1 frame をまとめる場合は `submitBinaryBatch` を使う
- 大きな JPEG、巨大な `push_image`、スプライト作成、タッチ操作は通常の API を使う
- アニメーション中の軽い文字重ね描きは `submitBinaryBatch` の文字命令を使える

明示的バッチは任意機能です。通常の関数呼び出しはそのまま使えます。

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

### 線

- `draw_fast_vline/6`
- `draw_fast_hline/6`
- `draw_line/7`

```elixir
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
