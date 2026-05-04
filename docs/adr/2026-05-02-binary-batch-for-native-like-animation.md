<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 2026-05-02: Standardize v2 hot rendering on binary render batches

## Status

Superseded

Superseded by [ADR 2026-05-03: Treat BinaryBatch as the standard render transaction API](2026-05-03-binary-batch-as-render-transaction-api.md).

This ADR captured the native-like animation and presentation-strip direction. The active render-batch rule now lives in ADR 2026-05-03.

## Context

`atomlgfx` v2 replaces the v1-style broad native wrapper surface with a smaller call-based protocol around LovyanGFX operations. That improves maintainability, but animation workloads still expose a separate performance problem: many small drawing calls crossing the AtomVM/native boundary are too expensive for native-like animation.

The representative workloads are:

- MovingIcons-style animation with many sprite blits or transformed sprite blits per frame
- Stack-chan-style face rendering with sprite-backed drawing and palette-index colors

The v2 MVP proved that the call-based protocol is viable, but it also showed that ordinary synchronous calls are not the right hot path for every-frame rendering.

Native-like animation needs this shape:

```text
Elixir updates animation state
  -> Elixir builds one compact binary frame script
  -> AtomVM sends one request
  -> native code validates and executes the frame script
  -> LovyanGFX performs the actual drawing under one native write session
```

This ADR is the single active rendering decision for the current v2 plan. It replaces the earlier narrow packed-scalar and paletted-sprite batch ADRs while keeping their useful principles.

## Decision

Use `submitBinaryBatch` as the explicit v2 hot-rendering path.

Elixir callers build frame scripts with `AtomLGFX.BinaryBatch` and submit them through `AtomLGFX.submit_binary_batch/2` or `AtomLGFX.BinaryBatch.render/2`.

Ordinary synchronous operations remain the default path for:

- setup
- configuration
- sprite and palette lifecycle
- debugging
- low-frequency operations
- simple applications that do not need animation performance

Animation loops should avoid one native port call per primitive. They should instead submit one binary frame script per frame where practical.

## Current render-batch model

A binary render batch is an ordered command stream:

```text
opcode u8 + opcode-specific payload
```

The command stream is interpreted by the native render-batch dispatcher. The dispatcher owns:

- command decode
- command validation
- command-local target and color-mode state
- one native execution pass
- one `startWrite` / `endWrite` grouping around supported drawing work

The current batch surface includes reusable render operations rather than demo-specific commands:

- target selection
- RGB565 and palette-index color mode selection
- scalar primitive drawing
- text drawing for small overlays
- sprite push
- sprite push lists
- sprite source-region lists
- rotate/zoom instance lists
- native presentation strip begin/present commands
- display command

The intended boundary is:

```text
Elixir describes the frame.
Native executes the frame.
```

Native code must not know about application concepts such as MovingIcons or Stack-chan.

## Native presentation strips

For strip-buffered rendering in binary-batch mode, v2 uses native presentation strips instead of public sprite handles as frame buffers.

The preferred strip frame shape is:

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

While a native strip is active, logical render target `0` resolves to the active native strip. Outside an active strip, target `0` resolves to the live LCD.

This keeps application code from managing public frame-buffer sprites in the animation hot path, while still preserving the normal target-numbering contract.

The Elixir strip loop must use the strip height negotiated by native code. The native presentation layer may allocate a smaller strip height than the preferred height when memory is constrained. Hard-coding `160` rows in Elixir is not a stable contract.

## MovingIcons benchmark interpretation

The MovingIcons benchmark should focus on `strip_buffers + binary_batch` modes.

`direct_lcd` currently clears the visible display every frame. That makes it useful as a correctness/debug mode, but not as the primary animation-performance baseline.

Current draw modes have different meanings:

- `push_rotate_zoom_list`
  - closest to the dynamic transform behavior
  - useful for measuring transformed sprite cost

- `push_sprite_list`
  - whole-sprite blit baseline
  - useful for separating transform cost from sprite blit and presentation cost

