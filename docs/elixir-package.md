<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Elixir package

This repository provides an Elixir package named `AtomLGFX` for AtomVM.

It is a wrapper around the native `lgfx_port` driver in this repository. The
package provides the Elixir-facing API, convenience helpers, and wrapper-side
normalization on top of the shared native protocol.

The current native protocol is v2:

- ordinary drawing and control calls are synchronous tuple requests
- binary batches are explicit binary frame scripts built with
  `AtomLGFX.BinaryBatch` and submitted with `AtomLGFX.submit_binary_batch/2`
- retained native render programs allow Elixir to configure native-owned hot
  display loops through `AtomLGFX.create_object_buffer/2`,
  `AtomLGFX.write_object_buffer/3`, and `AtomLGFX.RenderProgram`
- render-time text, JPEG, image payload, sprite push, and palette color
  commands may be encoded in binary batches when supported by
  `AtomLGFX.BinaryBatch` builders
- setup, queries, allocation, calibration, sprite lifecycle, palette creation,
  and touch operations stay on the ordinary call path

## Requirements

This package depends on AtomVM firmware that includes the native `lgfx_port`
driver.

The native driver and this Elixir package are developed together in the same
repository and should be kept in sync, especially while the project remains
pre-release.

## Installation

This project is still under development. If you depend on it directly from Git,
choose the branch or revision you intend to track.

```elixir
defp deps do
  [
    {:atomlgfx, git: "https://github.com/mnishiguchi/atomlgfx.git", branch: "main"}
  ]
end
```

Then fetch dependencies.

```bash
mix deps.get
```

## Basic usage

```elixir
{:ok, port} = AtomLGFX.open(panel_driver: :ili9488, width: 320, height: 480)

:ok = AtomLGFX.init(port)
:ok = AtomLGFX.display(port)

:ok = AtomLGFX.fill_screen(port, 0x0000)
:ok = AtomLGFX.set_text_font_preset(port, :jp)
:ok = AtomLGFX.set_text_size(port, 2)
:ok = AtomLGFX.set_text_color(port, 0xFFFF, 0x0000)

:ok = AtomLGFX.draw_string(port, 16, 16, "こんにちは")
:ok = AtomLGFX.draw_string(port, 16, 56, "日本語テキスト")

:ok = AtomLGFX.display(port)
```

Adjust options and function calls to match your board and target device.

## Render batches

Use `AtomLGFX.BinaryBatch` when a frame should cross the AtomVM/native boundary
as one render script.

```elixir
frame = [
  AtomLGFX.BinaryBatch.target(0),
  AtomLGFX.BinaryBatch.fill_screen(0x0000),
  AtomLGFX.BinaryBatch.draw_line(0, 0, 319, 239, 0xFFFF),
  AtomLGFX.BinaryBatch.fill_rect(20, 20, 80, 40, 0x07E0),
  AtomLGFX.BinaryBatch.display()
]

:ok = AtomLGFX.BinaryBatch.render(port, frame)
```

Successful submission means the script was decoded and executed synchronously.
Malformed bytes fail as protocol errors; unsupported render commands are
rejected.

## Retained render programs

Use retained render programs when the hot display loop should stay native after
setup.

Typical shape:

```elixir
{:ok, object_buffer} =
  AtomLGFX.create_object_buffer(port,
    layout: :sprite_transform_2d,
    capacity: 50
  )

:ok = AtomLGFX.write_object_buffer(port, object_buffer, objects)

{:ok, program} =
  AtomLGFX.RenderProgram.create(port,
    type: :striped_sprite_transform,
    object_buffer: object_buffer,
    sources: source_handles,
    strip_height: 160,
    background_color: 0x0000,
    transparent_color: 0x0000,
    update: :bounce
  )

:ok = AtomLGFX.RenderProgram.start(port, program, mode: :exclusive)
{:ok, stats} = AtomLGFX.RenderProgram.stats(port, program)
```

While an exclusive retained renderer is running, ordinary drawing operations are
rejected until the program is stopped or destroyed.

## Scope

This package owns:

- Elixir-facing API shape
- convenience helpers
- wrapper-side validation and normalization
- wrapper-local ergonomics

It does not define the native protocol contract or LovyanGFX device semantics.

For those, see:

- [Architecture](architecture.md)
- [Protocol](protocol.md)
- [ESP-IDF component guide](esp-idf-component.md)
- [Port layer](../lgfx_port/README.md)
- [Device adapter layer](../lgfx_device/README.md)

## Status

The project is usable but still pre-release.

Until the first tagged release, update the Elixir package and the native driver
together.
