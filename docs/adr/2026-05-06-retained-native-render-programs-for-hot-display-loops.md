<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 2026-05-06: Add retained native render scenes for hot display loops

## Status

Superseded by [ADR 2026-05-13: Reduce OOM risk in AtomLGFX v2](2026-05-13-reduce-oom-risk-in-v2.md).

## Context

`atomlgfx` v2 provides a compact call-based protocol for invoking LovyanGFX operations from AtomVM. It also provides `BinaryBatch` for submitting compact drawing transactions, including native frame commands such as the transformed-sprite PRZF frame command used by the `MovingIcons` example.

This design significantly reduces AtomVM-to-native call overhead compared with issuing one port call per drawing operation. It keeps the Elixir interface generic and keeps most ordinary drawing, setup, query, and control operations maintainable.

However, the `MovingIcons` workload remains performance-sensitive because each animation frame still requires AtomVM participation:

- Elixir updates object state.
- Elixir encodes the frame command.
- AtomVM submits one native command per frame.
- Native code decodes the command.
- Native code renders and presents the frame.
- Control returns to AtomVM before the next frame.

A comparable upstream LovyanGFX implementation reached approximately 10 fps on the same class of hardware by using a different execution model. In that implementation, the hot frame loop runs entirely inside a native FreeRTOS task. Object state, movement, strip rendering, sprite presentation, and display flushing are all handled natively without AtomVM participation per frame.

That means v2 has solved much of the per-operation protocol overhead, but it does not fully match the upstream native execution model for animation-heavy workloads.

## Decision

Extend the existing v2 protocol with retained native render scenes.

A retained native render scene is a native-side object that is configured from Elixir, then executed repeatedly by native code. Elixir remains responsible for setup, resource creation, initial data upload, lifecycle control, and optional low-frequency updates. Native code owns the hot display loop.

This is an additive protocol extension. It does not require a protocol version bump unless the implementation changes existing command semantics, existing payload layouts, response formats, or protocol negotiation behavior.

The initial target is `MovingIcons`-class performance, but the protocol should not expose a `moving_icons`-specific API. The public Elixir interface should remain generic.

## Design

The existing v2 protocol remains the immediate-call and binary-batch protocol. Retained native render scenes are added as an extension for workloads where the display frame loop must run natively.

The extension introduces retained native resources such as:

- display handles
- sprite or image handles
- instance buffers
- render scenes
- renderer runtime state
- renderer statistics

The initial render scene type should support striped transformed-sprite rendering.

Conceptually:

```text
Elixir
  creates display and sprite resources
  uploads source sprites or image data
  creates an instance buffer
  uploads initial object state
  creates a retained render scene
  starts or stops the native renderer
  reads renderer statistics

Native
  owns the hot frame loop
  owns per-frame object updates when configured
  renders strips
  pushes strips to the display
  calls display flush operations
  records frame statistics
```

The first useful render scene can be modeled as:

```text
sprite_transform
```

It should support:

- multiple source sprites
- compact object records
- per-object position
- velocity or movement parameters
- rotation
- zoom
- source index
- transparent color
- background color
- strip height
- display bounds
- renderer statistics

The renderer should support at least these lifecycle operations:

```text
create_instance_buffer
write_instances
create_render_scene
start_render_scene
stop_render_scene
read_render_scene_stats
destroy_render_scene
```

The initial native update policies should be intentionally small:

```text
none
bounce
```

`none` allows Elixir to own object updates and upload changes manually.

`bounce` allows native code to update object positions every frame, matching the common `MovingIcons` style workload.

## Protocol-version policy

Retained native render scenes are added without changing the protocol version.

The protocol version should be bumped only if a future change breaks compatibility, such as:

- changing existing opcode meanings
- changing existing payload layouts
- changing response formats incompatibly
- requiring new negotiation semantics
- making existing v2 clients unable to talk to the port

Adding new commands for retained resources is compatible with the existing v2 protocol model.

## Exclusive display ownership

A native render scene may need exclusive display ownership while running.

For maximum performance, the renderer may use settings similar to native LovyanGFX examples:

- long-lived write transaction
- no display lock
- non-shared bus
- retained source sprites
- retained strip sprites
- native task pinned to a core when appropriate

While an exclusive renderer is running, ordinary drawing calls should either be rejected or require the renderer to be stopped first.

This should be explicit in the API rather than hidden.

Example concept:

```elixir
AtomLGFX.RenderScene.start(port, scene, mode: :exclusive)
```

## Example Elixir shape

The exact API may change, but the intended shape is:

