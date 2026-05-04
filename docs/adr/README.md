<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

## Architecture Decision Records

We keep architecture decisions as Markdown ADRs in the repository.

## Current decision map

Current v2 readers should normally start with these active ADRs:

- [ADR 2026-04-28: Call-based LovyanGFX port protocol](2026-04-28-call-based-lovyangfx-port-protocol.md)
  - scalar call protocol and generated numeric opcode model
- [ADR 2026-05-03: Treat BinaryBatch as the standard render transaction API](2026-05-03-binary-batch-as-render-transaction-api.md)
  - active render-batch and animation hot-path rule
- [ADR 2026-04-29: Flatten native v2 implementation](2026-04-29-flatten-native-v2-implementation.md)
  - native implementation shape for the v2 protocol
- [ADR 2026-04-07: Controller-first panel and touch support](2026-04-07-controller-first-panel-and-touch-support.md)
  - hardware configuration direction

Earlier binary-batch ADRs are preserved as history and should not be read as the active protocol contract.

### Basic rules

- Write an ADR when a decision changes long-term architecture, layering, execution model, ownership model, buffering model, or public API behavior.
- Keep one ADR focused on one decision.
- Use stable statuses:
  - `Proposed`
  - `Accepted`
  - `Superseded`
  - `Deprecated`
- Use `Accepted` consistently once a decision is agreed.
- When a decision moves from `Proposed` to `Accepted`, update the same ADR file in place.
- Create a new ADR only when a later decision changes or replaces the earlier one.
- When that happens, keep the old ADR and mark it `Superseded`.
- Cross-reference related ADRs when one builds on or replaces another.

### Scope guidance

Use ADRs for decisions such as:

- protocol and API semantics
- execution model changes
- runtime and buffering strategy
- driver/device-layer responsibility splits
- payload ownership and lifetime rules
- compatibility and versioning policy

Do not use ADRs for routine implementation details, temporary checklists, or ordinary task tracking.

### Benchmark and work-log guidance

Use ADRs for durable decisions. Keep benchmark logs, temporary measurements, and task checklists in normal `docs/` work-log files, then link them from the relevant ADR.

Recommended naming for work logs:

```text
docs/YYYY-MM-DD-short-topic-work-log.md
```

This keeps ADRs readable while preserving the evidence that motivated the decision.

### Contributor rule

If an ADR leads to protocol-visible behavior changes, also update the protocol-facing sources and docs together:

- `lgfx_port/include_internal/lgfx_port/ops.def`
- handlers or device code as needed
- `docs/protocol.md`
- synchronized protocol tables

Current contributor guidance already requires those protocol-facing updates when externally visible behavior changes.

### Naming

Recommended file naming:

```text
docs/adr/YYYY-MM-DD-short-title.md
```

Examples:

- `docs/adr/2026-04-01-explicit-batching-execution-model.md`
- `docs/adr/2026-04-02-driver-managed-strip-buffer-composition.md`

### Minimal ADR template

```markdown
# ADR YYYY-MM-DD: Title

## Status

Proposed

## Context

## Decision

## Rationale

## Consequences

### Positive

### Negative

## Rejected alternatives

## Follow-up implications
```
