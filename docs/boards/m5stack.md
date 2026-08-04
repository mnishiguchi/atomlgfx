<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# M5Stack 基板

この文書は、現在 `atomlgfx` に関係する M5Stack 基板を記録します。

`atomlgfx` は制御器を中心に設計しています。LCD とタッチ制御器の共通対応は汎用制御器層へ置き、基板固有の電源、リセット、バックライト処理は `board_preset` で重ねます。

## 共通する制御器群

現在、`ILI9342C` と `FT6336U` の組み合わせに関係する基板は次のとおりです。

- M5Stack Core2
- M5Stack Core2 for AWS
- M5Stack CoreS3

共通するハードウェア:

- LCD: `ILI9342C`
- タッチ: `FT6336U`
- 表示寸法: `320x240`

LCD とタッチ制御器は共通ですが、基板単位の起動処理は同一ではありません。

## 対応する基板プリセット

現在のコードは、次の `board_preset` に対応します。

- `:m5stack_core2`
- `:m5stack_cores3`

これらは汎用制御器経路の上に重ねる設定です。

## Core2 の基準設定

次の開始設定は、Core2 の動作確認済み基準です。

```elixir
[
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

実行時の初期回転:

```elixir
AtomLGFX.set_rotation(port, 1)
```

## CoreS3 の基準設定

次の開始設定は、CoreS3 の初回検証基準です。

```elixir
[
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

実行時の初期回転:

```elixir
AtomLGFX.set_rotation(port, 1)
```

## 注意事項

- Core2 の基板制御には `AXP192` を使う
- CoreS3 の基板制御には `AXP2101` と `AW9523B` を使う
- LCD とタッチ制御器が同じでも、基板起動処理が同じとは限らない
- CoreS3 の LCD リセットやタッチ側の基板制御は、通常の ESP GPIO 配線ではなく基板プリセット層で扱う
- 現在の動作確認済み基準は Core2 であり、CoreS3 は実機確認が完了するまで立ち上げ対象として扱う
