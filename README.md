# LovyanGFX AtomVM Port Driver

ESP-IDF component that exposes a **LovyanGFX** display driver to **AtomVM Elixir** via an **Erlang term (tuple) port protocol**.

- Host and driver communicate using Erlang terms (tuples / atoms / integers / binaries).
- Large payloads (pixel data, text strings) use binaries for throughput.
- The protocol surface is documented and kept aligned with source metadata.

## Documentation

- [Protocol specification](docs/LGFX_PORT_PROTOCOL.md)
- [Architecture](docs/ARCHITECTURE.md)

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

## LovyanGFX configuration

LovyanGFX is vendored as a git submodule under `third_party/LovyanGFX` and compiled into this ESP-IDF component.

Configuration is compile-time and is driven by CMake cache variables (set from the parent project or via `idf.py -D...`).

- Build knobs
  - `LGFX_PORT_ENABLE_JP_FONTS` (default `ON`)
    - When `ON`, compiles a custom minimal JP font subset (`LGFX_PORT_JP_FONT_CPP`) and enables `jp_*` presets.

  - `LGFX_PORT_ENABLE_TOUCH` (default `ON`)
    - When `ON`, compiles in `Touch_XPT2046` support.
    - Touch capability is advertised only when touch is attached (see below).

- Wiring defaults (override as needed)
  - SPI
    - `LGFX_PORT_SPI_SCLK_GPIO` (default `7`)
    - `LGFX_PORT_SPI_MOSI_GPIO` (default `9`)
    - `LGFX_PORT_SPI_MISO_GPIO` (default `8`)

  - LCD
    - `LGFX_PORT_LCD_SPI_HOST` (default `SPI2_HOST`)
    - `LGFX_PORT_LCD_CS_GPIO` (default `43`)
    - `LGFX_PORT_LCD_DC_GPIO` (default `3`)
    - `LGFX_PORT_LCD_RST_GPIO` (default `2`)

  - Touch (XPT2046, SPI)
    - `LGFX_PORT_TOUCH_CS_GPIO` (default `44`)
    - `LGFX_PORT_TOUCH_IRQ_GPIO` (default `-1`)
    - `LGFX_PORT_TOUCH_SPI_HOST` (default: **same as** `LGFX_PORT_LCD_SPI_HOST`)
    - `LGFX_PORT_TOUCH_SPI_FREQ_HZ` (default `1000000u`)
    - `LGFX_PORT_TOUCH_OFFSET_ROTATION` (default: **same as** `LGFX_PORT_LCD_OFFSET_ROTATION`)
    - If `LGFX_PORT_TOUCH_CS_GPIO=-1`, touch code may still compile (when enabled) but touch is not attached and `CAP_TOUCH` is not advertised.

- Extra LovyanGFX config knobs (override as needed)
  - Panel geometry
    - `LGFX_PORT_PANEL_WIDTH` (default `320`)
    - `LGFX_PORT_PANEL_HEIGHT` (default `480`)

  - `lgfx::Bus_SPI` knobs
    - `LGFX_PORT_LCD_SPI_MODE` (default `0`)
    - `LGFX_PORT_LCD_FREQ_WRITE_HZ` (default `20*1000*1000`)
    - `LGFX_PORT_LCD_FREQ_READ_HZ` (default `10*1000*1000`)
    - `LGFX_PORT_LCD_DMA_CHANNEL` (default `SPI_DMA_CH_AUTO`)
    - `LGFX_PORT_LCD_SPI_3WIRE` (default `OFF`)
    - `LGFX_PORT_LCD_USE_LOCK` (default `ON`)

  - `lgfx::Panel_ILI9488` knobs
    - `LGFX_PORT_LCD_PIN_BUSY` (default `-1`)
    - `LGFX_PORT_LCD_OFFSET_X` (default `0`)
    - `LGFX_PORT_LCD_OFFSET_Y` (default `0`)
    - `LGFX_PORT_LCD_OFFSET_ROTATION` (default `0`)
    - `LGFX_PORT_LCD_DUMMY_READ_PIXEL` (default `8`)
    - `LGFX_PORT_LCD_DUMMY_READ_BITS` (default `1`)
    - `LGFX_PORT_LCD_READABLE` (default `OFF`)
    - `LGFX_PORT_LCD_INVERT` (default `OFF`)
    - `LGFX_PORT_LCD_RGB_ORDER` (default `OFF`)
    - `LGFX_PORT_LCD_DLEN_16BIT` (default `OFF`)
    - `LGFX_PORT_LCD_BUS_SHARED` (default `ON`)

  - Touch knobs
    - `LGFX_PORT_TOUCH_BUS_SHARED` (default `ON`)

- Custom JP font subset
  - `LGFX_PORT_JP_FONT_CPP` (default `src/fonts/generated/ui_font_ja_16_min.cpp`)
  - This repo intentionally does not compile LovyanGFX’s large IPA font blob.

Example override (from your ESP-IDF project build):

```bash
idf.py \
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

- `lgfx_port/`
  - Port driver implementation (protocol decode/validation/dispatch + worker integration)

- `src/`
  - Device-facing LovyanGFX adapter layer (protocol-agnostic)

- `docs/`
  - Protocol and architecture documentation

- `examples/elixir/`
  - Example Elixir client for exercising the port driver

## Protocol overview

The wire contract is tuple-based and synchronous (`port:call` style).

- Request
  - `{lgfx, ProtoVer, Op, Target, Flags, ...}`

- Response
  - `{ok, Result}` or `{error, Reason}`

See the full contract and operation matrix in:

- [docs/LGFX_PORT_PROTOCOL.md](docs/LGFX_PORT_PROTOCOL.md)

## Protocol doc sync

Generated tables inside `docs/LGFX_PORT_PROTOCOL.md` are synchronized from source metadata.

- Script
  - `scripts/sync_lgfx_protocol_doc.exs`

- Typical usage
  - `elixir scripts/sync_lgfx_protocol_doc.exs`
  - `elixir scripts/sync_lgfx_protocol_doc.exs --check`

## Status

This repository is under active development.

The term protocol is usable and intended to be the stable integration boundary for host-side Elixir code.
