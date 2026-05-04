<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Hardware performance smoke report - 2026-05-01

## Summary

This report records the first successful on-device performance smoke run for
the AtomLGFX v2 packed binary batch path.

The run establishes a baseline for the current v2 design:

- ordinary protocol calls remain the general-purpose path for correctness and
  API coverage
- packed binary batches substantially reduce per-command overhead for many
  small drawing commands
- `submitBinaryBatch` is working as the synchronous packed scalar drawing fast
  path
- protocol capability reporting now correctly recognizes `CAP_BATCH`

At 120 commands per run, the packed batch path was about `26.6x` faster than
direct `fill_rect` calls and about `31.1x` faster than direct `draw_line`
calls when comparing execution only. Even after including Elixir-side batch
construction, the packed path still came out about `11.8x` to `12.2x` faster.

## Context

The v2 implementation is designed around two execution styles:

- ordinary LovyanGFX-style calls
  - one request
  - one native handler
  - one reply
  - broad API coverage

- packed binary scalar batches
  - one target
  - one RGB565 command stream
  - synchronous execution
  - optimized for many small primitive drawing commands

The binary batch path was recently hardened so the native dispatcher prevalidates
the whole packed stream before starting the write session. This prevents
malformed bytes or unsupported packed opcodes from partially mutating the display.

This report exists to answer a narrow question: does that stricter validation
still leave the packed path meaningfully faster on real hardware? For this run,
the answer is yes.

## Test setup

Observed device configuration:

```text
panel=ILI9488
size=320x480
selected_rotation=1
viewport=480x320
write_hz=20000000
read_hz=10000000
touch_driver=XPT2046
touch_attached=true
```

Sample mode:

```text
SAMPLE_APP_MODE=perf
rounds=120
```

The captured serial output did not include the board name, so this baseline is
currently tied to the observed panel and bus configuration rather than to a
board label. Future runs should record `board=`, `branch=`, and `commit=`
explicitly alongside the perf lines.

The run completed successfully:

```text
perf_smoke done
perf_smoke ok
Return value: ok
```

## Protocol capability note

Before the `ProtocolSmoke` fix, the sample app printed:

```text
protocol smoke note: unknown feature bits present (future caps): 32
```

That value is not an unknown future capability. It is bit 5:

```text
1 << 5 = 32
```

In the v2 protocol, bit 5 is `CAP_BATCH`.

The sample app was updated so `SampleApp.ProtocolSmoke` recognizes `CAP_BATCH`
and treats it as part of the known capability mask. After the fix, the warning
disappeared:

```text
protocol smoke ok
protocol_smoke ok
```

This matters for the perf report because the measured fast path depends on
`submitBinaryBatch`, and `submitBinaryBatch` is gated behind `LGFX_CAP_BATCH`.

## Performance results

Two successful runs were captured back to back. The second table below is the
latest one and is the baseline used for the derived comparisons in this report.

Previous successful run:

| Path | Commands | Bytes | Elapsed us | Per command us | Commands per sec |
| --- | ---: | ---: | ---: | ---: | ---: |
| build_fill_rect_batch | 120 | 1320 | 252957 | 2107 | 474 |
| build_draw_line_batch | 120 | 1320 | 256148 | 2134 | 468 |
| direct_fill_rect | 120 | 0 | 5328655 | 44405 | 22 |
| binary_batch_fill_rect | 120 | 1320 | 199885 | 1665 | 600 |
| direct_draw_line | 120 | 0 | 5151062 | 42925 | 23 |
| binary_batch_draw_line | 120 | 1320 | 165972 | 1383 | 723 |

Latest successful run:

| Path | Commands | Bytes | Elapsed us | Per command us | Commands per sec |
| --- | ---: | ---: | ---: | ---: | ---: |
| build_fill_rect_batch | 120 | 1320 | 253505 | 2112 | 473 |
| build_draw_line_batch | 120 | 1320 | 256656 | 2138 | 467 |
| direct_fill_rect | 120 | 0 | 5330118 | 44417 | 22 |
| binary_batch_fill_rect | 120 | 1320 | 200108 | 1667 | 599 |
| direct_draw_line | 120 | 0 | 5153652 | 42947 | 23 |
| binary_batch_draw_line | 120 | 1320 | 165932 | 1382 | 723 |

