<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Architecture

`atomlgfx` is split into two closely related deliverables:

- a native ESP-IDF component built around the `lgfx_port` AtomVM port driver
- an Elixir package that provides the `AtomLGFX` wrapper for that driver

This document gives the top-level system map.

For the caller-visible protocol contract, see [the protocol spec](protocol.md). For generated operation, capability, and error tables, see [the protocol reference](protocol-reference.md).

## Big picture

`atomlgfx` has two execution paths:

- a direct synchronous path for ordinary operations
- an explicit binary batch path for hot rendering work

```text
Elixir / AtomVM
    |
    | {lgfx, ProtoVer, Op, ...Args}
    | {lgfx, ProtoVer, Op, Target, Flags, ...Args}
    v
+------------------------------+
| lgfx_port/                   |
| - request decode             |
| - metadata validation        |
| - handler dispatch           |
| - binary script validation    |
| - reply encode               |
+---------------+--------------+
                |
                +--> direct synchronous path
                |      -> handlers
                |      -> lgfx_device
                |      -> LovyanGFX
                |
                +--> binary batch path
                       -> render script dispatch
                       -> lgfx_device
                       -> LovyanGFX
```

## Design summary

The current architecture uses a call-based protocol for ordinary operations and an explicit binary render-batch path for measured drawing bursts.

In short:

- ordinary operations execute immediately
- ordinary operations return real success or failure immediately
- binary batching is explicit and opt-in
- the binary-batch surface is intentionally small and measured
- frame scripts are built in Elixir and executed natively
- `startWrite` / `endWrite` grouping is internal to binary-batch execution
- target `0` is the LCD; target `1..254` are caller-owned sprites

The goal is to reduce control-plane overhead for grouped rendering without turning the whole API surface into a deferred execution system.

## Repository roles

- `include/lgfx_port/`
  - public native headers

- `lgfx_port/`
  - AtomVM-facing port layer
  - request envelope handling
  - metadata-driven validation
  - handler dispatch for ordinary operations
  - binary-batch envelope decode and script validation
  - ordered render command dispatch for explicit batch operations

- `lgfx_device/`
  - LovyanGFX-facing adapter layer
  - protocol-agnostic device operations
  - pinned-submodule integration
  - native presentation policy for logical LCD drawing

- `lib/`
  - root Elixir wrapper package
  - high-level `AtomLGFX` API
  - `AtomLGFX.BinaryBatch` command builders

- `examples/elixir/`
  - example application that consumes the root package
  - benchmark-oriented workloads such as MovingIcons

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
- no batch involvement
- no deferred failure model

### Binary render-batch path

Render batching is a separate execution path used only when the caller explicitly submits a frame script.

At a high level:

```text
submitBinaryBatch request
  -> lgfx_port validates submitBinaryBatch
  -> lgfx_port validates the binary frame script
  -> lgfx_port dispatches supported commands in order
  -> lgfx_device performs the final LovyanGFX calls
```

Properties:

- batch submission is explicit
- ordinary operations do not implicitly go through the batch path
- batch execution is ordered
- with `LGFX_PORT_RENDER_BATCH_PREVALIDATE=ON`, malformed streams are rejected before device mutation
- with the default prevalidation-off build, syntax/support checks happen while executing for lower hot-path overhead
- malformed commands or device/runtime failures can stop execution after earlier commands have run
- `startWrite` / `endWrite` grouping happens inside batch execution
- command-local state controls render target and color interpretation
- speculative compact list commands stay out of the protocol unless measured workloads justify them

## Responsibility split

### `lgfx_port/`

This layer owns protocol-facing responsibilities:

- request tuple decoding
- op lookup and validation
- handler dispatch for ordinary operations
- packed binary stream validation for explicit batch submission
- ordered packed command dispatch
- reply encoding
- protocol-visible error mapping

It should not own detailed LovyanGFX semantics.

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
- binary command builders
- convenience helpers
- wrapper-local ergonomics

It should not redefine the native protocol contract.

## Presentation model

The device layer keeps target resolution simple:

- target `0` means the live LCD device
- target `1..254` means an explicitly created sprite

Higher-level code that needs buffered or strip-based presentation owns those buffers as ordinary sprites. This keeps persistent allocation visible through `create_sprite` / `delete_sprite` and avoids hidden native presentation buffers in query or batch paths.

## Build defaults and runtime overrides

Build defaults come from CMake cache variables and are emitted into the generated config header used by the native component.

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

The design goal is one declarative source for the protocol-visible surface, with synchronized code and documentation around it.

## Ownership model

The current native design separates configuration persistence from live device ownership:

- open-time configuration is stored per port context
- the live LCD device remains singleton-backed
- only one port may own the live device at a time

This keeps per-port configuration explicit without pretending the underlying hardware is multi-instance.

## Binary payload rule

Variable-length payloads such as text, JPEG data, and RGB565 image data must not outlive the request by borrowing caller-owned term memory.

Current rule:

- ordinary handlers borrow request binary pointers only for the duration of the current request
- device calls consume those bytes synchronously
- device code must not retain borrowed payload pointers after the call returns

For explicit batching:

- `submitBinaryBatch` borrows the command binary only for the synchronous request boundary
- the render decoder must not retain pointers into the caller binary
- payload-heavy operations remain on the ordinary path unless a future batch command explicitly defines native-owned storage or request-scoped payload lifetime

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

- [`docs/adr/2026-05-05-keep-binary-batch-minimal-and-measured.md`](adr/2026-05-05-keep-binary-batch-minimal-and-measured.md)
  - active minimal BinaryBatch surface decision

- [`docs/adr/2026-05-05-native-frame-render-commands-for-hot-animation.md`](adr/2026-05-05-native-frame-render-commands-for-hot-animation.md)
  - native frame render commands for measured hot animation loops

- [`docs/2026-05-02-v2-render-batch-performance-work-log.md`](2026-05-02-v2-render-batch-performance-work-log.md)
  - benchmark notes and follow-up measurements

- [`docs/esp-idf-component.md`](esp-idf-component.md)
  - native component build and usage guide

- [`docs/elixir-package.md`](elixir-package.md)
  - Elixir wrapper usage guide

- [`lgfx_port/README.md`](../lgfx_port/README.md)
  - port-layer maintainer notes

- [`lgfx_device/README.md`](../lgfx_device/README.md)
  - device adapter and native presentation notes
