<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Migrating to the v3 API

The current pre-release work is v3 because it implements wire protocol v3.
There is no separate package or API version. The wire contract is the maintained
versioned boundary between the Elixir wrapper and native driver.

## Update both deliverables

Use the Elixir wrapper and native ESP-IDF component from the same Git commit.
Do not mix the current wrapper with a v1 or v2 native driver, or with a native
driver from a different development revision.

## Prefer render-first drawing

Group ordinary drawing commands into one transaction:

```elixir
:ok =
  AtomLGFX.render_lcd(port, [
    {:fill_screen, :black},
    {:set_text_color, :white},
    {:set_text_datum, :top_left},
    {:set_cursor, 8, 8},
    {:println, "Hello AtomLGFX"},
    {:draw_line, 0, 32, 160, 32, :red},
    :display
  ])
```

Use `AtomLGFX.render_sprite/4` when drawing into a sprite. Direct functions
remain appropriate for initialization, queries, touch, sprite lifecycle, JPEG
drawing, raw RGB565 upload, and occasional individual operations.

## Replace old batch APIs

Remove uses of the former tuple/list batch builder, its batch module, the old
batch submission function, and retained native render programs.

Use the friendly `AtomLGFX.render/3`, `render_lcd/3`, and `render_sprite/4`
functions first. `AtomLGFX.BinaryBatch` remains available as an advanced API for
diagnostics and measured hot loops.

## Use canonical Elixir names

Public function and low-level operation names use `snake_case`. LovyanGFX C++
names such as `fillRect` map to Elixir names such as `fill_rect`.

Render commands accept friendly colors including `:black`, `:white`,
`{:rgb, r, g, b}`, and `{:index, n}` for palette-backed sprites. Text datum
commands accept atoms such as `:top_left`, `:middle_center`, and
`:bottom_right`.

## Keep large payloads explicit

JPEG and RGB565 image data are deliberately not embedded in render batches.
Continue using `AtomLGFX.draw_jpg/5` or `/11` and
`AtomLGFX.push_image_rgb565/8` so allocation and payload ownership remain
visible.

## Migration checklist

- Pin the Elixir dependency and native component to the same Git commit.
- Replace removed tuple/list and retained-render APIs.
- Group ordinary primitives and text with the render-first helpers.
- Keep setup, queries, allocation, touch, JPEG, and raw image upload direct.
- Use canonical `snake_case` names.
- Check optional capabilities before using sprites, palettes, touch, or binary
  batches.
- Run `SAMPLE_APP_MODE=all` on the target device after upgrading.
