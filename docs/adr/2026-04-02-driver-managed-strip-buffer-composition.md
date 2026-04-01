<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 2026-04-02: Driver-managed strip-buffer composition for smooth LCD animation

## Status

Accepted

## Context

`atomlgfx` already adopted explicit batching to reduce Elixir/native control-plane overhead while preserving the simple synchronous mental model for ordinary operations.

The accepted direction keeps ordinary operations direct, keeps batching explicit, and confines runtime concerns to explicit batch execution.

That batching model improves grouped native execution, but it does not by itself guarantee flicker-free animation on the live LCD. In the current Elixir `MovingIcons` sample, the direct-LCD frame path clears the LCD and then redraws all objects on the visible target each frame. This is simple, but it exposes frame rebuild on the live panel.

A reference LovyanGFX-based C++ app demonstrates a smoother 50-object path using two strip-buffer sprites. For each frame, it:

- moves objects
- iterates vertical strips
- clears the current strip sprite
- renders all objects into that strip sprite with `pushRotateZoom`
- pushes the finished strip sprite to the LCD
- calls `lcd.display()` after the frame

That app keeps object logic simple and delegates composition to LovyanGFX sprite rendering instead of app-side dirty-rectangle logic.

This creates a design question for `atomlgfx`:

- should animation smoothness be pursued by making application code smarter,
  or should the port driver own buffered composition so the public API and app logic remain simple?

## Decision

`atomlgfx` adopts the following direction for smooth animation workloads such as 50-object `MovingIcons`:

1. Buffered composition becomes a driver concern.
   - The port driver will own strip-buffer composition for LCD presentation-oriented workloads.
   - Application code should not manage strip sprites, strip iteration, or buffer flipping directly.

2. `lgfx_device` remains the LovyanGFX-facing composition layer.
   - Internal strip sprites are managed inside the device layer.
   - LovyanGFX sprite operations remain the primary rendering mechanism.
   - We continue to delegate heavy lifting to upstream LovyanGFX rather than introducing scene-specific drawing engines.

3. The public API remains generic.
   - Logical LCD target `0` continues to represent “the LCD” from the caller’s perspective.
   - We do not introduce a scene-specific API such as `render_moving_icons_frame`.
   - Application code should still feel like:
     - clear frame
     - draw objects
     - display frame

4. Smooth animation does not require smarter app-side redraw policy.
   - We do not add app-side dirty rectangles.
   - We do not add app-side previous-frame bookkeeping.
   - We do not make `MovingIcons` responsible for composition strategy.

5. Direct-LCD rendering remains useful, but not the primary smooth-animation target.
   - Direct-LCD mode may remain for comparison, bring-up, and diagnostics.
   - The primary path for smooth 50-object animation becomes buffered composition.

6. Batching remains complementary, not a replacement for buffering.
   - Explicit batching continues to reduce control-plane overhead.
   - Buffered composition addresses visible frame rebuild on the live LCD.
   - These concerns are related but distinct.

## Rationale

The reference app is strong evidence that smooth multi-object animation can be achieved with:

- buffered composition
- LovyanGFX sprite rendering
- simple object logic
- no app-side dirty-region complexity

This matches the broader `atomlgfx` goal of keeping the API and application model simple while pushing implementation complexity downward into the native layers. The earlier batching ADR already established that runtime and native execution machinery should serve performance without distorting the ordinary API model. Driver-managed strip buffers follow the same philosophy.

Keeping buffering inside the driver has several advantages:

- the application remains easy to read and reason about
- the public API remains a generic LovyanGFX-style wrapper
- composition strategy can evolve without changing Elixir sample logic
- the solution stays closer to the proven reference rendering model

A direct-LCD full-frame clear-and-redraw path is simpler internally, but it exposes frame rebuild on the live panel. Buffered composition is a better fit for the stated goal: smooth 50-object animation with dumb app logic.

## Consequences

### Positive

- Keeps `MovingIcons` and similar apps simple
- Preserves the generic public AtomLGFX API
- Delegates composition work to upstream LovyanGFX sprite operations
- Aligns with the proven reference architecture for smooth 50-object animation
- Allows buffering policy to evolve inside the native driver without forcing Elixir-side redesign

### Negative

- Increases complexity inside `lgfx_device`
- Requires internal buffer lifecycle and strip-height allocation logic
- Consumes memory for hidden strip sprites
- Introduces another internal presentation mode that must be documented and tested carefully
- May require additional tuning of bus speed, DMA, and lock behavior to achieve full benefit

## Rejected alternatives

### Alternative 1: make `MovingIcons` smarter with dirty rectangles

Rejected.

This would push complexity into application code, make the demo less representative of a simple user-facing rendering model, and move responsibility away from the native layer where composition policy belongs.

### Alternative 2: keep only direct-LCD full-frame redraw and rely on batching alone

Rejected.

Batching reduces control-plane overhead, but it does not by itself hide visible rebuild of the live LCD surface during full-frame clear-and-redraw rendering. Buffered composition addresses a different problem.

### Alternative 3: add scene-specific native rendering APIs

Rejected.

This would weaken the goal of keeping `atomlgfx` a generic LovyanGFX wrapper and would couple the driver too tightly to one demo pattern.

## Follow-up implications

- Add internal strip-buffer state and lifecycle management in `lgfx_device`.
- Implement adaptive strip-height allocation similar in spirit to the reference approach.
- Keep logical LCD rendering generic from the caller’s perspective.
- Preserve direct-LCD mode only as an optional comparison or bring-up path.
- Benchmark smoothness and throughput at 10, 25, and 50 objects.
- After correctness, tune LCD bus write speed and related transport settings using the reference app as one practical comparison point.
