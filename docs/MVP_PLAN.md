# AtomVM (ESP32-S3) + LovyanGFX Port Driver MVP Plan

- updated at: 20260208212029

## Goal

- Provide a minimal, reproducible MVP that:
  - builds as an ESP-IDF component inside AtomVM’s `src/platforms/esp32` build
  - exposes an AtomVM port driver named `lgfx_port`
  - allows an Elixir example app to:
    - open the driver via `open_port({:spawn_driver, ...})`
    - `ping`
    - `init_display(rotation)`
    - `fill_screen(rgb565)`
    - `draw_text(x, y, rgb565, text_size, text)`

## Non-goals

- Touch, SD card, sprites, partial blits, image decode, custom fonts, DMA tuning
- High-throughput rendering
- Scheduler-friendly asynchronous port execution (we will document the risk)

## Architecture

- `ports/lgfx_port.c`
  - AtomVM port driver
  - Implements a small **binary protocol** (opcode + payload)
  - Calls the “device layer” (`lgfx_device_*`) implemented in C++
- `src/lgfx_device.cpp`
  - LovyanGFX-backed device implementation
  - Owns the `lgfx::LGFX_Device` and panel/bus configuration
  - Uses a mutex to serialize LCD access
- `examples/elixir/*`
  - Minimal ExAtomVM project
  - Talks to the port driver via `:port.call/2` (request is a binary)

## Binary protocol

### Request

- `<<opcode::u8, payload::binary>>`

### Reply

- OK: `<<0x00, payload::binary>>`
- Error: `<<0x01, code::u8>>`

### Opcodes

- `0x01` PING
  - reply payload: ASCII `"PONG"`
- `0x10` INIT
  - payload: `rotation::u8` (0..7)
  - reply payload: `0x01` (dummy “ok” byte)
- `0x11` FILL_SCREEN
  - payload: `rgb565::u16be`
  - reply payload: `0x01`
- `0x12` DRAW_TEXT
  - payload:
    - `x::i16be`
    - `y::i16be`
    - `rgb565::u16be`
    - `text_size::u8`
    - `len::u16be`
    - `text::binary-size(len)` (ASCII recommended for MVP)
  - reply payload: `0x01`

## Wiring assumptions (MVP)

- Piyopiyo board mapping (from existing wiring notes / repo docs)
- Pins are currently hard-coded in `src/lgfx_device.cpp`.
- If board revisions differ (e.g., CS pin changes), we’ll add Kconfig or runtime opts later.

## Build + flash workflow

### Why custom firmware build is required

Because the port driver is compiled into AtomVM’s ESP-IDF build, we must
build/flash AtomVM firmware (not only the Elixir app).

### Steps (happy path)

1. Build + flash AtomVM firmware (with this component linked into AtomVM’s `esp32/components/`)
2. Build + flash Elixir AVM image

### Flash offset

This repo defaults to `flash_offset: 0x250000` for ESP32-S3.

## Runtime/scheduler note (important)

- The port native handler runs inside the AtomVM scheduler.
- LovyanGFX calls can take time (SPI transfers).
- For MVP we accept it, but a production version should offload work to a
  dedicated FreeRTOS task and reply asynchronously.

## Directory layout

- C / C++
  - `CMakeLists.txt`
  - `ports/include/lgfx_port.h`
  - `ports/lgfx_port.c`
  - `src/lgfx_device.h`
  - `src/lgfx_device.cpp`
  - `third_party/LovyanGFX/` (vendored or submodule)
- Elixir example
  - `examples/elixir/mix.exs`
  - `examples/elixir/lib/sample_app.ex`
  - `examples/elixir/lib/sample_app/port.ex`
- Helpers
  - `scripts/atomvm-esp32.sh`

## Acceptance checklist

- Firmware builds under AtomVM ESP32 platform (ESP-IDF)
- Device boots, serial logs show AtomVM boot
- Elixir example prints:
  - “Port opened: #Port<…>”
- `ping` returns OK + "PONG"
- Display visibly alternates background and shows incrementing `n=...`
