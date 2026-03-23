<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# atomlgfx

`atomlgfx` is a LovyanGFX integration for AtomVM on ESP32-class boards.

This repository contains two closely related deliverables:

- an ESP-IDF component that provides the native `lgfx_port` AtomVM port driver
- an Elixir package that provides the `AtomLGFX` wrapper module for that driver

Both layers share the same tuple-based protocol and are intended to evolve together.

## What to read

- [ESP-IDF component guide](docs/esp-idf-component.md)
- [Elixir package guide](docs/elixir-package.md)
- [Architecture](docs/architecture.md)
- [Protocol](docs/protocol.md)

## Repository map

- `CMakeLists.txt`
  - ESP-IDF component entry point
- `include/`
  - public native headers
- `lgfx_port/`
  - AtomVM-facing native port layer
- `lgfx_device/`
  - LovyanGFX-facing native adapter layer
- `lib/`
  - Elixir wrapper package
- `examples/elixir/`
  - example AtomVM application
- `third_party/LovyanGFX/`
  - pinned LovyanGFX submodule

## Status

This project is under active development.

Until the first tagged release, the native driver and the Elixir wrapper should be updated together.
