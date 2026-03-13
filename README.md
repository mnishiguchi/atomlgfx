# LovyanGFX AtomVM Port Driver

`atomlgfx` is an ESP-IDF component that exposes LovyanGFX to AtomVM Elixir through a synchronous tuple-based port protocol.

## Documentation

- [Protocol specification](docs/LGFX_PORT_PROTOCOL.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Worker model](docs/WORKER_MODEL.md)

## Quickstart

```bash
cd atomlgfx

# Make sure the LovyanGFX submodule is present
git submodule sync --recursive
git submodule update --init --recursive

# Build and flash AtomVM firmware (includes this port driver)
./scripts/atomvm_esp32.exs install --target esp32s3 --port /dev/ttyACM0

# Build and flash the Elixir example app
cd examples/elixir
mix deps.get
mix do clean, atomvm.esp32.flash --port /dev/ttyACM0

# Monitor serial output
cd ../..
./scripts/atomvm_esp32.exs monitor --port /dev/ttyACM0
```

Before flashing the example app, adjust `LGFXPort.open(...)` in `examples/elixir/lib/lgfx_port.ex` if your board uses different panel settings or pin assignments.

## Protocol at a glance

Requests and responses use this shape:

```erlang
{lgfx, ProtoVer, Op, Target, Flags, ...}
{ok, Result}
{error, Reason}
```

See `docs/LGFX_PORT_PROTOCOL.md` for the full operation matrix, validation rules, capability bits, and operation semantics.

## Configuration

LovyanGFX is vendored as a git submodule under `third_party/LovyanGFX` and compiled as part of this component.

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

## Contributor note

Generated tables inside `docs/LGFX_PORT_PROTOCOL.md` are synchronized from source metadata.

```bash
elixir scripts/sync_lgfx_protocol_doc.exs
elixir scripts/sync_lgfx_protocol_doc.exs --check
```

## Status

This repository is under active development.

The protocol is usable, but the project is still pre-release. Until the first tagged release, host and driver should be updated together.
