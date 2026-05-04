<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 2026-05-01: Expand BinaryBatch for paletted sprite workloads

## Status

Superseded

Superseded by [ADR 2026-05-03: Treat BinaryBatch as the standard render transaction API](2026-05-03-binary-batch-as-render-transaction-api.md).

## Context

This ADR originally proposed expanding the binary batch path for Stack-chan-like paletted sprite workloads.

The motivating workload remains valid:

- render into sprite-backed targets
- use palette-index colors such as `{:index, n}`
- draw many scalar primitives or sprite regions per frame
- preserve transparent palette-index behavior
- avoid unnecessary RGB565 conversion for naturally indexed assets

The later render-batch ADR broadened the scope. The project no longer treats paletted drawing as a separate batch architecture. Paletted sprite support is now one required capability of the general binary render-batch path.

## Superseded decision

The useful principles from this ADR are carried forward:

- `submitBinaryBatch` remains the single explicit binary frame-script entry point.
- Palette-index color semantics must be first-class in render batches.
- Transparent palette indices must be distinct from transparent RGB565 colors.
- Stack-chan should use generic primitives, not Stack-chan-specific native commands.
- Atlas, list, and region primitives should be reusable across animation workloads.

The older framing of this as a separate paletted-sprite batch decision is superseded.

## Current direction

Future work should extend the generic render-batch command stream.

Likely work items:

- complete palette-index scalar drawing coverage in render batches
- support palette-index transparent sprite/list/region operations
- keep RGB565 and palette-index interpretation explicit
- test that indexed colors require valid palette backing where appropriate
- benchmark Stack-chan-style workloads separately from MovingIcons

## Consequences

### Positive

- Keeps the v2 hot path unified around `submitBinaryBatch`.
- Avoids a second batch architecture just for indexed rendering.
- Preserves the Stack-chan requirement as a concrete performance target.
- Keeps future work generic and reusable.

### Negative

- This ADR no longer records the active decision by itself.
- Readers must follow the later render-batch ADR for the current architecture.
- Palette-index sprite performance remains a follow-up implementation area.

## Related documents

- [ADR 2026-05-03: Treat BinaryBatch as the standard render transaction API](2026-05-03-binary-batch-as-render-transaction-api.md)
- [Protocol](../protocol.md)
- [V2 render-batch performance work log](../2026-05-02-v2-render-batch-performance-work-log.md)
