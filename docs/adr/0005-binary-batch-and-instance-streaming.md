<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 0005: Binary batch and instance streaming for animation hot paths

## Status

Superseded

Superseded by [ADR 0010: Treat BinaryBatch as the standard render transaction API](0010-binary-batch-as-render-transaction-api.md).

This ADR captured the early distinction between scalar control calls and render-heavy binary data. Current v2 folds that direction into the single `BinaryBatch` render transaction API.

## Context

`atomlgfx` v2 introduced a call-based protocol and explicit batching to reduce
ordinary Elixir/native call overhead while preserving a simple synchronous API
model. The current explicit scalar batch implementation is a packed binary
stream submitted through `submitBinaryBatch`.

The MovingIcons investigation showed two separate issues:

- large LovyanGFX sprite buffers must live in PSRAM on ESP32-S3-class boards
- after PSRAM is enabled, the example stops crashing, but protocol-level batch execution still cannot be expected to match native LovyanGFX speed

The native LovyanGFX reference loop is fast because most work stays in C++:

- object state is already native
- transform arguments are already scalar native values
- strip rendering loops do not allocate BEAM terms
- drawing calls do not cross the AtomVM port boundary per object
- no nested protocol batch needs to be decoded each frame

The protocol batch path still pays significant per-frame costs compared with a
specialized native loop:

- Elixir builds lists and tuples for each command
- Elixir creates float terms for angle and zoom arguments
- the native port must still decode each described command
- strip rendering may still require one batch submit plus one sprite push per strip

Packed scalar batching remains valuable for correctness, diagnostics, and
ordinary grouped scalar drawing. It is not the right representation for
high-frequency animation instance data.

The design question is:

- should `atomlgfx` continue widening generic batches, or should it add a separate binary data plane for compact, streaming animation workloads?

## Decision

Add a protocol-level binary data plane for render-heavy workloads while keeping
the ordinary call protocol and explicit scalar batching intact.

The binary data-plane direction has two layers:

1. Specialized fixed-layout binary operations are the first implementation target.
   - Add operations such as `pushRotateZoomList(Binary)`.
   - Follow with fixed-layout bulk operations such as `pushImage`, `fillRectsBinary`, or `drawLinesBinary` where measurement shows they matter.
   - Each binary format should match one repeated native loop closely.
   - Native code validates the binary header and record count, then scans records linearly without materializing nested command structures.

2. A generic binary batch stream is optional later, not required for the first performance step.
   - A command such as `submitBinaryBatchBinary(Binary)` may still be useful for some mixed binary workloads.
   - If added later, it should remain a thin streaming decoder rather than a second scene VM or a replacement for the public tuple API.

The existing tuple call protocol remains the stable, human-readable control plane:

- ordinary calls stay tuple-based
- packed `submitBinaryBatch` remains available for scalar command streams
- ordinary tuple calls remain useful for small or mixed command sets
- binary operations are explicitly opt-in and reserved for data-plane workloads

The binary data plane is an optimization, not a replacement for the public Elixir API.

## Protocol shape

The exact wire layout may evolve during implementation, but the first-class shape is a specialized fixed-layout record stream.

For the first implemented sprite transform instance stream:

```text
pushRotateZoomList payload:
  magic:bytes[4] = "PRZL"
  version:u8 = 1
  options:u8
  transparent_color:u16
  y_offset:i16
  instance_count:u16
  InstanceRecord*

InstanceRecord:
  src_target:u8
  reserved:u8
  x:i16
  y:i16
  angle_cdeg:u16
  zoom_x1024:u16
  zoom_y1024:u16
```

The preferred transform representation is fixed-point:

- angle uses centidegrees in `0..35999`
- zoom uses `x1024`
- native C++ converts fixed-point values to `float` only at the LovyanGFX call boundary

If a generic binary batch stream is added later, it should still follow the same philosophy:

