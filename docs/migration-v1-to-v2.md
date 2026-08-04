<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Migrating from v1 to v2

> Historical document: this describes the superseded v2 API and is retained for
> repository history. For current applications, use the
> [v3 migration guide](migration-to-v3.md).

This guide summarizes the practical changes needed when moving code from the v1
`v1` branch to the v2 `main` branch.

v2 keeps the same project goal: an AtomVM-facing LovyanGFX wrapper for
ESP32-class boards. The main change is the protocol and batching model.

## Summary

v1 used a tuple-based protocol with a higher-level tuple/list batch path.

v2 uses:

- synchronous tuple calls for ordinary operations
- canonical `snake_case` operation names for public low-level Elixir calls
- an explicit binary batch path for grouped frame submission
- generated metadata to keep the native protocol, Elixir wrapper, and docs
  synchronized

The native component and Elixir package should be updated together.

## Native component and Elixir wrapper compatibility

Update the ESP-IDF component and the Elixir package at the same time.

Do not mix:

- v1 native driver with v2 Elixir wrapper
- v2 native driver with v1 Elixir wrapper

The protocol surface, operation metadata, and capability bits are designed to
move together during the pre-release period.

## Ordinary calls

Most normal drawing and control calls should continue to use the public
`AtomLGFX` API.

Typical v2 usage:

```elixir
{:ok, port} = AtomLGFX.open(panel_driver: :ili9488, width: 320, height: 480)

:ok = AtomLGFX.ping(port)
:ok = AtomLGFX.init(port)

:ok = AtomLGFX.fill_screen(port, 0x0000)
:ok = AtomLGFX.set_text_font_preset(port, :jp)
:ok = AtomLGFX.set_text_size(port, 2)
:ok = AtomLGFX.set_text_color(port, 0xFFFF, 0x0000)
:ok = AtomLGFX.draw_string(port, 16, 16, "こんにちは")
:ok = AtomLGFX.display(port)
```

Low-level public Elixir calls use canonical `snake_case` operation atoms.

For example:

```elixir
AtomLGFX.call(port, :fill_rect, [20, 20, 80, 40, 0x07E0])
```

LovyanGFX-style `camelCase` atoms such as `:fillRect` are not supported in v2.
Use canonical `snake_case` names in application and raw-call code.

## Batch migration

The biggest source-level change is batch usage.

### v1 style

v1 code may look like this:

```elixir
batch =
  AtomLGFX.batch()
  |> AtomLGFX.Batch.add(AtomLGFX.Batch.Command.clear(0x0000))
  |> AtomLGFX.Batch.add(AtomLGFX.Batch.Command.draw_rect(8, 8, 120, 80, 0xFFFF))
  |> AtomLGFX.Batch.add(AtomLGFX.Batch.Command.line(8, 8, 127, 87, 0x07E0))

{:ok, _} = AtomLGFX.submit_batch(port, batch)
```

### v2 style

In v2, build one explicit render script with `AtomLGFX.BinaryBatch`.

```elixir
frame = [
    AtomLGFX.BinaryBatch.target(0),
    AtomLGFX.BinaryBatch.fill_screen(0x0000),
    AtomLGFX.BinaryBatch.draw_rect(8, 8, 120, 80, 0xFFFF),
    AtomLGFX.BinaryBatch.draw_line(8, 8, 127, 87, 0x07E0),
    AtomLGFX.BinaryBatch.display()
  ]

:ok = AtomLGFX.BinaryBatch.render(port, frame)
```

## Batch scope

The v2 binary batch path is intentionally explicit.

It is suitable for building one compact frame script that executes
synchronously.

Binary batches may include render-time commands such as text, sprite
push/rotate/zoom commands, and palette color writes when represented by
`AtomLGFX.BinaryBatch` builders. JPEG drawing and RGB565 image upload remain on
the ordinary API because their payloads are intentionally excluded from the
binary-batch format.

Keep these on the ordinary call path:

- setup, queries, allocation, and calibration
- sprite lifecycle operations
- palette creation
- touch operations

This keeps the hot path explicit while avoiding accidental expansion into a
general-purpose deferred LovyanGFX API.

## Capability check

Use capability discovery before relying on optional behavior.

```elixir
{:ok, true} = AtomLGFX.supports_batch?(port)
{:ok, max_bytes} = AtomLGFX.max_binary_bytes(port)
```

If batch support is unavailable, fall back to ordinary drawing calls.

## Error handling

v2 ordinary operations return immediate success or failure.

Typical return values are:

- `:ok`
- `{:ok, value}`
- `{:error, reason}`

For binary batches:

- success means the frame script was decoded and executed synchronously
- malformed render commands return protocol errors such as `bad_args`
- unsupported render command opcodes return `bad_op`

Treat error reasons as protocol-level values and avoid matching overly detailed
internal forms unless the protocol document explicitly defines them.

## Migration checklist

- Update the native ESP-IDF component to v2.
- Replace `AtomLGFX.batch/0`, `AtomLGFX.Batch`, and `AtomLGFX.submit_batch/2`.
- Use `AtomLGFX.BinaryBatch` frame builders plus `AtomLGFX.submit_binary_batch/2`
  or `AtomLGFX.BinaryBatch.render/2`.
- Use public `AtomLGFX` wrappers for ordinary operations.
- Use `snake_case` atoms for low-level `AtomLGFX.call/4`.
- Keep setup, queries, allocation, calibration, sprite lifecycle, palette
  creation, and touch operations outside binary batches.
- Run protocol smoke tests on hardware.
- Run performance smoke tests for rendering paths that matter to the
  application.
