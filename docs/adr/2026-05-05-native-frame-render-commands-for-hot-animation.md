<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 2026-05-05: Allow native frame render commands for hot animation loops

## Status

Proposed

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

The current v2 primitive render-batch path can express equivalent behavior, but it does so through a more generic command stream:

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

## Decision

Allow `BinaryBatch` to include native frame render commands for selected hot animation patterns.

A native frame render command may represent a complete reusable rendering pattern, not just one LovyanGFX primitive.

For MovingIcons-style workloads, Elixir should send compact frame state such as:

```text
source sprite id
x
y
angle
zoom
```

Native code should own the full frame render loop:

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

## Initial target

The first target is a generic transformed-sprite frame command for MovingIcons-like workloads.

The command should support:

- native presentation strips
- a small fixed set of source sprite handles
- object records with source index, x, y, angle, and zoom
- transparent key handling
- one native loop over strips and objects
- one final display operation

The command should avoid:

- per-object sprite registry lookup when source sprites can be resolved once
- per-command target resolution inside the strip loop
- duplicate culling when clipping can be handled by LovyanGFX or by a caller-selected mode
- application-specific object movement

## Consequences

### Positive

- Moves the hottest render loop closer to upstream native LovyanGFX examples.
- Avoids repeated command dispatch and target resolution inside each strip.
- Keeps Elixir responsible for high-level animation state and object movement.
- Preserves v2's small ordinary-call protocol.
- Preserves primitive `BinaryBatch` operations for generic render scripts.
- Gives performance-critical examples a realistic native-like path.
- Keeps the optimization reusable for sprite animation, particle systems, icon scenes, and face-part rendering.

### Negative

- Adds a higher-level render command category to `BinaryBatch`.
- Requires more native implementation than primitive-only batching.
- Creates another path that must be tested and documented.
- Risks drifting toward demo-specific native APIs if naming and boundaries are not kept strict.
- Makes some render behavior less directly visible from the Elixir frame script.

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

## Related documents

- [ADR 2026-05-02: Standardize v2 hot rendering on binary render batches](./2026-05-02-binary-batch-for-native-like-animation.md)
- [ADR 2026-05-03: Treat BinaryBatch as the standard render transaction API](./2026-05-03-binary-batch-as-render-transaction-api.md)
- [Architecture](../architecture.md)
- [Protocol](../protocol.md)