```elixir
{:ok, icon0} = AtomLGFX.create_sprite(display, 32, 32, color_depth: 16)
{:ok, icon1} = AtomLGFX.create_sprite(display, 32, 32, color_depth: 16)
{:ok, icon2} = AtomLGFX.create_sprite(display, 32, 32, color_depth: 16)

:ok = AtomLGFX.upload_sprite(icon0, icon0_pixels)
:ok = AtomLGFX.upload_sprite(icon1, icon1_pixels)
:ok = AtomLGFX.upload_sprite(icon2, icon2_pixels)

{:ok, instance_buffer} =
  AtomLGFX.create_instance_buffer(port,
    layout: :sprite_transform_2d,
    capacity: 50
  )

:ok = AtomLGFX.write_instances(port, instance_buffer, objects)

{:ok, scene} =
  AtomLGFX.RenderScene.create(port,
    renderer: :sprite_transform,
    instance_buffer: instance_buffer,
    sprites: [icon0, icon1, icon2],
    strip_height: 160,
    background_color: 0,
    transparent_color: 0,
    motion: :bounce,
    target_fps: :max
  )

:ok = AtomLGFX.RenderScene.start(port, scene, mode: :exclusive)

{:ok, stats} = AtomLGFX.RenderScene.stats(port, scene)

:ok = AtomLGFX.RenderScene.stop(port, scene)
```

## Consequences

This design gives the existing protocol two clear performance modes.

Immediate and batch commands answer:

```text
How can Elixir call LovyanGFX efficiently?
```

Retained native render scenes answer:

```text
How can Elixir configure a native LovyanGFX renderer that runs at hardware speed?
```

Benefits:

- avoids AtomVM participation on every animation frame
- avoids per-frame command encoding
- avoids per-frame port request and reply overhead
- allows native code to keep the display write path hot
- allows native object movement for simple animation policies
- keeps the Elixir-facing interface generic
- provides a realistic path toward upstream LovyanGFX-class frame rates
- preserves the existing v2 protocol version when implemented additively

Costs:

- adds a second protocol style inside v2
- introduces native retained state
- requires lifecycle management for native resources
- requires clear ownership rules while a renderer is running
- may require careful cleanup on process exit or port close
- may require renderer-specific statistics and diagnostics

## Implementation findings

The first retained native renderer confirmed the intended execution-model change. `MovingIcons` can run with native-owned object updates, native-owned strip rendering, and Elixir-side stats polling instead of Elixir submitting every frame.

Initial retained-render measurements reached approximately 6 to 7 fps with 50 objects and 160-pixel presentation strips. The observed frame time was dominated by presentation rather than object update or transformed-sprite drawing:

```text
frame_ms:   143..253
draw_ms:     25..56
present_ms: 104..195
update_ms:    0
```

This means the retained-render design removed the most important AtomVM per-frame overhead, but the remaining gap to approximately 10 fps is now mostly in the strip presentation path.

The retained renderer also exposed a scheduler concern. A continuously ready native render task can starve the CPU idle task and trigger the ESP-IDF task watchdog. Retained render loops must include a real scheduler delay or equivalent watchdog-friendly pacing. A plain `taskYIELD()` is not sufficient when the render task remains ready at a priority above idle.

## Alternatives considered

### Continue optimizing v2 immediate and binary-batch commands only

v2 can still be improved, especially around sprite memory placement, strip height, display locking, bus sharing, and PSRAM usage.

However, immediate and binary-batch commands still require AtomVM participation per frame. That makes them structurally different from the upstream native implementation that reached approximately 10 fps.

This remains useful, but it is unlikely to be the cleanest path for native-class animation performance.

### Add more BinaryBatch commands

Adding more batch operations would improve generic drawing transactions, but it would not remove per-frame Elixir encoding and submission.

This is useful for v2, but it does not address the main execution-model gap.

### Add a MovingIcons-specific native command

A dedicated native `MovingIcons` command would likely be fastest to implement, but it would make the API scene-specific.

That would conflict with the goal of keeping `atomlgfx` a generic LovyanGFX interface for AtomVM.

The retained render-scene design keeps the native hot loop while avoiding a demo-specific API.

### Bump the protocol version

Rejected for now.

The proposed change is additive. It can be implemented as new commands and retained resource types without changing existing command semantics, payload layouts, response formats, or negotiation behavior.

A protocol version bump should be reserved for breaking changes.

## Initial implementation scope

The first implementation should be intentionally narrow:

- one instance-buffer layout: `sprite_transform_2d`
- one render-scene renderer: `sprite_transform`
- two update policies: `none` and `bounce`
- one renderer mode: `exclusive`
- basic statistics reporting
- explicit start and stop operations

This is enough to validate whether `atomlgfx` can approach the upstream LovyanGFX `MovingIcons` frame rate while preserving a generic Elixir interface.

## Follow-up work

After the first benchmarkable version, continue with:

- configurable sprite memory policy
- configurable strip memory policy
- renderer task core affinity
- target FPS limiting
- asynchronous stats messages
- safe renderer interruption
- additional render-scene types
- non-exclusive renderer mode
- documentation comparing immediate, binary-batch, native-frame, and retained-render usage
- watchdog-friendly render-loop pacing
- presentation-path profiling
- SPI and DMA configuration comparison against upstream LovyanGFX examples
- PSRAM versus internal-RAM comparison for source sprites and presentation strips
- strip-height benchmarking
