<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 2026-05-05: Keep BinaryBatch minimal and measured

## Status

Accepted

Supersedes:

- [ADR 2026-05-03: Treat BinaryBatch as the standard render transaction API](2026-05-03-binary-batch-as-render-transaction-api.md)

## Context

`atomlgfx` v2 introduced `BinaryBatch` to reduce AtomVM/native crossing overhead for frame rendering.

During v2 protocol development, the binary-batch surface expanded to include several render-private packed-list commands for generic primitive shapes, whole-sprite list blits, sprite-region list blits, JPEG drawing, and RGB565 image pushing.

Those commands made the batch path more expressive, but they also increased maintenance cost across:

- native batch decode and dispatch
- Elixir encoders and decoders
- protocol documentation
- generated protocol metadata
- freeze tests
- malformed-input tests
- diagnostic summaries
- benchmark logs

MovingIcons-focused measurements showed that the most important retained hot path is transformed-sprite animation. In particular, `push_rotate_zoom_list` is useful as a generic transformed-sprite batch primitive, and `push_rotate_zoom_frame_strips` is useful as a native frame command for MovingIcons-like workloads where the upstream LovyanGFX example keeps the strip loop in C++.

The speculative packed-list commands for generic primitive shapes and atlas-style sprite regions did not justify their maintenance cost for the v2 protocol.

## Decision

Keep `BinaryBatch` as the v2 render transaction API, but intentionally minimize the retained binary-batch surface.

Retain only command categories that are justified by ordinary render-script ergonomics or measured hot-path value:

- scalar render commands inside one batch transaction
- small render state commands such as target and color mode
- native presentation strip control
- simple sprite push and transformed-sprite commands
- `push_rotate_zoom_list` for generic transformed-sprite batch work
- `push_rotate_zoom_frame_strips` for measured native-loop animation workloads

Remove speculative batch commands before the v2 protocol freeze:

- packed primitive-list commands for generic shapes
- whole-sprite list and sprite-region list commands
- batch-level JPEG drawing
- batch-level RGB565 image pushing
- the extended ellipse-list sub-opcode

Payload-heavy image operations remain available as ordinary scalar calls. They are not retained in `BinaryBatch` for the v2 protocol because their payload lifetime, framing, and copying rules increase the batch contract surface without helping the measured animation hot path.

## Retained layering

The retained v2 layering is:

```text
ordinary scalar calls
  setup, queries, allocation, calibration, touch, image payloads, low-frequency control

BinaryBatch
  compact render transaction API for repeated drawing work

native frame render command
  measured hot-path escape hatch for transformed-sprite animation loops
```

## Rationale

The finalized v2 protocol optimizes for a small maintainable protocol surface while preserving the measured performance path that motivated v2.

Repeated primitive drawing can still be expressed as repeated scalar commands inside one binary batch. That costs more bytes than a packed-list command, but it avoids a parallel compact record format for every primitive shape.

New compact list commands may be reintroduced later, but only when a real workload shows that repeated scalar batch commands are insufficient and the new command remains reusable beyond one demo.

## Consequences

### Positive

- Smaller native batch decoder and dispatcher.
- Smaller Elixir encoder and decoder surface.
- Less protocol documentation and generated-reference churn.
- Fewer malformed-input and freeze-test cases to maintain.
- Clearer v2 contract.
- Keeps the transformed-sprite path that matters for MovingIcons-like workloads.
- Keeps room for future measured additions without freezing speculative commands now.

### Negative

- Some non-critical batch conveniences are no longer available.
- Some repeated primitive workloads use repeated scalar batch commands instead of compact list encodings.
- Whole-sprite list and atlas-region experiments must be reintroduced deliberately if a future workload needs them.
- Payload-heavy JPEG and RGB565 image operations must use ordinary scalar calls for now.

## Rejected alternatives

### Keep all packed-list commands because they are already implemented

Rejected.

Implementation existence is not enough to justify protocol surface. Each retained command becomes a long-term decoder, encoder, test, and documentation responsibility.

### Keep primitive packed-list commands but remove sprite-region commands only

Rejected for the v2 protocol.

The same maintenance argument applies to primitive packed-list commands. Repeated scalar batch commands are adequate until a benchmark proves otherwise.

### Remove BinaryBatch entirely and keep only the native frame command

Rejected.

`BinaryBatch` remains useful as the generic v2 render transaction API. The native frame command is a measured hot-path option, not a replacement for ordinary render scripts.

### Move animation state into native code

Rejected.

Elixir should continue to own animation state and scene behavior. Native code should only execute reusable hot render patterns.

## Follow-up implications

Protocol-facing sources and docs should reflect the smaller surface together:

- `lgfx_port/include_internal/lgfx_port/protocol.h`
- `lgfx_port/include_internal/lgfx_port/ops.def`
- `lgfx_port/render_batch_dispatch.cpp`
- `lib/atom_lgfx/binary_batch.ex`
- `lib/atom_lgfx/generated.ex`
- `docs/protocol.md`
- `docs/protocol-reference.md`
- protocol freeze and binary-batch tests

Future batch additions should include a short benchmark note or work-log reference explaining why repeated scalar batch commands are insufficient.

## Related documents

- [ADR 2026-04-28: Call-based LovyanGFX port protocol](2026-04-28-call-based-lovyangfx-port-protocol.md)
- [ADR 2026-05-05: Allow native frame render commands for hot animation loops](2026-05-05-native-frame-render-commands-for-hot-animation.md)
- [Architecture](../architecture.md)
- [Protocol](../protocol.md)
- [V2 render-batch performance work log](../2026-05-02-v2-render-batch-performance-work-log.md)
