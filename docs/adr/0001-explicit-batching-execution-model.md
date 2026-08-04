<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 0001: Explicit batching execution model

## Status

Superseded

Superseded by [ADR 0006: Flatten native v2 implementation](0006-flatten-native-v2-implementation.md)
and [ADR 0007: Packed binary scalar batch path](0007-packed-binary-scalar-batch.md).

Current v2 does not expose the tuple/list batch runtime described below. The
implemented batch fast path is the packed binary scalar `submitBinaryBatch`
operation.

## Context

`atomlgfx` aims to improve rendering-heavy workloads while preserving the simple and predictable mental model of ordinary operations.

The adopted direction is:

- keep ordinary operations synchronous
- make batching explicit and optional
- use `lgfx_runtime` only for batch execution
- keep `lgfx_device` as the LovyanGFX-facing adapter
- treat `startWrite` / `endWrite` as an internal batch optimization

For MovingIcons-like workloads, the main performance objective is to reduce Elixir/native control-plane overhead:

- fewer Elixir-to-native crossings
- fewer reply allocations
- fewer lock/unlock cycles
- wider native write windows across grouped rendering commands

Tuple batching helps with control-plane overhead, but it is not the final representation for repeated homogeneous animation data. Fixed-layout binary operations remain a better long-term fit for data-plane workloads.

At the time this ADR was accepted, the batch path provided:

- explicit batch submission via `submitBinaryBatch`
- inline, no-payload batch commands for the first render-heavy slice
- runtime tracking for batch id, state, and failure
- a single pending batch slot per port
- grouped native dispatch under a wider write window
- a preserved direct synchronous path for ordinary non-batch operations

A design question remained open:

- should batch execution be forced into a richer deferred worker/queue model now,
  or should the architecture stay smaller and focus on the main performance win first?

## Decision

`atomlgfx` adopts the following batching execution model:

1. Ordinary non-batch operations remain on the direct synchronous path.
   - Requests are decoded in `lgfx_port`.
   - Handlers call `lgfx_device_*` directly.
   - Success and failure for ordinary operations remain immediate.

2. Batching is explicit and opt-in.
   - Heavy render paths may build and submit a batch explicitly.
   - Ordinary API usage continues to feel like v1.

3. `lgfx_runtime` is batch-only infrastructure, but not a general queue.
   - The current design uses a single pending batch slot per port.
   - A second batch is rejected while one batch is pending.
   - This is an intentional simplification, not an accidental omission.

4. The batching model does not require a separate worker or richer deferred scheduler at this time.
   - Same port-thread-driven batch execution is acceptable.
   - We do not introduce extra queueing machinery solely to make the model appear more asynchronous.
   - The architecture should stay honest about the execution model it actually needs.

5. The main performance win comes from grouped native execution, not from stricter delay semantics.
   - Many Elixir-side render calls collapse into one explicit batch submit.
   - Many replies collapse into one reply.
   - Batch commands execute in one native loop.
   - Batch execution may hold one wider `startWrite` / `endWrite` window across grouped commands.

6. `lgfx_runtime` must not become the universal execution path.
   - Runtime concerns stay confined to explicit batching.
   - Ordinary operations remain direct.
   - `lgfx_device` remains the device-facing execution core.

7. Tuple batch is a control-plane feature, not the final animation data plane.
   - It is suitable for grouped coarse operations, diagnostics, and mixed command sets.
   - It is not intended to become a second interpreted graphics VM.

8. Public batch policy belongs to Elixir.
   - Elixir decides whether an operation is public-batchable, raw-only, payload-bearing, or otherwise unsafe for the public batch API.
   - Native batch code still rejects malformed payloads, invalid opcodes, unsupported live capabilities, payload-ownership violations, and runtime-state errors.

9. Repeated homogeneous hot-path rendering should prefer binary operations or native presentation helpers.
   - Tuple batch should not grow indefinitely to describe every frame-oriented workload.
   - The batch runtime stays smaller when high-frequency animation traffic uses a dedicated binary data plane.

## Rationale

This decision keeps the architecture aligned with the narrower direction:

- `v2 = v1 + explicit batching`
- the direct synchronous path remains the default path
- explicit batching is the only place where deferred-style execution semantics appear
- runtime concerns remain separate from device semantics
- tuple batch remains a grouped control-plane tool rather than the long-term animation data plane

This approach is simpler to implement, easier to explain, and sufficient for the main performance goal:

- improve MovingIcons-like heavy render performance by reducing Elixir/native control-plane overhead

A richer asynchronous queue model would add complexity:

- broader queue semantics
- more lifecycle bookkeeping
- more states to explain and test
- more opportunities to blur the direct sync path

That complexity is not justified unless measurement shows that the simpler model is insufficient.

## Consequences

### Positive

- Preserves the simple v1 mental model for ordinary operations
- Keeps batching explicit and contained
- Reduces protocol/control-plane overhead where it matters most
- Avoids premature queue/scheduler complexity
- Keeps the current implementation easier to reason about and benchmark

### Negative

- The runtime semantics are simpler than a full queue/worker model
- A single pending batch slot limits concurrency across multiple outstanding batches
- Future `flush` or wait semantics may need refinement if richer scheduling is introduced later
- Documentation must be careful not to overstate async behavior
- Tuple batch is not the ideal representation for repeated homogeneous animation records

## Rejected alternatives

### Alternative 1: route all operations through runtime

Rejected.

This would over-generalize the runtime, add deferred semantics to ordinary operations, and weaken the simple v1-style behavior that remains valuable for most of the API.

### Alternative 2: require a richer worker or queue model now

Rejected for now.

A stricter queue model may become useful later, but it is not required to validate the main performance objective for MovingIcons-like workloads.

### Alternative 3: optimize for semantic purity before measuring performance

Rejected.

The primary concern is to prove the main win first: fewer Elixir/native crossings and wider native execution windows in heavy render paths.

## Follow-up implications

- Benchmark MovingIcons and similar grouped render workloads against v1-style per-op execution.
- Keep the single pending batch slot explicit in code comments and documentation.
- Avoid introducing broader runtime semantics until benchmark data shows they are needed.
- Keep tuple batch focused on grouped control-plane operations.
- Prefer fixed-layout binary operations or native presentation helpers for repeated homogeneous rendering workloads.
- Revisit richer queueing, flush semantics, and payload batching only if measured benefits justify the added complexity.