- `push_sprite_region_list`
  - atlas-oriented source-region primitive
  - useful for future animation layouts and dirty-region style rendering
  - not automatically faster for MovingIcons when every object still uses a full 32x32 icon

Benchmark conclusions should not compare non-equivalent behavior as if it were the same animation.

## Stack-chan and palette-index rendering

Stack-chan-like workloads remain an important target, but should be handled through generic render-batch primitives.

The direction is:

- preserve palette-index color semantics
- support transparent palette indices separately from transparent RGB565 values
- avoid forcing naturally indexed sprites through RGB565 conversion as the main solution
- prefer atlas/list/region primitives that are reusable beyond Stack-chan

Palette-index sprite performance is follow-up implementation work, not a separate architecture.

## Consequences

### Positive

- Reduces AtomVM/native crossings in animation frames.
- Keeps ordinary calls simple and synchronous.
- Keeps native APIs generic instead of demo-specific.
- Gives MovingIcons and Stack-chan a shared performance direction.
- Keeps native presentation policy in `lgfx_device` rather than in application code.
- Gives benchmark work a clearer matrix: presentation cost, transform cost, sprite blit cost, and command decode cost can be separated.
- Consolidates rendering decisions into one active ADR.

### Negative

- Adds a second execution path that must be tested separately from ordinary calls.
- Requires careful binary command validation.
- Requires generated protocol docs and freeze tests to stay synchronized.
- Requires benchmarking discipline because faster draw modes may not be behaviorally equivalent.
- Does not make expensive LovyanGFX primitives cheap by itself.

## Rejected alternatives

### Batch every ordinary operation automatically

Rejected. That would obscure error behavior, complicate ordering, and turn the whole API into a deferred execution model.

### Reintroduce a broad v1-style native wrapper surface

Rejected. That would improve some hot paths by hand, but it would bring back the code-volume and maintenance problem v2 is meant to solve.

### Add demo-specific native APIs

Rejected. MovingIcons and Stack-chan are representative workloads, not protocol concepts.

### Treat direct LCD rendering as the main baseline

Rejected for the current benchmark. The existing direct-LCD path clears the visible display every frame, so it is not a useful native-like animation baseline.

### Keep paletted sprite rendering as a separate batch architecture

Rejected. Palette-index support is required, but it should be part of the generic render-batch command stream.

## Follow-up checklist

- [x] Add binary render-batch submission through `submitBinaryBatch`
- [x] Add reusable `AtomLGFX.BinaryBatch` command builders
- [x] Add `push_rotate_zoom_list` for transformed sprite workloads
- [x] Add `push_sprite_list` for whole-sprite blit workloads
- [x] Add `push_sprite_region_list` for atlas/source-region workloads
- [x] Add native presentation strip commands
- [x] Use native presentation strips in MovingIcons binary-batch strip mode
- [x] Avoid public frame-buffer sprite allocation for `strip_buffers + binary_batch`
- [x] Add binary fast path for prebuilt batch binaries
- [x] Add protocol query for native presentation strip height
- [x] Use native-negotiated presentation strip height in MovingIcons
- [ ] Confirm negotiated strip height behavior on target hardware
- [ ] Re-run MovingIcons benchmarks after native strip integration
- [ ] Add native timing trace if frame time remains high
- [ ] Measure render-batch validation overhead separately from draw and presentation cost
- [ ] Add first-class palette-index sprite/list/region support needed by Stack-chan
- [ ] Update this ADR or add a follow-up ADR if the rendering strategy changes substantially

## Related documents

- [Architecture](../architecture.md)
- [Protocol](../protocol.md)
- [V2 render-batch performance work log](../2026-05-02-v2-render-batch-performance-work-log.md)
- [Superseded packed scalar batch ADR](2026-04-29-packed-binary-scalar-batch.md)
- [Superseded paletted sprite workload ADR](2026-05-01-expand-binary-batch-for-paletted-sprite-workloads.md)
