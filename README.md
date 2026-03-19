<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# atomlgfx

`atomlgfx` is a LovyanGFX integration for AtomVM on ESP32-class boards.

This repository contains two closely related deliverables:

- an ESP-IDF component that provides the native `lgfx_port` AtomVM port driver
- an Elixir package that provides the `LGFXPort` wrapper module for that driver

Both layers share the same tuple-based protocol and are intended to evolve together.

## Repository map

- `CMakeLists.txt`
  - ESP-IDF component entry point

- `include/`
  - public native headers

- `lgfx_port/`
  - AtomVM-facing port layer
  - request decode, dispatch, direct device calls

- `lgfx_device/`
  - LovyanGFX-facing device adapter layer

- `lib/`
  - root Elixir wrapper package

- `examples/elixir/`
  - example application that consumes the root package

- `third_party/LovyanGFX/`
  - pinned LovyanGFX submodule

## Documentation

- [Architecture](docs/architecture.md)
- [Protocol](docs/protocol.md)
- [Port layer](lgfx_port/README.md)
- [Device adapter layer](lgfx_device/README.md)

## Quickstart

### Prepare the repository

```bash
cd atomlgfx

git submodule sync --recursive
git submodule update --init --recursive
```

### Build and flash AtomVM firmware with the native driver

```bash
./scripts/atomvm_esp32.exs install --target esp32s3 --port /dev/ttyACM0
```

### Fetch Elixir dependencies

```bash
mix deps.get
```

### Build and flash the example app

```bash
cd examples/elixir
mix deps.get
mix do clean, atomvm.esp32.flash --port /dev/ttyACM0
```

### Monitor serial output

```bash
cd ../..
./scripts/atomvm_esp32.exs monitor --port /dev/ttyACM0
```

Before flashing the example app, adjust the example application's `LGFXPort.open(...)` call for your board settings and pin assignments.

## Protocol at a glance

Requests and responses use this shape:

```erlang
{lgfx, ProtoVer, Op, Target, Flags, ...}
{ok, Result}
{error, Reason}
```

See `docs/protocol.md` for the full operation matrix, validation rules, capability bits, and operation semantics.

## Contributor note

Generated tables inside `docs/protocol.md` are synchronized from source metadata.

```bash
elixir scripts/sync_lgfx_protocol_doc.exs
elixir scripts/sync_lgfx_protocol_doc.exs --check
```

## Status

This repository is under active development.

The protocol is usable, but the project is still pre-release. Until the first tagged release, the native driver and the Elixir wrapper should be updated together.