```text
submitBinaryBatchBinary(Binary)

Binary:
  Header
  CommandRecord*

Header:
  magic/version/options/record_count

CommandRecord:
  opcode:u8 or u16
  target:u8
  flags:u16 or u32
  payload_len:u16 or u32
  payload:payload_len bytes
```

Even in that form, the decoder should stream over the binary and avoid materializing a full native command array unless a specific command requires owned payload storage.

## Rationale

The bottleneck is no longer just native drawing. It is the amount of BEAM allocation and protocol decode work required to describe many similar drawing operations every frame.

A compact binary stream better matches the workload:

- Elixir builds one binary rather than many nested terms
- native code scans bytes linearly
- validation is predictable and cheap
- transform values stay integer until the final C++ call
- repeated sprite operations become one command with many instances
- fixed-layout operations keep native dispatch small and mechanical

This keeps the architecture honest:

- tuple protocol remains the normal RPC-style control surface
- packed scalar batch remains a grouped control-plane feature
- binary protocol is reserved for data-plane workloads
- native LovyanGFX still performs the actual drawing
- no scene-specific `render_moving_icons_frame` API is introduced

The specialized instance-stream command is justified because repeated transformed sprite blits are a common graphics primitive, not just a MovingIcons implementation detail.

Using fixed-layout specialized operations first also avoids over-designing a universal binary command language before measurement shows it is needed.

## Consequences

### Positive

- Reduces BEAM heap pressure in animation loops
- Reduces native term-walking and tuple decode overhead
- Allows one native loop to draw many sprite instances
- Keeps integer animation state in Elixir without per-frame float terms
- Preserves the existing tuple API for readability and compatibility
- Provides a path toward native-like LovyanGFX throughput without hardcoding one demo

### Negative

- Adds a second protocol representation to maintain
- Requires careful binary layout versioning and validation
- Binary payloads are less self-describing than ordinary tuple calls
- Debugging malformed batches becomes harder without tooling
- Endianness, alignment, and record-size rules must be documented precisely
- Some workloads may need multiple specialized binary operations instead of one generic binary entry point

## Rejected alternatives

### Alternative 1: keep optimizing generic batches only

Rejected.

Generic batches reduce port call count, but they still require command
construction and decode. That overhead is fundamental to the representation and
becomes visible in frame-by-frame animation workloads.

### Alternative 2: make generic binary batch the first and only new binary feature

Rejected for now.

A universal binary command stream may still be useful later, but it is not the cheapest first step. The biggest immediate wins come from specialized fixed-layout operations that match common repeated native loops directly.

### Alternative 3: implement a scene-specific native MovingIcons renderer

Rejected.

This would likely be fastest, but it would turn the driver into a demo-specific engine. The project should remain a generic LovyanGFX wrapper.

### Alternative 4: move all object simulation state into C++

Rejected for now.

Moving object state into native code would reduce Elixir work further, but it changes the programming model more aggressively. The next step should first optimize the command/data representation while keeping application-owned state.

### Alternative 5: make every operation binary-only

Rejected.

The tuple protocol is easier to inspect, easier to validate while developing, and appropriate for ordinary operations. Binary encoding should be used where measurement shows it matters.

## Follow-up implications

- Define fixed-layout payloads for specialized binary operations in `docs/protocol.md`.
- Add capability bits for sprite instance streaming and future binary bulk operations as they are introduced.
- Implement `pushRotateZoomList` or equivalent bulk sprite-transform operation first.
- Extend `pushImage` and future bulk primitive operations with the same fixed-layout binary philosophy where measurement justifies them.
- Consider a generic `submitBinaryBatchBinary` stream only after specialized binary operations are benchmarked and the remaining gap is clear.
- Add Elixir builders that construct binaries without intermediate tuple command lists.
- Add minimal protocol tests for binary layout validation and malformed payload rejection.
- Benchmark MovingIcons with:
  - sync calls
  - packed scalar batch
  - specialized binary operation
  - generic binary batch if introduced later
  - sprite instance stream
- Keep PSRAM sprite allocation as a prerequisite for large strip-buffer workloads.
