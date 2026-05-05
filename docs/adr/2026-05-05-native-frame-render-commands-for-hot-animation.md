<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 2026-05-05: Allow native frame render commands for hot animation loops

## Status

Accepted

## Context

`atomlgfx` v2 replaces the broad v1-style native wrapper surface with a smaller call-based protocol around LovyanGFX operations. This keeps ordinary drawing, setup, query, and control operations maintainable.

For animation-heavy workloads, v2 introduced `BinaryBatch` so Elixir can submit one compact frame script instead of many ordinary port calls. This reduces AtomVM/native crossing overhead and gives animation code an explicit render-transaction path.

However, the MovingIcons workload showed that reducing port-call overhead is not enough by itself. The upstream LovyanGFX `MovingIcons` example keeps the core frame-render loop entirely in C++:

```cpp
for (int_fast16_t y = 0; y < lcd_height; y += sprite_height) {
  flip = flip ? 0 : 1;
  sprites[flip].clear();

  for (size_t i = 0; i != obj_count; i++) {
    a = &objects[i];
    icons[a->img].pushRotateZoom(&sprites[flip], a->x, a->y - y, a->r, a->z, a->z, 0);
  }

  sprites[flip].pushSprite(&lcd, 0, y);
}

lcd.display();
```

The primitive v2 render-batch path can express equivalent behavior, but it does so through a more generic command stream:

```text
begin strip
clear strip
push rotate zoom list
present strip
repeat for each strip
display
```

This preserves protocol flexibility, but it still leaves extra work inside the native hot path:

- render command dispatch
- target resolution
- sprite handle lookup
- command-local validation
- fixed-point conversion
- optional culling
- presentation-strip state transitions

These costs are small individually, but they happen inside the frame-critical path. For workloads like MovingIcons, the goal is to get closer to the upstream native LovyanGFX loop while still allowing Elixir to own animation state.

Early benchmarking confirmed this direction:

- A native transformed-sprite frame command reduced MovingIcons frame time from roughly `746–815 ms` to roughly `628–629 ms`.
- Raising the LCD write clock to `40 MHz` with DMA enabled reduced the frame time further to roughly `547–565 ms`.
- Directly encoding the native transform-frame command from the MovingIcons object list reduced Elixir-side frame construction and brought the measured frame time to roughly `484 ms`.
- A trace run showed native execution dominated by strip presentation, with `frame_present_us` significantly larger than draw and clear time.
- Full-height native strips reduced strip count from `2` to `1` but did not improve total frame time.
- Per-object dirty-region presentation was rejected after causing instability and increasing presentation complexity.

These findings support native frame render commands as the right v2 hot-animation direction, while also showing that further work should focus on presentation efficiency and Elixir-side frame construction, not on ordinary batch dispatch or write transaction overhead.

## Decision

Allow `BinaryBatch` to include native frame render commands for selected hot animation patterns.

A native frame render command may represent a complete reusable rendering pattern, not just one LovyanGFX primitive.

For MovingIcons-style workloads, Elixir sends compact frame state such as:

```text
source sprite id
x
y
angle
zoom
```

Native code owns the full frame render loop:

```text
for each native presentation strip:
  clear strip
  for each object:
    push transformed source sprite to strip
  present strip

display
```

The architectural boundary is:

```text
Elixir owns animation state.
Native owns hot frame rendering.
LovyanGFX owns drawing.
```

Native frame render commands must remain generic. They must not encode application-specific concepts such as `MovingIcons`, `StackChan`, demo object names, physics, or scene rules.

Acceptable command names are generic, for example:

```text
push_transform_list_frame
render_transform_list_strips
```

Unacceptable command names are demo-specific, for example:

```text
moving_icons_frame
stackchan_face_frame
```

## Relationship to primitive BinaryBatch operations

Native frame render commands do not replace primitive `BinaryBatch` operations.

The expected layering is:

```text
ordinary calls
  setup, queries, allocation, calibration, low-frequency control

primitive BinaryBatch operations
  generic render scripts and reusable LovyanGFX-like drawing operations

native frame render commands
  compact frame-state payloads executed by tight native loops
```

Primitive `BinaryBatch` operations remain the standard way to express ordinary render scripts, mixed drawing work, smoke tests, text overlays, scalar primitives, sprite pushes, and presentation-strip experiments.

Native frame render commands are an additional hot-path option for repeated animation patterns where the performance-critical structure is the loop itself, not only the individual drawing primitive.

A native frame command should be added only when primitive batch operations are a measured bottleneck and the command remains reusable beyond a single demo.

## Opcode-space policy

The render-private opcode space is intentionally small.

