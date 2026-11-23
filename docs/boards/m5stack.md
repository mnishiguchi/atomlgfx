# M5Stack Core2 / CoreS3

両基板は ILI9342C（320x240）と FT6336U を使います。電源やリセットの処理は基板ごとに異なるため、
対応する `board_preset` と GPIO 設定を使ってください。

先に [Getting Started](../getting-started.md) でツールを準備し、
[ESP-IDF 部品](../esp-idf-component.md) の手順で使用基板向けのファームウェアを構築・書き込みします。

| 基板 | ファームウェアのターゲット | `board_preset` | 基板制御 |
| --- | --- | --- | --- |
| Core2 | `esp32` | `:m5stack_core2` | AXP192 |
| CoreS3 | `esp32s3` | `:m5stack_cores3` | AXP2101 / AW9523B |

以下は実機確認に使った実行時設定です。

## Core2 の基準設定

```elixir
options = [
  board_preset: :m5stack_core2,
  panel_driver: :ili9342c,
  width: 320,
  height: 240,
  offset_rotation: 3,
  invert: true,
  readable: false,
  rgb_order: false,
  dlen_16bit: false,
  lcd_spi_host: :spi2_host,
  spi_sclk_gpio: 18,
  spi_mosi_gpio: 23,
  spi_miso_gpio: 38,
  lcd_cs_gpio: 5,
  lcd_dc_gpio: 15,
  lcd_rst_gpio: -1,
  touch_driver: :ft6336u,
  touch_i2c_port: 0,
  touch_i2c_addr: 0x38,
  touch_sda_gpio: 21,
  touch_scl_gpio: 22,
  touch_irq_gpio: 39,
  lcd_spi_mode: 0,
  lcd_bus_shared: true,
  touch_bus_shared: true
]
```

## CoreS3 の基準設定

```elixir
options = [
  board_preset: :m5stack_cores3,
  panel_driver: :ili9342c,
  width: 320,
  height: 240,
  offset_rotation: 3,
  invert: true,
  readable: false,
  rgb_order: false,
  dlen_16bit: false,
  lcd_spi_host: :spi2_host,
  spi_sclk_gpio: 36,
  spi_mosi_gpio: 37,
  spi_miso_gpio: -1,
  lcd_cs_gpio: 3,
  lcd_dc_gpio: 35,
  lcd_rst_gpio: -1,
  touch_driver: :ft6336u,
  touch_i2c_port: 1,
  touch_i2c_addr: 0x38,
  touch_sda_gpio: 12,
  touch_scl_gpio: 11,
  touch_bus_shared: false
]
```

## 描画する

上で選んだ `options` を使い、文字と図形を表示します。

```elixir
:ok = AtomLGFX.LGFX.init(options)
:ok = AtomLGFX.LGFX.set_rotation(1)
:ok = AtomLGFX.LGFX.fill_screen(:black)
:ok = AtomLGFX.LGFX.set_text_color(:white)
:ok = AtomLGFX.LGFX.draw_string("Hello M5Stack", 16, 16)
:ok = AtomLGFX.LGFX.display()
```

スプライトやタッチを使う場合は、同じ `options` を `AtomLGFX.open/1` へ渡し、
作成したプロセスからハンドルを使います。手順は [Elixir パッケージ](../elixir-package.md) を参照してください。

両基板で初期化・基本描画・小スプライト・顔デモを確認済みです。
PSRAM を無効にした構成での確認であり、タッチ精度や長時間動作は未検証です。
詳細は [実機の確認結果](../validation.md) を参照してください。
