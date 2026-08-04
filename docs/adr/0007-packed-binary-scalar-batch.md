<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 0007: Packed binary scalar batch path

## Status

Superseded

Superseded by [ADR 0010: Treat BinaryBatch as the standard render transaction API](0010-binary-batch-as-render-transaction-api.md).

## Context

This ADR recorded the first `submitBinaryBatch` decision for `atomlgfx` v2.

At that time, v2 already used one call-shaped protocol request:

```elixir
{lgfx, 2, :call, op_code, target, flags, args}
```

That ordinary call path was still appropriate for setup, configuration, text, sprite lifecycle, touch, image payloads, and ordinary drawing calls. However, drawing-heavy workloads paid too much control-plane overhead when many small scalar drawing calls crossed the AtomVM/native boundary individually.

The initial goal was narrow: add a packed scalar drawing path without turning v2 into a general LovyanGFX job runtime.

## Superseded decision

The original decision added `submitBinaryBatch` as a normal v2 operation in `ops.def`.

The outer protocol shape stayed unchanged:

```elixir
{lgfx, 2, :call, OpCode.submitBinaryBatch, target, 0, [command_binary]}
```

The command binary was defined as an atomlgfx-specific byte stream, not Erlang external term format:

```text
ordinary opcode u8 + opcode-specific little-endian scalar arguments
```

The initial path was intentionally limited:

- single target
- RGB565 colors
- fixed-size scalar drawing commands
- synchronous, completion-reporting execution
- stop on the first malformed or failed command

That narrow scalar-batch framing is now superseded by the general binary render-batch strategy.

## Current interpretation

The important surviving decision is `submitBinaryBatch` itself:

- it remains the explicit binary frame-script entry point
- it remains synchronous and completion-reporting
- it remains inside the normal protocol envelope
- it still avoids repeated AtomVM/native crossings
- it still avoids tuple/list traversal for hot rendering work

The active design is no longer only a packed scalar drawing batch. It now covers a reusable render command stream for animation-oriented work, including target selection, color-mode state, sprite lists, region lists, rotate/zoom lists, native presentation strips, and simple overlays.

For the active rendering strategy, read the later render-batch ADR.

## Consequences

### Positive

- Keeps the historical reason for adding `submitBinaryBatch`.
- Preserves the compatibility and error-handling intent of the original decision.
- Makes the later render-batch ADR the single active rendering decision.

### Negative

- This ADR no longer describes the full current binary-batch surface.
- Readers must follow the later ADR for the current architecture.

## Related documents

- [ADR 0010: Treat BinaryBatch as the standard render transaction API](0010-binary-batch-as-render-transaction-api.md)
- [Protocol](../protocol.md)
- [V2 render-batch performance work log](../worklog/20260502-v2-render-batch-performance-work-log.md)
