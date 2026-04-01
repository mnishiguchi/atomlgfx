<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Architecture

`atomlgfx` is split into two closely related deliverables:

- a native ESP-IDF component built around the `lgfx_port` AtomVM port driver
- an Elixir package that provides the `AtomLGFX` wrapper for that driver

This document gives the top-level system map.

For the caller-visible protocol contract, see [the protocol spec](protocol.md).
For generated operation, capability, and error tables, see
[the protocol reference](protocol-reference.md).

## Big picture

`atomlgfx` has two execution paths:

- a direct synchronous path for ordinary operations
- an explicit batch path for grouped rendering work

```text
Elixir / AtomVM
    |
    | {lgfx, ProtoVer, Op, Target, Flags, ...}
    v
+------------------------------+
| lgfx_port/                   |
| - request decode             |
| - metadata validation        |
| - handler dispatch           |
| - batch command build        |
| - reply encode               |
+---------------+--------------+
                |
                +--> direct synchronous path
                |      -> handlers
                |      -> lgfx_device
                |      -> LovyanGFX
                |
                +--> explicit batch path
                       -> lgfx_runtime
                       -> lgfx_command_dispatch
                       -> lgfx_device
                       -> LovyanGFX
```

## Design summary

The current architecture keeps the v1 mental model for ordinary operations,
adds explicit batching for grouped rendering work, and keeps buffered LCD
presentation inside `lgfx_device`.

In short:

- ordinary operations execute immediately
- ordinary operations return real success or failure immediately
- batching is explicit and opt-in
- `lgfx_runtime` is used only for explicit batch execution
- native presentation policy stays in `lgfx_device`
- `startWrite` / `endWrite` grouping is an internal batch optimization

The goal is to reduce control-plane overhead for grouped rendering without
turning the whole API surface into a deferred execution system, while also
keeping live-LCD presentation concerns in the device layer rather than in
application code.

## Repository roles

- `include/lgfx_port/`
  - public native headers

- `lgfx_port/`
  - AtomVM-facing port layer
  - request envelope handling
  - metadata-driven validation
  - handler dispatch for ordinary operations
  - batch submission decode and command construction for explicit batch operations

- `lgfx_runtime/`
  - batch-only runtime support
  - pending batch ownership
  - ordered batch execution
  - batch status and failure recording

- `lgfx_command_dispatch/`
  - command-to-device routing for runtime-executed batch commands

- `lgfx_device/`
  - LovyanGFX-facing adapter layer
  - protocol-agnostic device operations
  - pinned-submodule integration
  - native presentation policy for logical LCD drawing

- `lib/`
  - root Elixir wrapper package
  - high-level `AtomLGFX` API

- `examples/elixir/`
  - example application that consumes the root package

## Execution model

### Direct synchronous path

Ordinary operations follow the direct path:

```text
AtomVM message
  -> lgfx_port decodes and validates
  -> handler decodes wire args
  -> handler calls lgfx_device_*
  -> handler maps result to protocol reply
```

This path is the default behavior for the API surface.

Properties:

- immediate execution
- immediate success or failure
- no batch runtime involvement
- no deferred failure model

### Explicit batch path

Batching is a separate execution path used only when the caller explicitly
submits a batch.

At a high level:

```text
submitBatch request
  -> lgfx_port validates submitBatch
  -> lgfx_port validates inner commands against ops.def metadata
  -> lgfx_port builds native batch commands
  -> lgfx_runtime accepts one pending batch
  -> lgfx_runtime executes commands in order
  -> lgfx_command_dispatch routes each command
  -> lgfx_device performs the final LovyanGFX call
```

Properties:

- batch submission is explicit
- ordinary operations do not implicitly go through the runtime
- batch execution is ordered
- batch status and failure state are runtime-owned
- `startWrite` / `endWrite` grouping happens inside batch execution

## Current runtime scope

`lgfx_runtime` is intentionally narrow.

It owns batch-only concerns such as:

- pending batch command ownership
- ordered execution of accepted batch commands
- grouped execution windows
- batch status
- failure recording

It is not a universal execution layer for the whole driver.

Current design choice:

- one pending batch slot per port
- no general multi-batch scheduler
- no requirement that all operations become deferred or queue-backed

This keeps the execution model small and easy to reason about while still
delivering the main batching benefit for rendering-heavy workloads.

## Responsibility split

### `lgfx_port/`

This layer owns protocol-facing responsibilities:

- request tuple decoding
- op lookup and validation
- handler dispatch for ordinary operations
- batch command construction for explicit batch submission
- reply encoding
- protocol-visible error mapping

