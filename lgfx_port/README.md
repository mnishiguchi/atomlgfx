<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# lgfx_port

`lgfx_port/` is the AtomVM-facing native layer.

It owns:

- request tuple handling
- metadata-driven validation
- handler dispatch
- reply mapping
- the direct call boundary from AtomVM-facing handlers into `lgfx_device/`

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

- `handlers/*.c`
  - op-specific wire decode
  - direct calls into `lgfx_device_*`

- `include_internal/lgfx_port/handler_decode.h`
  - tiny shared decode helpers for handlers
  - cached LCD dimension refresh helper

- `include_internal/lgfx_port/ops.def`
  - protocol-visible operation metadata

## Responsibility split

### Port thread

The port thread owns all AtomVM-facing work:

- mailbox handling
- tuple decoding
- metadata validation
- handler dispatch
- reply encoding
- protocol-visible error state such as `last_error`

### Handler layer

Handlers stay wire-oriented and small.

They are responsible for:

- decoding request payload fields
- translating wire values into the native call shape
- calling `lgfx_device_*`
- mapping native results to protocol replies

Handlers should not duplicate device semantics.

### Device layer

Detailed device semantics belong in `../lgfx_device/`, not here.

Examples:

- sprite existence and allocation rules
- target resolution
- palette-backed behavior
- `pushImage` payload semantics
- rotate/zoom semantic validity

## Request flow

```text
AtomVM message
  -> port thread decodes and validates
  -> handler decodes wire args
  -> handler calls lgfx_device_*
  -> handler maps result to protocol reply
```

## Core rules

- AtomVM terms stay in `lgfx_port/`.
- `lgfx_device/` stays free of AtomVM term handling.
- Handlers decode wire arguments, but should not duplicate device semantics.
- Protocol metadata lives in `ops.def`.
- Shared handler-side decode helpers live in `handler_decode.h`.
- Externally visible protocol changes must be reflected in `../docs/protocol.md`.

## Design intent

This layer should stay small and explicit.

Policy:

- keep AtomVM-facing protocol work in `lgfx_port/`
- call `lgfx_device_*` directly
- avoid extra bridging layers unless they solve a real problem
- keep request decoding close to the handlers that use it
- keep device truth in `../lgfx_device/`

The goal is not to build another abstraction tower. The goal is to keep the protocol boundary easy to read, easy to change, and hard to misunderstand.

## When changing this layer

When adding a new protocol-visible operation:

- add one row to `ops.def`
- add the handler declaration via `ops.h`
- implement the handler in `handlers/`
- keep AtomVM decoding in `lgfx_port/`
- keep detailed device semantics in `../lgfx_device/`
- update protocol docs only when the externally visible contract changes
