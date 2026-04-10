<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# M5Stack boards

This document records the M5Stack boards currently relevant to `atomlgfx`.

`atomlgfx` is controller-first. Shared LCD and touch controller support lives in the generic controller layer, while board-specific power, reset, and backlight behavior is layered on top through `board_preset`.

## Shared controller family

The following boards currently matter for the shared `ILI9342C` + `FT6336U` controller family:

- M5Stack Core2
- M5Stack Core2 for AWS
- M5Stack CoreS3

Shared hardware characteristics:

- LCD: `ILI9342C`
- touch: `FT6336U`
- display size: `320x240`

These boards share the same LCD and touch controller family, but they do not share identical board-level bring-up behavior.

## Supported board presets

Current board-preset support in the codebase:

- `:m5stack_core2`
- `:m5stack_cores3`

These presets are layered on top of the generic controller path.

## Core2 baseline

The following open config is a known-good baseline for Core2 bring-up:

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

Initial runtime rotation target:

```elixir
AtomLGFX.set_rotation(port, 1)
```

## CoreS3 baseline

The following open config is the first validation baseline for CoreS3 bring-up:

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

Initial runtime rotation target:

```elixir
AtomLGFX.set_rotation(port, 1)
```

## Notes

- Core2 board control is handled through `AXP192`.
- CoreS3 board control is handled through `AXP2101` and `AW9523B`.
- Matching LCD and touch controllers do not imply identical board bring-up.
- For CoreS3, LCD reset and touch-side board control belong in the board-preset layer rather than being modeled as ordinary ESP GPIO wiring.
- Core2 is currently the validated baseline. CoreS3 should be treated as a bring-up target until verified on hardware.
