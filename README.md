<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# atomlgfx

`atomlgfx` is a LovyanGFX integration for AtomVM on ESP32-class boards.

This repository contains two closely related deliverables:

- an ESP-IDF component that provides the native `lgfx_port` AtomVM port driver
- an Elixir package that provides the `AtomLGFX` wrapper module for that driver

Both layers share wire protocol v3 and evolve together. The current pre-release
work is v3 because it implements that contract; there is no separate package or
API version.

## Current API direction

The preferred drawing API is now render-first. Common LovyanGFX-style drawing
commands should be grouped into one render transaction:

```elixir
AtomLGFX.render_lcd(port, [
  {:fill_screen, :black},
  {:set_text_color, :white},
  {:set_text_datum, :top_left},
  {:set_cursor, 10, 10},
  {:println, "Hello AtomLGFX"},
  {:draw_line, 0, 40, 200, 40, {:rgb, 255, 0, 0}},
  :display
])
```

Internally this compiles to the existing low-memory `BinaryBatch` protocol and
submits one request to the AtomVM port driver. Use `AtomLGFX.render_lcd/3` and
`AtomLGFX.render_sprite/4` when the target should be obvious at the call site.
Direct APIs remain available for setup, lifecycle, touch, queries, sprite
allocation, JPEG drawing, and raw image upload. Render commands accept named
colors, RGB tuples such as `{:rgb, 255, 0, 0}`, common text datum atoms such
as `:top_left`, `:middle_center`, and `:bottom_right`, and sprite commands such
as `{:push_sprite, sprite_target, x, y}`. Palette-backed sprite primitives use
explicit colors such as `{:index, 2}`; the friendly render API manages the
low-level color mode automatically. The example app now includes a
render-first `:sprites` mode for drawing into a caller-owned sprite and pushing
it to the LCD. The `:moving_icons` demo deliberately lives outside the friendly
command API. Its example-local renderer uses the advanced transformed-sprite
list command, clears only previous icon bounds, and submits one batch per frame.
Isolated icons are erased and redrawn immediately; intersecting icons are
grouped so their cleanup regions cannot cut into one another.

Familiar scalar helpers such as `draw_pixel`, `draw_line`, and `fill_rect`
remain available for occasional direct calls. Use a render transaction for
loops so repeated pixels or primitives do not each pay a port-call boundary.

See [ADR 2026-07-08](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/adr/2026-07-08-render-first-low-memory-api.md) for
the current architecture decision.

## What to read

- [Changelog](https://github.com/mnishiguchi/atomlgfx/blob/main/CHANGELOG.md)
- [Migration guide](docs/migration-to-v3.md)
- [ESP-IDF component guide](docs/esp-idf-component.md)
- [Elixir package guide](docs/elixir-package.md)
- [Architecture](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/architecture.md)
- [Protocol](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/protocol.md)
- [Pre-release validation](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/pre-release-validation.md)

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

The v3 API is the current supported pre-release surface. The native driver and
Elixir wrapper must come from the same Git revision because both layers
implement wire protocol v3 together.

No package release version has been assigned. The `0.1.0` value required by Mix
is placeholder metadata and does not identify a published AtomLGFX release.
