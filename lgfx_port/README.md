<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# lgfx_port

`lgfx_port/` is the AtomVM-facing native layer.

It owns:

- request tuple handling
- defensive envelope and native-state validation
- handler dispatch for ordinary operations
- reply mapping
- explicit binary batch submission decode and validation
- render command validation and dispatch for the batch path

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
  - request-envelope validation helpers

- `open_config.c`
  - `open_port/2` option parsing and validation

- `op_registry.c`
  - `ops.def`-driven op metadata lookup
  - opcode dispatch-surface lookup
  - capability derivation

- `render_batch_dispatch.cpp`
  - multi-target render script validation for `submitBinaryBatch`
  - batch-local target and color-mode state
  - sprite push, transform-list, scalar drawing, and display dispatch under one native write window

- `handlers.c`
  - op-specific wire decode for ordinary operations
  - direct calls into `lgfx_device_*`
  - includes small section files from `include_internal/lgfx_port/handlers/`

- `include_internal/lgfx_port/handler_decode.h`
  - tiny shared decode helpers for handlers
  - cached LCD dimension refresh helper

- `include_internal/lgfx_port/ops.def`
  - native safety metadata
  - allowed flags
  - target and state policy
  - capability linkage
  - internal execution classification for binary batch support

## Responsibility split

### Port thread

The port thread owns all AtomVM-facing work:

- mailbox handling
- tuple decoding
- crash-safety and native-state validation
- handler dispatch
- reply encoding
- protocol-visible error state such as `last_error`

### Ordinary direct call path

The ordinary path is synchronous and intentionally boring:

```text
request tuple
  -> structural decode
  -> `ops.def` metadata validation
  -> one handler
  -> one `lgfx_device_*` adapter call
  -> one protocol reply
```

Handlers stay wire-oriented and small.

They are responsible for:

- decoding request payload fields
- translating wire values into the native call shape
- calling `lgfx_device_*`
- mapping native results to protocol replies

Handlers should not duplicate device semantics. They should also not depend on
binary-batch dispatch state.

Payload-bearing scalar operations such as `drawString`, `print`, `println`,
`drawJpg`, and `pushImage` remain valid on this direct synchronous path;
borrowed binary pointers must be consumed before the handler returns. The
render-batch path may also support explicitly framed payload operations, such as
BinaryBatch `drawJpg` and `pushImage`, when the native decoder owns the full
command stream.

### Render-batch path

Explicit binary batch submission is also owned by `lgfx_port/`.

This path is responsible for:

- validating `submitBinaryBatch`
- accepting exactly one non-empty binary command stream
- rejecting malformed command bytes as `bad_args`
- rejecting unsupported binary command opcodes as `bad_op`
- optionally prevalidating the full stream before device mutation
- validating and dispatching supported render commands synchronously

Batch dispatch remains inside `lgfx_port/` and routes decoded commands to
`lgfx_device_*`. It keeps wire-protocol render script handling separate from
the LovyanGFX adapter logic in `../lgfx_device/`.

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
- no batch involvement

### Explicit binary batch submission

```text
submitBinaryBatch message
  -> port thread decodes and validates submitBinaryBatch
  -> render script is validated
  -> supported render commands are dispatched in order
```

This path is used only for explicit binary batching.

Properties:

- batching is opt-in
- ordinary operations do not implicitly flow through the batch path
- binary-batch command execution happens outside the ordinary handler path
- command execution is synchronous and stops at the first malformed or failed command
- with `LGFX_PORT_RENDER_BATCH_PREVALIDATE=ON`, malformed streams are rejected before any command mutates the device
- with the default prevalidation-off build, malformed commands are detected while executing to avoid an extra hot-path pass

## Core rules

- AtomVM terms stay in `lgfx_port/`.
- `lgfx_device/` stays free of AtomVM term handling.
- Handlers decode wire arguments, but should not duplicate device semantics.
- Elixir owns public API policy and friendly validation.
- Native metadata in `ops.def` is limited to safety, capability, and execution guardrails.
- Shared handler-side decode helpers live in `handler_decode.h`.
- Externally visible protocol changes must be reflected in `../docs/protocol.md`.
- Generated protocol reference tables must stay synchronized with `ops.def` and protocol constants.

## Design intent

This layer should stay small and explicit.

Policy:

- keep AtomVM-facing protocol work in `lgfx_port/`
- keep ordinary operations on the direct synchronous handler path
- keep batching explicit rather than implicit
- keep batch dispatch out of ordinary handlers
- keep request decoding close to the code that uses it
- keep device truth in `../lgfx_device/`

The goal is not to build another abstraction tower. The goal is to keep the protocol boundary easy to read, easy to change, and hard to misunderstand.

## Relationship to batch dispatch

`lgfx_port/` is protocol-facing and owns explicit batch dispatch.

The split is:

- `lgfx_port/`
  - accepts and validates explicit binary batch submission
  - validates render scripts
  - dispatches render commands in order

- `lgfx_device/`
  - owns the final LovyanGFX-facing behavior

This keeps binary batching explicit and prevents it from becoming the default execution
path for the whole API surface.

## Binary payload rule

Borrowed request payloads are request-scoped.

For ordinary operations:

- handlers may borrow request binary pointers only for the duration of the current request
- device calls must fully consume borrowed payloads before returning
- borrowed payload pointers must not survive the request boundary

For explicit binary batching:

- retained payloads must stay inside the current request unless native-owned storage is introduced later

## When changing this layer

When adding or changing a protocol-visible operation:

- add or update one row in `ops.def`
- add the handler declaration via `ops.h` when needed
- implement the ordinary handler in the relevant `handlers.c` section when the op has a direct path
- update `render_batch_dispatch.cpp` when the op participates in binary batching
- keep AtomVM decoding in `lgfx_port/`
- keep detailed device semantics in `../lgfx_device/`
- update protocol docs when the externally visible contract changes
- resync generated protocol reference tables

## Render-batch prevalidation

`LGFX_PORT_RENDER_BATCH_PREVALIDATE=ON` enables a full syntax/support pass before
starting the native write session. This preserves the strict no-partial-mutation
behavior for malformed batches, but it parses each batch twice.

The default is `OFF` so animation hot loops validate commands while executing
and avoid the extra pass. Keep it off for performance demos; enable it while
debugging native render-batch encoding issues.

Example CMake override:

```sh
-DLGFX_PORT_RENDER_BATCH_PREVALIDATE=ON
```

## Render-batch trace logging

`LGFX_PORT_ENABLE_RENDER_BATCH_TRACE=ON` enables native binary-batch execution counters.

This is intended for temporary animation-performance diagnosis. It logs one line per
`submitBinaryBatch` execution with timing and counters such as:

- batch bytes
- prevalidation time, when `LGFX_PORT_RENDER_BATCH_PREVALIDATE=ON`
- native write-session start time
- binary-batch execution time
- native write-session end time
- total measured native batch time
- command count
- scalar command count
- text command count
- sprite push count, including `pushSprite` and one-off `pushRotateZoom`
- `pushRotateZoomList` command count
- total transform-list instances
- executed transform instances
- approximately culled transform instances
- display command count

Example CMake override:

```sh
-DLGFX_PORT_ENABLE_RENDER_BATCH_TRACE=ON
```

Keep this disabled for normal demos to avoid serial log overhead in hot animation loops.