It should not own detailed LovyanGFX semantics.

### `lgfx_runtime/`

This layer owns explicit batch execution responsibilities:

- accepted batch ownership
- ordered execution of native batch commands
- batch status transitions
- failure recording
- grouped batch execution flow

It should not become the default execution path for ordinary operations.

### `lgfx_command_dispatch/`

This layer owns the bridge between runtime-executed native commands and the
device adapter surface.

It should:

- receive decoded native batch commands
- route them to the correct `lgfx_device_*` entry point

It should not decode AtomVM terms or define protocol behavior.

### `lgfx_device/`

This layer owns device-facing responsibilities:

- target resolution
- render-target resolution for logical LCD drawing
- sprite existence and allocation rules
- palette-backed behavior
- image and JPEG device semantics
- native strip-presentation state and lifecycle
- presentation fallback behavior
- final validation and forwarding to the pinned LovyanGFX call surface

It should not decode AtomVM terms or build protocol replies.

### `lib/`

This layer owns Elixir-facing responsibilities:

- wrapper API shape
- Elixir-side validation and normalization
- convenience helpers
- wrapper-local ergonomics

It should not redefine the native protocol contract.

## Native presentation model

The device layer distinguishes between raw target resolution and logical
render-target resolution.

For drawing:

- raw target resolution
  - target `0` means the live LCD device

- render-target resolution
  - target `0` may resolve to an active native presentation strip during strip presentation
  - otherwise it falls back to the live LCD

This keeps raw LCD control separate from logical LCD drawing and allows native
presentation policy to evolve without changing the public target numbering.

The current native presentation path uses:

- lazy, not eager, allocation
- adaptive double strip buffers
- direct-LCD fallback when native strip allocation is unavailable

At the moment, higher-level code may still manage some strip orchestration on
its own. Native strip presentation is therefore available as a device-layer
capability without yet being the only possible strip path.

## Build defaults and runtime overrides

Build defaults come from CMake cache variables and are emitted into the
generated config header used by the native component.

- template:
  - `lgfx_port/cmake/lgfx_port_config.h.in`

- generated output:
  - `<build>/.../generated/lgfx_port/lgfx_port_config.h`

Selected values may also be overridden per port at `open_port/2`. In practice:

- build defaults define the baseline
- open-time config may override selected fields per port
- `init` applies the calling port's stored snapshot

## Metadata-driven surface

The protocol-visible operation surface is declared in:

- `lgfx_port/include_internal/lgfx_port/ops.def`

That metadata drives:

- operation registration
- validation rules
- dispatch surface
- capability linkage
- internal execution classification for batch support
- generated tables in `docs/protocol-reference.md`

The design goal is one declarative source for the protocol-visible surface,
with synchronized code and documentation around it.

## Ownership model

The current native design separates configuration persistence from live device
ownership:

- open-time configuration is stored per port context
- the live LCD device remains singleton-backed
- only one port may own the live device at a time

This keeps per-port configuration explicit without pretending the underlying
hardware is multi-instance.

## Binary payload rule

Variable-length payloads such as text, JPEG data, and RGB565 image data must
not outlive the request by borrowing caller-owned term memory.

Current rule:

- ordinary handlers borrow request binary pointers only for the duration of the current request
- device calls consume those bytes synchronously
- device code must not retain borrowed payload pointers after the call returns

For explicit batching, payload-bearing commands are a separate concern:

- queued payload-bearing commands must not retain borrowed request pointers
- batch execution must use runtime-owned payload storage when payload-bearing ops are added to the batch path

See [the protocol spec](protocol.md) for the caller-visible payload contract.

## Pinned LovyanGFX policy

`atomlgfx` targets the pinned LovyanGFX submodule used by this repository.

Policy:

- prefer direct integration over compatibility probing
- update deliberately when the pinned submodule changes
- keep compatibility scaffolding minimal

## Where to read next

- [`docs/protocol.md`](protocol.md)
  - tuple protocol contract

- [`docs/protocol-reference.md`](protocol-reference.md)
  - generated operation, capability, and error tables

- [`docs/esp-idf-component.md`](esp-idf-component.md)
  - native component build and usage guide

- [`docs/elixir-package.md`](elixir-package.md)
  - Elixir wrapper usage guide

- [`lgfx_port/README.md`](../lgfx_port/README.md)
  - port-layer maintainer notes

- [`lgfx_device/README.md`](../lgfx_device/README.md)
  - device adapter and native presentation notes

- [`lgfx_runtime/README.md`](../lgfx_runtime/README.md)
  - batch runtime maintainer notes
