<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# lgfx_port

`lgfx_port/` is the AtomVM-facing native layer.

It owns:

- request tuple handling
- metadata-driven validation
- handler dispatch for ordinary operations
- reply mapping
- explicit batch submission decode and validation
- native batch command construction for the batch path

See [the protocol spec](../docs/protocol.md) for wire-level rules and
[the architecture overview](../docs/architecture.md) for the top-level repository map.

## File map

- `lgfx_port.c`
  - port entrypoint
  - mailbox drain
  - per-port lifecycle
  - request dispatch

- `proto_term.c`
  - request and reply term helpers

- `open_config.c`
  - `open_port/2` option parsing and validation

- `request_validation.c`
  - request-envelope validation helpers

- `op_registry.c`
  - `ops.def`-driven op metadata lookup
  - dispatch-surface lookup
  - capability derivation

- `batch_decode.c`
  - `submitBatch` decode and validation
  - inner command validation against `ops.def`
  - native batch command construction

- `handlers/*.c`
  - op-specific wire decode for ordinary operations
  - direct calls into `lgfx_device_*`

- `include_internal/lgfx_port/handler_decode.h`
  - tiny shared decode helpers for handlers
  - cached LCD dimension refresh helper

- `include_internal/lgfx_port/ops.def`
  - protocol-visible operation metadata
  - allowed flags
  - target and state policy
  - capability linkage
  - internal execution classification for batch support

## Responsibility split

### Port thread

The port thread owns all AtomVM-facing work:

- mailbox handling
- tuple decoding
- metadata validation
- handler dispatch
- reply encoding
- protocol-visible error state such as `last_error`

### Ordinary handler path

Handlers stay wire-oriented and small.

They are responsible for:

- decoding request payload fields
- translating wire values into the native call shape
- calling `lgfx_device_*`
- mapping native results to protocol replies

Handlers should not duplicate device semantics.

### Explicit batch path

Explicit batch submission is also owned by `lgfx_port/`.

This path is responsible for:

- validating `submitBatch`
- validating inner batch commands against `ops.def`
- rejecting unsupported batch shapes at the protocol boundary
- building native batch commands for execution by the batch runtime

`lgfx_port/` does not execute batch commands itself. After command construction, execution responsibility moves to the batch runtime path.

### Device layer

Detailed device semantics belong in `../lgfx_device/`, not here.

Examples:

- sprite existence and allocation rules
- target resolution
- palette-backed behavior
- `pushImage` payload semantics
- rotate/zoom semantic validity

## Request flow

`lgfx_port/` has two request flows.

### Ordinary operations

```text
AtomVM message
  -> port thread decodes and validates
  -> handler decodes wire args
  -> handler calls lgfx_device_*
  -> handler maps result to protocol reply
```

This is the default execution path.

Properties:

- immediate execution
- immediate success or failure
- no batch runtime involvement

### Explicit batch submission

```text
submitBatch message
  -> port thread decodes and validates submitBatch
  -> inner commands are validated against ops.def
  -> native batch commands are built
  -> accepted batch is handed to lgfx_runtime
```

This path is used only for explicit batching.

Properties:

- batching is opt-in
- ordinary operations do not implicitly flow through the batch runtime
- batch command execution happens outside the ordinary handler path

## Core rules

- AtomVM terms stay in `lgfx_port/`.
- `lgfx_device/` stays free of AtomVM term handling.
- Handlers decode wire arguments, but should not duplicate device semantics.
- Protocol metadata lives in `ops.def`.
- Shared handler-side decode helpers live in `handler_decode.h`.
- Externally visible protocol changes must be reflected in `../docs/protocol.md`.
- Generated protocol reference tables must stay synchronized with `ops.def` and protocol constants.

## Design intent

This layer should stay small and explicit.

Policy:

- keep AtomVM-facing protocol work in `lgfx_port/`
- keep ordinary operations on the direct synchronous handler path
- keep batching explicit rather than implicit
- keep batch-runtime concerns out of ordinary handlers
- keep request decoding close to the code that uses it
- keep device truth in `../lgfx_device/`

The goal is not to build another abstraction tower. The goal is to keep the protocol boundary easy to read, easy to change, and hard to misunderstand.

## Relationship to the batch runtime

`lgfx_port/` is protocol-facing. `lgfx_runtime/` is execution-facing.

The split is:

- `lgfx_port/`
  - accepts and validates explicit batch submission
  - builds native batch commands

- `lgfx_runtime/`
  - owns accepted batch state
  - executes batch commands in order
  - records batch status and failure state

- `lgfx_command_dispatch`
  - routes runtime-executed native commands to the appropriate `lgfx_device_*` entry point

This keeps the batch runtime narrow and prevents it from becoming the default execution path for the whole API surface.

## Binary payload rule

Borrowed request payloads are request-scoped.

For ordinary operations:

- handlers may borrow request binary pointers only for the duration of the current request
- device calls must fully consume borrowed payloads before returning
- borrowed payload pointers must not survive the request boundary

For explicit batching:

- payload-bearing batch commands are a separate concern
- queued payload-bearing commands must use runtime-owned storage when that part of the batch path is supported

## When changing this layer

When adding or changing a protocol-visible operation:

- add or update one row in `ops.def`
- add the handler declaration via `ops.h` when needed
- implement the ordinary handler in `handlers/` when the op has a direct path
- update `batch_decode.c` when the op participates in explicit batching
- keep AtomVM decoding in `lgfx_port/`
- keep detailed device semantics in `../lgfx_device/`
- update protocol docs when the externally visible contract changes
- resync generated protocol reference tables
