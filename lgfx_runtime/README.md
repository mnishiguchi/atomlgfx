<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# lgfx_runtime

`lgfx_runtime/` is the batch-only native runtime layer.

It owns:

- accepted batch ownership
- runtime-owned command storage
- pending-batch lifecycle
- ordered execution of accepted batch commands
- batch state tracking
- failure recording for the most recent batch

It does not decode AtomVM terms, validate protocol envelopes, or define device semantics.
Those responsibilities belong to `lgfx_port/`, `lgfx_command_dispatch/`, and `lgfx_device/`.

See [the architecture overview](../docs/architecture.md) for the repository map and
[the protocol spec](../docs/protocol.md) for the protocol-visible batching contract.

## File map

- `lgfx_runtime.c`
  - runtime state reset and initialization
  - accepted batch enqueue
  - pending batch execution
  - batch state transitions
  - last-failure recording
  - runtime-owned command and payload cleanup

- `include_internal/lgfx_runtime/lgfx_runtime.h`
  - runtime state structure
  - runtime lifecycle API
  - enqueue and process entry points
  - batch status and failure accessors

- `include_internal/lgfx_runtime/batch_command.h`
  - native batch command representation
  - batch id and batch state types
  - failure record structure
  - owned payload wrapper
  - inline-argument storage used by the current batch command format

- `include_internal/lgfx_runtime/lgfx_command_dispatch.h`
  - bridge from runtime-owned batch commands to dispatch execution
  - per-command support checks
  - ordered batch execution entry point
  - dispatch result structure

## Core rules

- `lgfx_runtime/` begins after a batch has already been accepted by `lgfx_port/`.
- It owns runtime state, not protocol shape.
- It must not decode AtomVM terms or build protocol replies.
- It must not redefine device semantics.
- It should stay narrow and batch-only.

## Current design constraints

- one pending batch slot per port
- ordered execution only
- runtime-owned cleanup after execution or reset
- no general multi-batch scheduler

## When changing this layer

When changing `lgfx_runtime/`:

- keep protocol validation in `lgfx_port/`
- keep final device behavior in `lgfx_device/`
- keep command routing in `lgfx_command_dispatch/`
- update architecture or protocol docs only when externally visible behavior changes
