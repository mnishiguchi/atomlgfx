<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Hardware smoke and performance validation

This guide records the minimal on-device checks for the AtomLGFX LovyanGFX
AtomVM port.

The goal is not a precise benchmark suite. The goal is to catch obvious
regressions and compare the ordinary call path with the binary batch path on
real hardware.

Collected reports:

- [2026-08-04 v3 hardware validation report](2026-08-04-v3-hardware-validation-report.md)
- [2026-05-01 hardware performance smoke report](2026-05-01-hardware-performance-smoke-report.md)

## What this validates

- The driver boots and initializes the panel.
- Basic direct drawing calls still work.
- `submitBinaryBatch` accepts a valid render script.
- Sprite and palette paths can be checked separately when the board supports
  them.
- Render-batch rendering is measurably different from many ordinary port
  calls.
- The output gives stable enough numbers to compare branches and boards.

## Sample app modes

The current default mode is `face`, the practical low-memory application smoke
target. `moving_icons` uses a separate advanced renderer and remains the main
hot-loop validation path for on-device animation work.

Use `smoke` for the compact ordinary-call and binary-batch regression surface:
boot, binary batch, primitives, text, clip rects, RGB565 image transfer,
best-effort JPEG drawing, color helpers, palette sprites when available, and a
touch probe when available.

Available modes:

- `smoke`
- `protocol`
- `boot`
- `basic_shapes`
- `text`
- `perf`
- `face`
- `japanese_text`
- `moving_icons`
- `sprites`
- `sprite_protocol`
- `touch_calibrate`
- `all`

Use `all` for the default smoke path plus the sprite protocol smoke. Use `perf`
only when collecting timing numbers.

Use `moving_icons` when validating animation on hardware. The current demo config exercises:

- icon sprite upload
- Elixir-owned object updates
- one compact transformed-sprite list per frame
- dynamic dirty-bound clearing without a full-frame sprite
- immediate erase/redraw for isolated icons and safe grouping for intersections

Observed log shape:

```text
moving_icons stats renderer=transformed_sprite_list obj_count=<n> fps=<n> target_fps=<n>
```

On the connected 480x320 ESP32-S3 device, six rotating and zooming icons
sustained 4-5 FPS with a 5 FPS target. Visual comparison confirmed that
overlap-aware immediate redraw substantially reduces flicker and keeps crossing
icons intact. The render-first strip path needed a
480x20 buffer to avoid AtomVM heap exhaustion and measured about 0.2 FPS; the
direct-operation 480x40 strip path was stable but similarly slow.

## Perf smoke mode

`SampleApp.PerfSmoke` emits one-line records:

```text
PERF label=<name> commands=<n> bytes=<n> elapsed_us=<n> per_command_us=<n> commands_per_sec=<n>
```

Current measurements:

- `build_fill_rect_binary_batch`
  - Elixir-side command binary construction cost
- `build_draw_line_binary_batch`
  - Elixir-side command binary construction cost
- `direct_fill_rect`
  - many ordinary `fill_rect` calls
- `binary_batch_fill_rect`
  - one binary batch containing equivalent `fill_rect` commands
- `direct_draw_line`
  - many ordinary `draw_line` calls
- `binary_batch_draw_line`
  - one binary batch containing equivalent `draw_line` commands

## Run on ESP32

From `examples/elixir`:

```sh
mix clean
mix atomvm.esp32.flash --port /dev/ttyACM0
```

Use the serial port that matches the board, for example:

```sh
SAMPLE_APP_MODE=perf mix clean
SAMPLE_APP_MODE=perf mix atomvm.esp32.flash --port /dev/ttyUSB0
```

The sample app reads `SAMPLE_APP_MODE` at compile time. Re-run `mix clean` when
switching modes.

## Optional local round count

The default is intentionally small enough for repeated smoke testing.

To change the count for ad-hoc experiments, set the process dictionary before
running the perf module manually:

```elixir
:erlang.put(:sample_app_perf_rounds, 300)
SampleApp.start(:perf)
```

For normal firmware flashing, keep the default first. Increase the count only
after the basic smoke run succeeds.

## Suggested board matrix

Record the same output for each board:

| Board | Port | Notes |
| --- | --- | --- |
| XIAO ESP32S3 | `/dev/ttyACM0` or `/dev/ttyUSB0` | current known-good baseline |
| ESP32 DevKit | `/dev/ttyUSB0` | compare against S3 results |
| XIAO ESP32C3 | TBD | lower-cost comparison |
| XIAO ESP32C5 | TBD | Wi-Fi 6 class comparison |
| XIAO ESP32C6 | TBD | RISC-V comparison |

## What to copy into a report

```text
board=
panel=
branch=
commit=
SAMPLE_APP_MODE=perf
PERF label=build_fill_rect_batch ...
PERF label=build_draw_line_batch ...
PERF label=direct_fill_rect ...
PERF label=binary_batch_fill_rect ...
PERF label=direct_draw_line ...
PERF label=binary_batch_draw_line ...
```

## Interpretation

Expected shape:

- direct ordinary calls should be slower when many small primitives are issued
- packed binary batch should reduce per-command overhead
- build cost should be visible, but it should usually be much lower than many
  separate port calls
- if binary batch is not faster, inspect:
  - double decode cost from strict binary preflight
  - SPI/display flush behavior
  - whether the benchmark is dominated by drawing work rather than port-call
    overhead
  - whether the panel or bus configuration differs between boards

## Keep comparisons fair

- Use the same branch and commit.
- Use the same panel configuration.
- Use the same display rotation.
- Run once after a fresh flash, then repeat once more after reset.
- Compare integer output lines, not visual smoothness alone.
