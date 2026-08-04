<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Elixir package

This repository provides the pre-release `atomlgfx` Elixir wrapper for AtomVM.
The current work is v3 because it implements wire protocol v3. No separate
package or API version has been assigned.

It is a wrapper around the native `lgfx_port` driver in this repository. The
package provides the Elixir-facing API, convenience helpers, and wrapper-side
normalization on top of the shared native protocol.

The current native protocol is v3:

- ordinary drawing and control calls are synchronous flat tuple requests
- common drawing commands should normally be grouped with `AtomLGFX.render/3`
- `AtomLGFX.render_lcd/3` and `AtomLGFX.render_sprite/4` make common target
  selection explicit while still using the same render-first path
- sprite drawing can use `AtomLGFX.render_sprite/4`, then `{:push_sprite, ...}`
  in an LCD render command list
- `AtomLGFX.render/3` compiles LovyanGFX-style commands to the existing
  `AtomLGFX.BinaryBatch` protocol
- `AtomLGFX.BinaryBatch` remains available for lower-level tests, diagnostics,
  and tuned frame scripts
- setup, queries, allocation, calibration, sprite lifecycle, palette creation,
  and touch operations stay on the ordinary call path

## Requirements

This package depends on AtomVM firmware that includes the native `lgfx_port`
driver.

The native driver and this Elixir wrapper are developed together in the same
repository and must come from the same Git commit.

## Installation

When depending on Git, pin the exact commit used to build the native driver.

```elixir
defp deps do
  [
    {:atomlgfx,
     git: "https://github.com/mnishiguchi/atomlgfx.git",
     ref: "FULL_GIT_COMMIT_SHA"}
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

:ok =
  AtomLGFX.render_lcd(port, [
    {:fill_screen, :black},
    {:set_text_font_preset, :jp},
    {:set_text_size, 2},
    {:set_text_color, :white, :black},
    {:set_text_datum, :top_left},
    {:draw_string, "こんにちは", 16, 16},
    {:draw_string, "日本語テキスト", 16, 56},
    :display
  ])
```

Adjust options and function calls to match your board and target device.

## Render batches

Use `AtomLGFX.render/3` first for ordinary drawing. `AtomLGFX.render_lcd/3` and
`AtomLGFX.render_sprite/4` are thin helpers that make the target explicit while
still using the same render-first path. Render commands normalize common color
inputs, including named colors, `{:rgb, r, g, b}`, `{:rgb565, value}`, and
`{:rgb888, value}`. Text datum commands accept LovyanGFX-style atoms such as
`:top_left`, `:middle_center`, and `:bottom_right`. Palette-backed sprite
primitives use explicit `{:index, n}` colors; render normalization inserts the
necessary low-level color-mode transitions. Use `AtomLGFX.BinaryBatch` directly
when a test, benchmark, or tuned example needs exact control of the wire-level
frame script.

```elixir
:ok = AtomLGFX.create_sprite(port, 120, 72, 16, 1)

:ok =
  AtomLGFX.render_sprite(port, 1, [
    {:clear, :navy},
    {:draw_rect, 0, 0, 120, 72, :white},
    {:set_cursor, 8, 8},
    {:set_text_color, :white},
    {:println, "sprite"}
  ])

:ok =
  AtomLGFX.render_lcd(port, [
    {:fill_screen, :black},
    {:push_sprite, 1, 24, 32},
    :display
  ])
```

When exact wire-level control matters, use `AtomLGFX.BinaryBatch` directly.
It is now a stable public facade. Lower-level implementation details live in
focused internal modules for encoding, submission, validation, and diagnostics.
Ordinary callers should continue using `AtomLGFX.render/3` or the public
`AtomLGFX.BinaryBatch` facade instead of calling those internal modules directly.


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

## High-load animation

The experimental retained render-program API was removed to reduce OOM risk.
Use the friendly render API for ordinary drawing bursts. For measured hot loops
such as MovingIcons, keep object state in Elixir and isolate the advanced
`AtomLGFX.BinaryBatch.push_rotate_zoom_list/2` path in an application-specific
renderer. This keeps the common API intuitive without hiding large buffers or
adding demo concepts to the protocol.

## Scope

This package owns:

- Elixir-facing API shape
- convenience helpers
- wrapper-side validation and normalization
- wrapper-local ergonomics

It does not define the native protocol contract or LovyanGFX device semantics.

For those, see:

- [Architecture](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/architecture.md)
- [Protocol](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/protocol.md)
- [ESP-IDF component guide](esp-idf-component.md)
- [Port layer](../lgfx_port/README.md)
- [Device adapter layer](../lgfx_device/README.md)

## Compatibility

The v3 public API is the current supported pre-release surface. Always use the
native component from the same Git commit as the Elixir wrapper; mixing
development revisions can produce `bad_proto`, `bad_op`, or capability
mismatches. The `0.1.0` value in `mix.exs` is required placeholder metadata,
not a published AtomLGFX version.
