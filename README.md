# atomlgfx

`atomlgfx` is a LovyanGFX integration for AtomVM on ESP32-class boards.

This repository currently serves two closely related deliverables:

- an ESP-IDF component that provides the native `lgfx_port` AtomVM port driver
- an Elixir package that provides the `LGFXPort` wrapper module for that driver

The native driver and the Elixir wrapper share the same tuple-based protocol and are intended to evolve together.

## What is in this repository

### ESP-IDF component

The native component:

- exposes LovyanGFX to AtomVM through a synchronous tuple-based port protocol
- compiles LovyanGFX from the vendored submodule
- owns the native protocol implementation, worker model, and device integration

Key native areas:

- `CMakeLists.txt`
- `include/`
- `lgfx_port/`
- `src/`
- `third_party/LovyanGFX`

### Elixir package

The Elixir package:

- provides the `LGFXPort` module
- wraps the tuple protocol with a more idiomatic Elixir API
- performs Elixir-side argument validation, normalization, and convenience handling

Key Elixir areas:

- `mix.exs`
- `lib/`
- `test/`

### Example app

The example app under `examples/elixir` is a consumer of the root `LGFXPort` package.

It exists to exercise the driver and demonstrate usage patterns such as:

- device bring-up
- primitives
- text
- sprites
- images
- touch

## Documentation

- [Protocol specification](docs/LGFX_PORT_PROTOCOL.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Worker model](docs/WORKER_MODEL.md)

## Quickstart

### 1. Prepare the repository

```bash
cd atomlgfx

git submodule sync --recursive
git submodule update --init --recursive
```

### 2. Build and flash AtomVM firmware with the native driver

```bash
./scripts/atomvm_esp32.exs install --target esp32s3 --port /dev/ttyACM0
```

### 3. Build the root Elixir package

```bash
mix deps.get
mix test
```

### 4. Build and flash the example app

```bash
cd examples/elixir
mix deps.get
mix do clean, atomvm.esp32.flash --port /dev/ttyACM0
```

### 5. Monitor serial output

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

See `docs/LGFX_PORT_PROTOCOL.md` for the full operation matrix, validation rules, capability bits, and operation semantics.

## Native configuration

LovyanGFX is vendored as a git submodule under `third_party/LovyanGFX` and compiled as part of the native component.

Build defaults come from CMake cache variables set by the parent ESP-IDF project or passed via `idf.py -D...`. Selected values may also be overridden later at port open time. When an open-time key is omitted, the build default is used.

Common build-default options:

- `LGFX_PORT_ENABLE_JP_FONTS` (`ON`)
- `LGFX_PORT_ENABLE_TOUCH` (`ON`)
- `LGFX_PORT_PANEL_DRIVER` (`ILI9488`; supported: `ILI9488`, `ILI9341`, `ILI9341_2`, `ST7789`)
- `LGFX_PORT_PANEL_WIDTH`
- `LGFX_PORT_PANEL_HEIGHT`
- `LGFX_PORT_LCD_CS_GPIO` (`43`)
- `LGFX_PORT_LCD_DC_GPIO` (`3`)
- `LGFX_PORT_LCD_RST_GPIO` (`2`)
- `LGFX_PORT_TOUCH_CS_GPIO` (`44`)

If `LGFX_PORT_TOUCH_CS_GPIO=-1`, touch support may still compile, but touch is not attached and `CAP_TOUCH` is not advertised.

Example:

```bash
idf.py \
  -DLGFX_PORT_PANEL_DRIVER=ILI9341_2 \
  -DLGFX_PORT_LCD_CS_GPIO=10 \
  -DLGFX_PORT_LCD_DC_GPIO=9 \
  -DLGFX_PORT_LCD_RST_GPIO=8 \
  -DLGFX_PORT_LCD_FREQ_WRITE_HZ="40*1000*1000" \
  -DLGFX_PORT_ENABLE_TOUCH=ON \
  -DLGFX_PORT_TOUCH_CS_GPIO=-1 \
  -DLGFX_PORT_ENABLE_JP_FONTS=OFF \
  build
```

## Elixir package notes

The root Elixir package provides `LGFXPort`, which wraps the native tuple protocol with convenience functions for:

- device lifecycle
- primitives
- text
- sprites and palette operations
- JPEG drawing
- RGB565 image transfer
- touch

Current wrapper notes:

- the native driver uses a singleton device model
- the wrapper currently assumes a single owning process per port in normal use
- host and driver should be updated together while the project remains pre-release

## Contributor note

Generated tables inside `docs/LGFX_PORT_PROTOCOL.md` are synchronized from source metadata.

```bash
elixir scripts/sync_lgfx_protocol_doc.exs
elixir scripts/sync_lgfx_protocol_doc.exs --check
```

## Status

This repository is under active development.

The protocol is usable, but the project is still pre-release. Until the first tagged release, the native driver and the Elixir wrapper should be updated together.
