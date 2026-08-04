<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# v3 hardware validation report - 2026-08-04

## Result

The current AtomLGFX v3 work passed its hardware gates on the connected
dual-core ESP32-S3 and 3.5-inch ILI9488 display. The native firmware was rebuilt
from a clean platform build before the example applications were flashed.

The standard `all` demo completed with `Return value: ok`, including direct and
scaled JPEG drawing. MovingIcons then sustained its 5 FPS target with six
rotating and zooming objects. Visual comparison confirmed that overlap-aware
cleanup substantially reduces flicker and preserves icons while they cross.

No hardware blocker for continuing the current API milestone remains from this
run.

## Revisions

| Item | Revision |
| --- | --- |
| Project state | pre-release v3 work, wire protocol v3 |
| Validation-equivalent source | `974b88d5621e0e732eb2e202d603824e34d114e3` |
| Core API and native commit | `91fe8b3137907f9df131f3732d01fb8438bc95d1` |
| Examples commit | `b3a12f13946d77277c097d7b1023982f215373ff` |
| Firmware-build commit | `974b88d5621e0e732eb2e202d603824e34d114e3` |
| AtomVM | `e2cacc998f455ad66b1aa9e6391f7b32928cc38d` |
| ESP-IDF | v5.5.4, `735507283d5b2f9fb363a1901172dbd9e847945d` |
| LovyanGFX | `6f6e9a052fc719ecb2b42640b0207215050d2560` |

The AtomVM helper used the exact SHA above rather than the moving
`release-0.7` branch head.

The local commits were consolidated after the hardware run. The rewritten
commits above preserve the tested executable sources; the subsequent changes
only corrected pre-release v3 naming and documentation.

## Device and display

Observed hardware:

- ESP32-S3 QFN56 revision v0.2
- dual-core 240 MHz Xtensa target
- USB Serial/JTAG on `/dev/ttyACM0`
- 8 MB embedded octal PSRAM, initialized at 80 MHz
- 8 MB flash detected by the application flasher
- 3.5-inch ILI9488, native 320x480
- rotation 1, landscape viewport 480x320
- XPT2046 touch controller

Display profile:

```text
lcd_spi_host=SPI2_HOST
touch_spi_host=SPI2_HOST
sclk=7 mosi=9 miso=8
lcd_cs=43 lcd_dc=3 lcd_rst=2
touch_cs=44 touch_irq=-1
lcd_freq_write_hz=60000000
lcd_dma_channel=SPI_DMA_CH_AUTO
lcd_bus_shared=true touch_bus_shared=true
```

## Native firmware gate

The clean AtomVM/ESP-IDF build loaded defaults in this order:

1. AtomVM's ESP32 defaults
2. AtomLGFX's bundled `sdkconfig.defaults`
3. the device's PSRAM validation profile

The generated configuration confirmed:

```text
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
# CONFIG_FREERTOS_UNICORE is not set
```

The 8 KB scheduler stack fixed the pthread stack overflow reproduced with
ESP-IDF's 3584-byte default. AtomVM SMP remained enabled by selecting ESP-IDF's
software-backed `stdatomic` implementation for the dual-core Xtensa build.
PSRAM initialization and its boot-time memory test passed.

The native build completed without AtomLGFX compiler warnings:

```text
atomvm-esp32.bin binary size 0x1a0390 bytes
smallest app partition 0x1c0000 bytes
0x1fc70 bytes (7%) free
```

The resulting firmware and boot AVM were flashed successfully before either
application test.

## Standard API smoke

`SAMPLE_APP_MODE=all` positively exercised the ordinary LovyanGFX-style API,
render and binary-batch paths, text, clipping, image upload, JPEG decoding,
colors, palettes, touch, sprites, and sprite protocol lifecycle.

The previous JPEG skip was traced to a malformed hand-written test fixture,
not to the native decoder. It was replaced with a valid baseline 8x8 JPEG, and
both normal and scaled drawing are now mandatory checks.

Captured result:

```text
clip_rects ok
jpeg_paths ok
image_paths ok
colors_palette ok
touch_probe ok
smoke ok
basic_shapes ok
text ok
sprites ok
sprite protocol smoke ok
sprite_protocol_smoke ok
AtomLGFX closed
Return value: ok
```

## MovingIcons smoke

The final advanced-renderer profile was:

```text
renderer=transformed_sprite_list
erase_mode=overlap_aware
submit_mode=binary_batch
draw_mode=push_rotate_zoom_list
obj_count=6
target_fps=5
frame=480x320
```

The demo reached 5 FPS and reported 5 FPS continuously throughout the
observation window. It did not show a reset, stack overflow, allocation error,
or dropped object count.

Visual observations on the attached display:

- flicker was definitely reduced compared with the earlier cleanup strategy
- icons looked better when crossing one another
- the overlap-aware approach achieved the improvement without a full-frame
  buffer or an unrealistic microcontroller workload

## Tooling note

The application flasher completed successfully but ExAtomVM currently invokes
deprecated esptool option spellings. Those host-side warnings do not affect the
generated AVM or the AtomLGFX runtime. The hardware guide avoids the separately
deprecated comma-separated `mix do` syntax.

## Conclusion

This run validates the intended v3 split:

- common LovyanGFX operations remain available through the stable, intuitive
  `AtomLGFX` facade
- render and binary-batch helpers provide realistic low-memory acceleration
- MovingIcons remains an advanced example-local renderer rather than expanding
  the friendly API with animation-specific commands

This report identifies a tested development snapshot. It does not assign a
package version or imply a published release.
