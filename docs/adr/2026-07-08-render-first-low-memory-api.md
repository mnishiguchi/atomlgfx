# 2026-07-08 Render-first low-memory API

## Status

Accepted

## Context

`atomlgfx` is an AtomVM LovyanGFX wrapper. It currently has a working AtomVM port driver, a v3 low-memory protocol, and a packed `BinaryBatch` path for sending several render operations in one native transaction.

The package has also accumulated several implementation experiments around batch rendering, native animation, and retained render programs. Those experiments were useful, but the active package direction should now be simpler:

- common LovyanGFX tutorial operations should be easy from Elixir
- ordinary drawing should avoid one port round trip per primitive
- memory ownership should remain explicit
- native code should stay synchronous and boring
- MovingIcons should remain a smoke/stress example, not the core architecture

A separate Linux/Nerves package, `lovyangfx_elixir`, has shown that a friendly `render(commands)` API is productive. The important lesson is the render-first API shape, not the NIF boundary itself.

## Decision

`atomlgfx` will remain port-based internally, but become render-first externally.

The main public drawing API is:

```elixir
AtomLGFX.render(port, [
  {:fill_screen, :black},
  {:set_text_color, :white},
  {:set_cursor, 10, 10},
  {:println, "Hello AtomLGFX"},
  {:draw_line, 0, 40, 200, 40, :red},
  :display
])
```

The implementation path is:

```text
AtomLGFX.render/3
  -> AtomLGFX.Command.normalize/2
  -> AtomLGFX.RenderBatch.encode/2
  -> AtomLGFX.BinaryBatch
  -> AtomLGFX.submit_binary_batch/2
  -> lgfx_port
  -> LovyanGFX
```

`AtomLGFX.BinaryBatch` remains available, but it becomes a lower-level API for tests, diagnostics, and carefully tuned examples. The recommended beginner-facing path is `AtomLGFX.render/3`.

## Consequences

This gives us:

- a small LovyanGFX-like Elixir API
- one native boundary crossing for common frame scripts
- less flicker than repeated direct primitive calls
- no NIF rewrite
- no retained native render program
- no hidden double-buffering
- no background renderer
- a clear place for command validation and color normalization

This does not solve every performance problem. Payload-heavy operations such as JPEG rendering and raw image upload should remain explicit APIs until their memory behavior is clear enough to batch safely.

## Implementation policy

The first implementation should be intentionally modest:

- add `AtomLGFX.Command`
- add an internal render-batch encoder/submission bridge
- add `AtomLGFX.render/3`
- support common primitive, text, sprite-push, and display commands
- keep packed multi-instance animation commands in the lower-level `BinaryBatch` API
- keep all native code unchanged
- add host-side tests around command normalization and batch encoding

Native cleanup can happen later after the public API settles.

## Non-goals

- Do not rewrite `atomlgfx` as NIF.
- Do not add a scene graph.
- Do not add a retained native render loop.
- Do not add hidden display buffers.
- Do not make the core API or native protocol MovingIcons-specific.
- Do not chase full LovyanGFX API coverage before common tutorial operations feel good.

## MovingIcons policy

MovingIcons stays useful as two examples:

- low-memory smoke test
- optional stress benchmark

It should use the same public APIs as ordinary examples where practical. The
ESP32-S3 hardware run measured roughly 0.2 FPS for caller-owned strip repainting
and 4-5 FPS for a compact transformed-sprite list. MovingIcons therefore keeps
an example-local renderer over the existing generic advanced batch primitive;
it does not add MovingIcons concepts to the friendly API or native protocol.
The selected renderer dynamically bounds each previous transform, immediately
redraws isolated icons, and groups intersecting icons so cleanup cannot erase an
already-rendered neighbor. This reduced visible flicker without a frame buffer.