## Repeatability

The two successful runs are very stable.

| Path | Previous elapsed us | Latest elapsed us | Difference |
| --- | ---: | ---: | ---: |
| direct_fill_rect | 5328655 | 5330118 | +0.03% |
| binary_batch_fill_rect | 199885 | 200108 | +0.11% |
| direct_draw_line | 5151062 | 5153652 | +0.05% |
| binary_batch_draw_line | 165972 | 165932 | -0.02% |

This is good enough for a hardware smoke benchmark. It is not a full benchmark
suite, but it is stable enough to use as a branch-to-branch and board-to-board
regression check.

## Approximate speedup

Execution-only comparison:

| Operation | Direct elapsed us | Binary batch elapsed us | Approximate speedup |
| --- | ---: | ---: | ---: |
| fill_rect | 5330118 | 200108 | 26.6x |
| draw_line | 5153652 | 165932 | 31.1x |

Including Elixir-side batch construction cost:

| Operation | Direct elapsed us | Build + batch elapsed us | Approximate speedup |
| --- | ---: | ---: | ---: |
| fill_rect | 5330118 | 453613 | 11.8x |
| draw_line | 5153652 | 422588 | 12.2x |

The packed binary path is therefore substantially faster even when including
Elixir-side command construction. The measured build cost is small relative to
the direct-call overhead that the packed path removes.

## Interpretation

The result supports the v2 architecture:

- the ordinary call path remains appropriate for setup, text, images, sprites,
  and less frequent operations
- the packed binary batch path is clearly better for many small scalar drawing
  commands
- keeping the binary batch path intentionally narrow is justified by the observed
  performance gain
- the current strict prevalidation pass does not erase the performance benefit

It is also worth reading the result conservatively. This report covers one
sample mode, one round count, and one observed panel/bus setup. It does not yet
say whether the same ratios hold across other boards or for more fill-heavy
scenes where the panel itself may dominate total time.

## Known tradeoff

The native binary batch dispatcher currently decodes the command stream twice:

- first pass: validate-only preflight
- second pass: execute

This is intentional for hardening. It prevents malformed streams from partially
mutating the display.

If future performance work needs more speed, compare:

- strict preflight mode
- single-pass fail-on-first-error mode

Do not remove preflight only for style reasons. Measure first.

## Raw output excerpt

The tables above were derived from the following successful run:

```text
protocol smoke ok
protocol_smoke ok
perf_smoke start viewport=480x320 rounds=120
PERF label=build_fill_rect_batch commands=120 bytes=1320 elapsed_us=253505 per_command_us=2112 commands_per_sec=473
PERF label=build_draw_line_batch commands=120 bytes=1320 elapsed_us=256656 per_command_us=2138 commands_per_sec=467
PERF label=direct_fill_rect commands=120 bytes=0 elapsed_us=5330118 per_command_us=44417 commands_per_sec=22
PERF label=binary_batch_fill_rect commands=120 bytes=1320 elapsed_us=200108 per_command_us=1667 commands_per_sec=599
PERF label=direct_draw_line commands=120 bytes=0 elapsed_us=5153652 per_command_us=42947 commands_per_sec=23
PERF label=binary_batch_draw_line commands=120 bytes=1320 elapsed_us=165932 per_command_us=1382 commands_per_sec=723
perf_smoke done
perf_smoke ok
Return value: ok
```

## Follow-up

- Run the same smoke on ESP32 DevKit.
- Run the same smoke on XIAO ESP32C3, C5, and C6 if available.
- Record `board=`, `branch=`, and `commit=` in each captured report.
- Add an optional compile-time or runtime benchmark round count.
- Investigate whether Elixir-side batch construction can allocate less.
- Consider a native-only validation test for `lgfx_binary_batch_dispatch_validate`.
- Keep `ProtocolSmoke` capability constants aligned with `protocol.h`.
