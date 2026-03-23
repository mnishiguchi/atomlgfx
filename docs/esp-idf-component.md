<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ESP-IDF component

This repository provides an ESP-IDF component that exposes a native AtomVM port driver backed by LovyanGFX.

The component is intended for AtomVM firmware on ESP32-class boards. It implements the native side of the `LGFXPort` API used by the Elixir package in this repository.

## What this component provides

- native `lgfx_port` AtomVM port driver
- request decode and dispatch for the tuple protocol
- LovyanGFX-backed display operations
- sprite, palette, image, text, and touch support
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

The repository includes a helper script for building AtomVM firmware with this native driver.

```bash
./scripts/atomvm_esp32.exs install --target esp32s3 --port /dev/ttyACM0
```

Example target and serial port values should be adjusted for your environment.

## Runtime model

This component exposes a tuple protocol between AtomVM and the native driver.

Requests use this shape:

```erlang
{lgfx, ProtoVer, Op, Target, Flags, Arg1, Arg2, ...}
```

Responses use this shape:

```erlang
{ok, Result}
{error, Reason}
```

See [the protocol document](protocol.md) for the full contract.

## Configuration model

Build defaults are derived from CMake configuration and emitted into the generated native config header.

Selected values may also be overridden at port open time by the host application.

In practice:

- build defaults define the baseline
- open-time config may override selected fields per port
- `init` applies the calling port's stored configuration snapshot

See [the architecture document](architecture.md) for the configuration and ownership model.

## Supported feature areas

Depending on build and runtime configuration, the component may provide:

- display control
- primitive drawing
- text rendering
- JPEG rendering
- RGB565 image push
- sprite operations
- palette operations
- touch operations
- diagnostics such as `getLastError`

Capability advertisement is available through the protocol.

## Internal design documents

These documents are mainly for maintainers of the native layer:

- [Port layer](../lgfx_port/README.md)
- [Device adapter layer](../lgfx_device/README.md)
- [Architecture](architecture.md)
- [Protocol](protocol.md)

## Notes for contributors

When changing protocol-visible behavior:

- update `lgfx_port/include_internal/lgfx_port/ops.def` as needed
- update handlers or device code as needed
- update `docs/protocol.md` if the external contract changed
- resync generated protocol tables

```bash
elixir scripts/sync_lgfx_protocol_doc.exs
elixir scripts/sync_lgfx_protocol_doc.exs --check
```