To avoid spending scarce private opcode values on every new frame-level command, `0xFF` may be used as an extended render opcode. Extended render commands should use a sub-opcode inside the extended command payload.

The initial extended commands are:

```text
0xFF / subop 0x00
  existing ellipse-list command moved under the extended opcode

0xFF / subop 0x01
  transformed-sprite frame command for native presentation strips
```

This keeps the render opcode space open for future frame-level commands without removing useful primitive operations.

## Rules

Native frame render commands are allowed only when all of these are true:

- The operation represents a repeated hot render pattern.
- The equivalent upstream LovyanGFX code would naturally keep the loop in C++.
- The command remains reusable outside one demo.
- Elixir still provides the frame state.
- Native code does not own application-specific behavior such as object movement, physics, or scene rules.

Native frame render commands must not be used for:

- setup
- queries
- calibration
- allocation
- low-frequency control operations
- one-off drawing that ordinary calls or primitive binary-batch commands already handle well

## Initial implementation

The first accepted implementation is a generic transformed-sprite frame command for MovingIcons-like workloads.

The command supports:

- native presentation strips
- a small fixed set of source sprite handles
- object records with source index, x, y, angle, and zoom
- transparent key handling
- one native loop over strips and objects
- one final display operation

The command avoids:

- per-object sprite registry lookup when source sprites can be resolved once
- per-command target resolution inside the strip loop
- repeated render command dispatch for each strip
- application-specific object movement

The MovingIcons example may use an example-local fast encoder that writes the native frame command directly from its object list. This is an implementation optimization for the benchmark path, not a replacement for the public `BinaryBatch` helpers.

## Consequences

### Positive

- Moves the hottest render loop closer to upstream native LovyanGFX examples.
- Avoids repeated command dispatch and target resolution inside each strip.
- Keeps Elixir responsible for high-level animation state and object movement.
- Preserves v2's small ordinary-call protocol.
- Preserves primitive `BinaryBatch` operations for generic render scripts.
- Gives performance-critical examples a realistic native-like path.
- Keeps the optimization reusable for sprite animation, particle systems, icon scenes, and face-part rendering.
- Provides a scalable opcode-space strategy through extended render sub-opcodes.

### Negative

- Adds a higher-level render command category to `BinaryBatch`.
- Requires more native implementation than primitive-only batching.
- Creates another path that must be tested and documented.
- Risks drifting toward demo-specific native APIs if naming and boundaries are not kept strict.
- Makes some render behavior less directly visible from the Elixir frame script.
- Requires care to avoid benchmark-only optimizations leaking into the generic protocol surface.

## Rejected alternatives

### Continue optimizing only primitive binary-batch commands

Rejected as the only strategy.

Primitive commands remain useful, but they cannot fully match upstream native examples when the upstream performance depends on a tight C++ loop around repeated drawing operations.

### Add a MovingIcons-specific native command

Rejected.

MovingIcons is a benchmark and representative workload, not a protocol concept.

### Move animation state into native code

Rejected.

Elixir should continue to own object state and application behavior. Native code should only execute the hot rendering pattern.

### Remove batch-level locking or write transactions

Rejected.

Locking and write transactions protect the singleton display and preserve LovyanGFX semantics. Recent testing did not show them to be the dominant performance bottleneck.

### Force small source sprites into internal RAM

Rejected as a default performance strategy.

Testing did not show an improvement for the current MovingIcons setup. Sprite memory placement may still be tuned later, but it is not part of this ADR's accepted direction.

### Use full-height native presentation strips by default

Rejected.

Testing reduced the strip count from `2` to `1`, but did not materially improve frame time. The upstream-style two-strip presentation remains a safer default for constrained memory.

### Use per-object dirty-region presentation

Rejected in the tested form.

Per-object dirty presentation increased complexity and caused instability before the first frame statistics line. Future dirty-region work should use a coarser and safer strategy if revisited.

## Follow-up work

Further performance work should focus on:

- reducing Elixir-side frame construction cost
- keeping benchmark encoders allocation-light
- reducing native presentation cost
- investigating lower-overhead strip presentation paths
- comparing against upstream native MovingIcons on the same board, panel, bus clock, and DMA configuration

Further work should not focus on:

- removing batch-level locking
- removing write transactions
- replacing primitive `BinaryBatch` operations
- adding demo-specific native commands

## Related documents

- [ADR 2026-05-02: Standardize v2 hot rendering on binary render batches](./2026-05-02-binary-batch-for-native-like-animation.md)
- [ADR 2026-05-03: Treat BinaryBatch as the standard render transaction API](./2026-05-03-binary-batch-as-render-transaction-api.md)
- [Architecture](../architecture.md)
- [Protocol](../protocol.md)
