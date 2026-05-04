<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# V2 render-batch performance work log

## Context

This note records MovingIcons performance work that followed [ADR 2026-05-02: Standardize v2 hot rendering on binary render batches](adr/2026-05-02-binary-batch-for-native-like-animation.md).

The ADR decision remains unchanged: v2 hot rendering should use binary render batches so animation frames can be submitted once and executed natively.

The benchmark results below clarify what render batches fixed and what still needs rendering-strategy work.

## Benchmark setup

Observed log shape:

```text
moving_icons stats obj_count=50 render_mode=... submit_mode=binary_batch draw_mode=... fps=... frame_ms=... batch_bytes=... strip_count=...
```

Unless otherwise noted, the logs used:

- `obj_count=50`
- `submit_mode=binary_batch`

## Timeline

| Step | Render mode | Draw mode | Frame time | Batch bytes | Strip count | Notes |
| --- | --- | --- | ---: | ---: | ---: | --- |
| Initial render batch | `strip_buffers` | `push_rotate_zoom_list` | 758-760 ms | 1289 | 2 | One-submit-per-frame worked, but all records were still submitted for each strip. |
| Strip pre-cull | `strip_buffers` | `push_rotate_zoom_list` | 653-683 ms | 845-929 | 2 | Elixir-side strip pre-culling reduced submitted records and improved frame time. |
| Native PRZL fast executor | `strip_buffers` | `push_rotate_zoom_list` | 659-684 ms | 881-941 | 2 | Resolving destination once and caching source lookup did not materially improve runtime. |
| Draw-mode benchmark baseline | `strip_buffers` | `push_rotate_zoom_list` | 642-658 ms | 833-881 | 2 | Baseline after adding explicit draw-mode logging. |
| Whole-sprite list blit | `strip_buffers` | `push_sprite_list` | 529-533 ms | 445-457 | 2 | Faster than dynamic transform, but still not near native-like FPS. |
| Source-region list blit | `strip_buffers` | `push_sprite_region_list` | 576-581 ms | 1067-1081 | 2 | Useful atlas-oriented primitive, but initially slower than whole-sprite list blits. |
| Region contiguous fast path | `strip_buffers` | `push_sprite_region_list` | 565-572 ms | 1025-1067 | 2 | Row-blit overhead was reduced, but it was not the main bottleneck. |
| Whole-source region fast path | `strip_buffers` | `push_sprite_region_list` | 552-563 ms | 955-1011 | 2 | Full-source records now approach whole-sprite list behavior, but strip-buffer cost remains visible. |
| Direct LCD comparison | `direct_lcd` | `push_rotate_zoom_list` | 637 ms | 653 | 1 | Not a useful performance baseline because direct LCD clears the visible display every frame. |
| Direct LCD comparison | `direct_lcd` | `push_sprite_list` | 455 ms | 345 | 1 | Useful only to show that full-screen direct drawing is still not native-like. |
| Direct LCD comparison | `direct_lcd` | `push_sprite_region_list` | 484 ms | 745 | 1 | Useful only as a diagnostic comparison. |
| Public strip-buffer comparison | `strip_buffers` | `push_rotate_zoom_list` | 641 ms | 833 | 2 | Before native presentation strips. |
| Public strip-buffer comparison | `strip_buffers` | `push_sprite_list` | 539 ms | 487 | 2 | Before native presentation strips. |
| Public strip-buffer comparison | `strip_buffers` | `push_sprite_region_list` | 564 ms | 1025 | 2 | Before native presentation strips. |

## Findings

- Render batches removed repeated AtomVM port calls from the frame loop. That remains necessary for v2.
- Submitting fewer transform records helped, so strip pre-culling is useful.
- Optimizing source and destination lookup inside `push_rotate_zoom_list` did not materially help, so repeated lookup/dispatch was not the main bottleneck.
- `push_sprite_list` is faster than `push_rotate_zoom_list`, confirming that dynamic transform cost matters.
- The improvement from `push_sprite_list` is modest, which suggests that strip presentation, sprite blits, memory bandwidth, or display transfer volume is also significant.
- `push_sprite_region_list` is useful for atlas-style rendering, but it needs careful layout and native fast paths to compete with whole-sprite blits.
- `direct_lcd` currently clears the visible display every frame. Treat it as a correctness/debug mode, not as the primary animation-performance baseline.

## Validation polish pass

This pass focused on making the binary-batch wire contract stricter and more observable.

Completed items:

- malformed binary-batch tests for truncated commands
- malformed binary-batch tests for unknown opcodes
- malformed binary-batch tests for bad color mode
- malformed binary-batch tests for invalid sprite list flags
- malformed binary-batch tests for invalid region-list payloads
- malformed binary-batch tests for invalid rotate/zoom-list payloads
- decode/summary tests for render-batch commands
- summary counters for list and strip command categories
- binary fast path for already-built batch binaries

## Native presentation strip pass

The next optimization direction is to remove public frame-buffer sprite blits from `strip_buffers + binary_batch` mode.

Desired frame shape:

```elixir
[
  AtomLGFX.BinaryBatch.begin_strip(y0),
  AtomLGFX.BinaryBatch.target(0),
  AtomLGFX.BinaryBatch.clear(background_color),
  draw_commands,
  overlay_commands,
  AtomLGFX.BinaryBatch.present_strip()
]
```

The important validation point is strip height. Native presentation may allocate a smaller strip height than the preferred value when memory is constrained. The Elixir strip loop must use the native-negotiated strip height, not a hard-coded value.

## Next benchmark matrix

After native presentation strips are wired in, rerun only the useful strip-buffer matrix:

| Render mode | Submit mode | Draw mode |
| --- | --- | --- |
| `strip_buffers` | `binary_batch` | `push_rotate_zoom_list` |
| `strip_buffers` | `binary_batch` | `push_sprite_list` |
| `strip_buffers` | `binary_batch` | `push_sprite_region_list` |

Record:

- `fps`
- `frame_ms`
- `batch_bytes`
- `strip_count`
- native strip height

## Next diagnostic step

If frame time remains high after native strips, add native timing trace around:

- total render-batch dispatch
- preflight validation
- strip clear
- sprite-list draw
- rotate/zoom-list draw
- strip presentation
- `startWrite` / `endWrite` scope

The goal is to identify whether the next bottleneck is command decode, LovyanGFX drawing, memory movement, or LCD transfer.
