<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 0010: Treat BinaryBatch as the standard render transaction API

## Status

Superseded

Superseded by:

- [ADR 0011: Keep BinaryBatch minimal and measured](0011-keep-binary-batch-minimal-and-measured.md)

Supersedes:

- [ADR 0005: Binary batch and instance streaming for animation hot paths](0005-binary-batch-and-instance-streaming.md)
- [ADR 0007: Packed binary scalar batch path](0007-packed-binary-scalar-batch.md)
- [ADR 0008: Expand BinaryBatch for paletted sprite workloads](0008-expand-binary-batch-for-paletted-sprite-workloads.md)
- [ADR 0009: Standardize v2 hot rendering on binary render batches](0009-binary-batch-for-native-like-animation.md)

## Context

`atomlgfx` v2 uses one call-based AtomVM port protocol. Elixir maps operation names to generated numeric opcodes, then sends a single request tuple to the native port.

This keeps the native wrapper smaller than the v1 one-function-per-operation style, but scalar port calls still have unavoidable overhead. Animation-heavy examples such as MovingIcons and Stack-chan need many drawing operations per frame, so one native call per primitive is too expensive.

LovyanGFX already has an efficient rendering model through `startWrite` / `endWrite`. Drawing operations inside that transaction can share setup and teardown costs. `BinaryBatch` gives v2 the same shape at the AtomVM boundary: Elixir builds one compact binary frame script, native code decodes it, and LovyanGFX executes the drawing work in one native transaction.

Earlier ADRs explored packed scalar batches, paletted sprite workloads, instance streams, and native-like animation. Those ideas are now folded into one rule: `BinaryBatch` is the v2 render transaction API.

## Decision

This decision has been superseded by [ADR 0011: Keep BinaryBatch minimal and measured](0011-keep-binary-batch-minimal-and-measured.md).

The original decision was that anything normally useful inside a LovyanGFX `startWrite` / `endWrite` drawing transaction should be expressible in `BinaryBatch`.

Scalar calls remain the preferred mechanism for:

- setup
- configuration
- queries
- allocation
- calibration
- touch input
- low-frequency control operations

`BinaryBatch` is the standard API for frame rendering and animation workloads.

The preferred frame shape is:

```text
Elixir frame construction
  -> one BinaryBatch payload
    -> one native port call
      -> one native lock
        -> one LovyanGFX startWrite / endWrite transaction
          -> many LovyanGFX drawing operations
```

## Design rules

### BinaryBatch owns the render transaction

Callers should not manually include `startWrite` or `endWrite` commands in a batch. Native code owns the LovyanGFX transaction boundary for the whole submitted frame script.

In this ADR, "transaction" means one native lock and one LovyanGFX write session. It does not imply rollback semantics. With native prevalidation disabled, a malformed command may fail after earlier commands in the same batch have already rendered.

### Scalar calls remain valid

Not every LovyanGFX operation belongs in `BinaryBatch`.

Scalar calls remain the right API for operations that are not part of the hot frame-rendering path, such as sprite creation, sprite deletion, display setup, dimension queries, touch polling, touch calibration, and one-off control operations.

### Batch coverage follows LovyanGFX rendering semantics

If an operation is commonly useful between `startWrite` and `endWrite`, it should be considered batchable.

Examples include:

- pixel, line, rectangle, circle, ellipse, triangle, arc, and Bézier drawing
- clipping and text drawing state
- small text drawing
- sprite push operations
- packed primitive-list operations
- RGB565 image pushing
- explicit native presentation strip operations
- final display presentation

Payload-heavy operations such as image pushing and JPEG drawing are batchable only when their binary framing and lifetime rules are explicit.

### Native strips are explicit

Native strip presentation is explicit inside `BinaryBatch`.

Callers use `begin_strip/1`, draw into the active strip, then call `present_strip/0`. `display/0` presents the frame, but it does not implicitly present an active strip.

While a native strip is active, logical target `0` resolves to the active native strip. Outside an active strip, target `0` resolves to the live LCD.

## Consequences

### Positive

- Frame rendering can be submitted as one compact payload instead of many scalar port calls.
- Animation-heavy examples avoid most AtomVM-to-native call overhead during rendering.
- The native wrapper remains small because v2 does not reintroduce a broad v1-style binding surface.
- MovingIcons and Stack-chan use the same generic rendering architecture instead of demo-specific native APIs.
- Palette-index drawing remains part of the general render-batch model instead of a separate batch architecture.
- Generated metadata, protocol docs, native decoder tests, and protocol freeze tests can describe one active render path.

### Negative

- `BinaryBatch` is a real protocol surface, not only an optimization detail.
- The binary decoder and Elixir encoders must stay synchronized.
- Malformed payload handling matters because native rendering may partially mutate the display when prevalidation is disabled.
- Payload-heavy commands require careful framing to avoid ambiguous lengths, excessive copying, or unsafe borrowed memory.
- New render-private command space is limited, so future expansion may require multiplexed command families or a follow-up wire-format ADR.

## Current implementation notes

The current implementation includes:

- `AtomLGFX.BinaryBatch.render/2` for the default hot path
- `AtomLGFX.BinaryBatch.render_checked/2` for opt-in Elixir-side preflight validation
- `AtomLGFX.BinaryBatch.validate/1` and `validate!/1`
- `AtomLGFX.BinaryBatch.summary/1`
- `AtomLGFX.BinaryBatch.diagnose/1`
- `AtomLGFX.BinaryBatch.compare/2` and `compare!/2`
- `AtomLGFX.BinaryBatch.check_budget/2` and `check_budget!/2`
- protocol drift checks for generated metadata and render-private opcodes
- malformed input tests for binary-batch decoding
- fixed-size and payload-bearing render commands
- native presentation strip commands

Detailed wire layouts belong in [the protocol document](../protocol.md) and generated tables belong in [the protocol reference](../protocol-reference.md), not in this ADR.

## v2 baseline acceptance criteria

The v2 baseline can be treated as accepted when:

- `mix test` passes on the host development machine
- `elixir scripts/sync_lgfx_protocol_doc.exs --check` passes
- the MovingIcons example runs correctly on hardware
- the BinaryBatch hot path remains one Elixir payload, one native call, one lock, and one LovyanGFX write session
- `ops.def` batch metadata, generated Elixir metadata, native render-private opcodes, and protocol freeze tests agree
- scalar APIs remain the preferred path for setup, allocation, query, touch, calibration, and low-frequency control operations

Follow-up work should be driven by measured examples and ergonomics rather than expanding the wire format by default.

## Non-goals

This ADR does not require every LovyanGFX operation to be batchable.

This ADR does not replace scalar calls.

This ADR does not introduce public manual `startWrite` / `endWrite` batch commands.

This ADR does not define every binary command layout. Those details are protocol contract, not decision history.

## Decision summary

`BinaryBatch` is the standard rendering transaction API for v2.

Scalar calls remain for setup, queries, allocation, calibration, input, and low-frequency control.

The simple rule is:

> If it belongs inside a LovyanGFX `startWrite` / `endWrite` drawing transaction, it should eventually be expressible in `BinaryBatch`.
