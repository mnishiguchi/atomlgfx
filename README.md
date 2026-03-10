# LovyanGFX AtomVM Port Driver

ESP-IDF component that exposes LovyanGFX to AtomVM Elixir through an Erlang term (tuple) port protocol.

## Documentation

- [Protocol specification](docs/LGFX_PORT_PROTOCOL.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Worker model](docs/WORKER_MODEL.md)

## Quickstart

```bash
git clone https://github.com/mnishiguchi/atomlgfx.git
cd atomlgfx

# Before compiling, make sure the LovyanGFX submodule is present
git submodule sync --recursive
git submodule update --init --recursive

# Build + flash AtomVM firmware (includes the port driver)
./scripts/atomvm_esp32.exs install --target esp32s3 --port /dev/ttyACM0

# Build + flash the Elixir example app
cd examples/elixir
mix deps.get
mix do clean, atomvm.esp32.flash --port /dev/ttyACM0

# Monitor serial output
cd ../..
./scripts/atomvm_esp32.exs monitor --port /dev/ttyACM0
```

Before flashing the example app, adjust its `LGFXPort.open(...)` config in
`examples/elixir/lib/lgfx_port.ex` if your board uses a different panel profile
or pin assignment.

## Protocol overview

The wire contract is synchronous and tuple-based.

- Request
  - `{lgfx, ProtoVer, Op, Target, Flags, ...}`

- Response
  - `{ok, Result}`
  - `{error, Reason}`

Current sprite protocol surface:

- `createSprite`
- `deleteSprite`
- `setPivot`
- `pushSprite`
- `pushRotateZoom`

See `docs/LGFX_PORT_PROTOCOL.md` for the full operation matrix and rules.

## Configuration

LovyanGFX is vendored as a git submodule under `third_party/LovyanGFX` and compiled into this ESP-IDF component.

Build defaults are driven by CMake cache variables set by the parent ESP-IDF
project or via `idf.py -D...`.

Some runtime values may also be overridden later at port-open time from the
host. When open-time keys are omitted, the build defaults are used.

Common build-default options:

- `LGFX_PORT_ENABLE_JP_FONTS` (default `ON`)
- `LGFX_PORT_ENABLE_TOUCH` (default `ON`)
- `LGFX_PORT_PANEL_DRIVER` (default `ILI9488`; supported: `ILI9488`, `ILI9341`, `ILI9341_2`)
- `LGFX_PORT_PANEL_WIDTH` (default depends on the selected build-default panel profile)
- `LGFX_PORT_PANEL_HEIGHT` (default depends on the selected build-default panel profile)
- `LGFX_PORT_LCD_CS_GPIO` (default `43`)
- `LGFX_PORT_LCD_DC_GPIO` (default `3`)
- `LGFX_PORT_LCD_RST_GPIO` (default `2`)
- `LGFX_PORT_TOUCH_CS_GPIO` (default `44`)

If `LGFX_PORT_TOUCH_CS_GPIO=-1`, touch code may still compile when enabled, but touch is not attached and `CAP_TOUCH` is not advertised.

Example override:

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

## Repository layout

- `include/lgfx_port/`
  - Public headers for this ESP-IDF component

- `lgfx_port/include_internal/lgfx_port/`
  - Internal protocol metadata, validation helpers, and worker definitions
  - Includes `ops.def` and `worker_jobs.def`

- `lgfx_port/`
  - Port-facing implementation
  - Mailbox drain, request decode, validation, dispatch, reply handling, worker bridge

- `src/`
  - Device-facing LovyanGFX adapter layer

- `docs/`
  - Protocol, architecture, and worker model documentation

- `examples/elixir/`
  - Example Elixir client

## Protocol doc sync

Generated tables inside `docs/LGFX_PORT_PROTOCOL.md` are synchronized from source metadata.

```bash
elixir scripts/sync_lgfx_protocol_doc.exs
elixir scripts/sync_lgfx_protocol_doc.exs --check
```

## Status

This repository is under active development.

The protocol is usable, but the project is still pre-release. Until the first tagged release, host and driver should be updated together.
