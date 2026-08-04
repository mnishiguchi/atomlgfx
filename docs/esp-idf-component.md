<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ESP-IDF component

This repository provides an ESP-IDF component that exposes a native AtomVM port
driver backed by LovyanGFX.

The component implements wire protocol v3 for the current AtomLGFX v3 API. It is
distributed from this repository rather than as part of the Hex package. Build
it from the same Git commit as the `atomlgfx` Elixir dependency.

The component is intended for AtomVM firmware on ESP32-class boards. It
implements the native side of the `AtomLGFX` API used by the Elixir package in
this repository.

## What this component provides

- native `lgfx_port` AtomVM port driver
- request decode and dispatch for the tuple protocol
- LovyanGFX-backed display operations
- sprite, palette, image, text, and touch support
- explicit binary batch submission for grouped rendering work
- protocol-level capabilities and diagnostics

## What this component does not provide

- high-level Elixir ergonomics
- Elixir-side validation helpers
- application-level board configuration guidance for every target board

For Elixir-side usage, see [the Elixir package guide](elixir-package.md).

## Repository areas related to this component

- `CMakeLists.txt`
  - component entry point

- `include/lgfx_port/lgfx_port.h`
  - public native header

- `lgfx_port/`
  - AtomVM-facing protocol boundary
  - ordinary request handling
  - explicit binary batch submission decode and validation
  - frame-script dispatch

- `lgfx_device/`
  - LovyanGFX-facing device adapter

- `third_party/LovyanGFX/`
  - pinned LovyanGFX submodule

## Build preparation

Initialize the LovyanGFX submodule first.

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

## Build and flash AtomVM firmware

The repository includes a helper script for building AtomVM firmware with this
native driver.

```bash
./scripts/atomvm_esp32.exs install --target esp32s3 --port /dev/ttyACM0
```

Adjust target and serial port values for your environment.

## Configuration

Build defaults are derived from CMake configuration and emitted into the
generated native config header.

Selected values may also be overridden at port open time by the host
application.

For the ownership model, execution model, and configuration flow, see
[the architecture document](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/architecture.md).

## Protocol and capabilities

This component implements the native side of the tuple protocol used by
`AtomLGFX`.

For request and response semantics, validation rules, data encodings, and
batching behavior, see [the protocol document](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/protocol.md).

For the generated operation matrix, error reasons, and capability vocabulary,
see [the protocol reference](protocol-reference.md).

## Internal design documents

These documents are mainly for maintainers of the native layer:

- [Port layer](../lgfx_port/README.md)
- [Device adapter layer](../lgfx_device/README.md)
- [Architecture](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/architecture.md)
- [Protocol](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/protocol.md)
- [Protocol reference](protocol-reference.md)

## Notes for contributors

When changing protocol-visible behavior:

- update `lgfx_port/include_internal/lgfx_port/ops.def` as needed
- update handlers, packed batch dispatch, or device code as needed
- update `docs/protocol.md` if the external contract changed
- resync generated protocol tables

```bash
elixir scripts/sync_lgfx_protocol_doc.exs
elixir scripts/sync_lgfx_protocol_doc.exs --check
```
