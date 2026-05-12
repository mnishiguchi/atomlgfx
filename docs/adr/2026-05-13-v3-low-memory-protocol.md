<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 2026-05-13: Design AtomLGFX v3 as a low-memory LovyanGFX-style protocol

## Status

Accepted

## Context

AtomLGFX v1 proved that a practical AtomVM graphics application can run on M5Stack-class hardware together with networking.

The important proven use case is:

- Stack-chan-like face animation
- touch input
- Wi-Fi
- SNTP
- Distributed Erlang
- remote control of expression, gaze, and mouth state

That v1 example uses a modest rendering model:

```text
one 320x240 4-bit sprite
+ palette colors
+ ordinary drawing calls
+ one push_sprite per frame
```

The approximate persistent face sprite memory is:

```text
320 * 240 * 4 bits / 8 = 38,400 bytes
```

This is realistic for AtomVM applications that must leave memory for Wi-Fi, TCP/IP buffers, application processes, and long-running logic.

AtomLGFX v2 improved API consistency, code generation, protocol documentation, and performance experiments. However, v2 also introduced retained native rendering for the MovingIcons example. That experiment added native object buffers, retained render programs, native presentation strip buffers, a render task, and additional lifecycle complexity.

The retained renderer improved a narrow animation benchmark, but it made the library more memory-sensitive and harder to reason about.

For AtomLGFX v3, we will use the lessons from v2 while returning to a v1-like memory profile.

## Problem

AtomLGFX needs a protocol that is:

- intuitive from Elixir
- close to LovyanGFX concepts
- small in native code
- predictable in memory behavior
- good enough for smooth user-interface examples
- reliable with Wi-Fi enabled

The v2 retained-native-rendering model is too heavy for this goal.

The problem is not only total memory use. On ESP32-class devices, large contiguous allocations can fail even when total free heap appears acceptable. Hidden or retained allocations are especially risky when graphics must coexist with Wi-Fi, TCP/IP, SNTP, and Distributed Erlang.

The v3 protocol must avoid hidden large allocations and must make persistent memory ownership explicit.

## Decision drivers

In priority order:

1. Long-running stability with Wi-Fi enabled
2. Predictable memory ownership
3. Non-flickering UI examples
4. Intuitive LovyanGFX-like API
5. Good enough performance
6. Native-like animation performance only when it does not conflict with the above

## Decision

AtomLGFX v3 will be designed as a low-memory, call-based protocol focused on ordinary LovyanGFX-style operations, explicit sprite ownership, touch support, palette support, and optional synchronous binary batches.

AtomLGFX v3 will not include retained native render programs.

The preferred design is:

```text
ordinary calls
+ explicit sprites
+ palette support
+ touch support
+ optional synchronous BinaryBatch
+ no retained native scene
+ no native render task
```

The primary real-world smoke target is:

```text
Stack-chan-like face animation
+ touch
+ Wi-Fi
+ SNTP
+ Distributed Erlang
+ remote control
+ long-running stability
```

MovingIcons can remain a stress test, but it is not the primary design target.

## Goals

- Preserve v1-like memory behavior.
- Keep the public API intuitive and close to LovyanGFX.
- Keep native implementation smaller and easier to audit.
- Avoid hidden large allocations.
- Avoid native background rendering tasks.
- Support smooth Stack-chan-like face animation with Wi-Fi enabled.
- Keep `BinaryBatch` as an optional synchronous optimization.
- Use v2 code-generation lessons for consistency without shipping large runtime metadata.

## Non-goals

- Native-like MovingIcons performance.
- A native retained scene graph.
- A native animation engine.
- A background render task.
- Full LovyanGFX API coverage in the first v3 implementation.
- Compatibility with v2 retained-render APIs.

## Compatibility policy

V3 does not need to be wire-compatible with v2.

The public Elixir API may preserve common v1/v2 function names where they support the v3 goals, but v3 should not preserve APIs that increase memory risk or implementation complexity.

Retained-render APIs are intentionally excluded from v3.

## Protocol shape

The v3 wire protocol will use a compact flat tuple form by default:

```elixir
{:lgfx, 3, op, arg1, arg2, ...}
```

Examples:

```elixir
{:lgfx, 3, :ping}

{:lgfx, 3, :init}

{:lgfx, 3, :set_rotation, 1}

{:lgfx, 3, :fill_rect, 0, 10, 20, 80, 40, 0xFFFF}
#                    target x   y   w   h   color

{:lgfx, 3, :create_sprite, 1, 320, 240, 4}
#                         target width height depth

{:lgfx, 3, :push_sprite, 1, 0, 0}
#                       target x  y

{:lgfx, 3, :get_touch}

{:lgfx, 3, :submit_binary_batch, 0, 0, <<...>>}
#                                  target flags binary
```

The tuple contains:

```text
:lgfx       protocol tag
3           protocol version
op          operation atom
args        operation-specific positional arguments
```

The protocol intentionally avoids a nested argument list such as:

```elixir
{:lgfx, 3, op, [arg1, arg2, arg3]}
```

Lists allocate one cons cell per argument. For hot drawing calls, a flat tuple is simpler and more memory-friendly.

The protocol also avoids the heavier v2-style envelope:

```elixir
{:lgfx, 2, :call, op_code, target, flags, args}
```

V3 should keep flags and metadata out of the common path unless they are clearly needed.

The default v3 wire shape uses operation atoms for readability and simpler debugging. If measurements show that atom dispatch is materially slower or larger than numeric dispatch, the Elixir wrapper may translate public operation names to compact numeric opcodes internally. The public API should remain unchanged.

## Operation naming

Operation names should use Elixir-style `snake_case` atoms:

```elixir
:fill_rect
:draw_line
:create_sprite
:push_sprite
:get_touch
:submit_binary_batch
```

The public Elixir API should also use idiomatic Elixir names:

```elixir
AtomLGFX.fill_rect(...)
AtomLGFX.create_sprite(...)
AtomLGFX.push_sprite(...)
```

Documentation may mention the upstream LovyanGFX names where helpful, but the runtime API should not require `camelCase` atoms.

## Target model

V3 will keep the target model simple:

```text
target 0      LCD
target 1..n   sprites
```

On the wire, target-aware drawing operations should pass the target explicitly near the front of the argument list.

Example:

```elixir
{:lgfx, 3, :fill_rect, target, x, y, width, height, color}
```

The public Elixir API may keep target as an optional final argument for readability and compatibility with existing examples:

```elixir
AtomLGFX.fill_rect(port, x, y, width, height, color)
AtomLGFX.fill_rect(port, x, y, width, height, color, target)
```

The wrapper normalizes this into the explicit wire form.

## Replies

Commands should return:

```elixir
:ok
```

Queries should return:

```elixir
{:ok, value}
```

Errors should return small terms:

```elixir
{:error, reason}
```

or, when a small detail is useful:

```elixir
{:error, {reason, detail}}
```

Avoid large structured error maps in the embedded runtime.

Good error examples:

```elixir
{:error, :invalid_arg}
{:error, :not_initialized}
{:error, :unsupported_op}
{:error, {:invalid_target, 9}}
```

## Capabilities

Capability discovery should remain lightweight.

The low-level capability query should return an integer bitmask:

```elixir
{:ok, capability_bits}
```

Suggested capability bits:

```text
LGFX_CAP_TOUCH
LGFX_CAP_SPRITES
LGFX_CAP_PALETTE
LGFX_CAP_BINARY_BATCH
LGFX_CAP_TEXT
LGFX_CAP_IMAGES
```

Do not include retained render capability.

A friendly Elixir helper may convert the bitmask into a list or map for development, but the embedded protocol should stay compact.

## BinaryBatch

V3 will keep `BinaryBatch` as an optional synchronous optimization.

Binary batches are acceptable because they are:

- explicitly submitted by the caller
- executed synchronously
- not retained as native scene state
- not backed by a native render task
- easier to reason about than retained rendering

BinaryBatch should be used for:

- bursts of many small primitive drawing commands
- setup drawing
- optional MovingIcons stress-test modes
- drawing into an offscreen sprite when ordinary calls are too slow

BinaryBatch should not become a hidden scene system.

Each flagship example must have a safe renderer that does not require `BinaryBatch`.

`BinaryBatch` may be used only as an optional measured optimization. If `BinaryBatch` causes OOM, responsiveness regressions, or confusing failure modes on low-memory boards, the example must fall back to the safe renderer.

BinaryBatch builders must avoid avoidable intermediate binaries in hot loops. Prefer iodata construction with one final binary conversion, or prebuilt/static command fragments where practical. Debug helpers such as decode, summary, and validation must not be used in animation loops.

The basic wire operation is:

```elixir
{:lgfx, 3, :submit_binary_batch, target, flags, binary}
```

Where:

```text
target   LCD or sprite target
flags    small integer, usually 0
binary   packed command stream
```

The command stream format should be documented separately.

## Memory rules

V3 protocol design must follow these rules.

### No hidden large allocation

Query operations must not allocate large native buffers.

Safe examples:

```elixir
AtomLGFX.width(port)
AtomLGFX.height(port)
AtomLGFX.get_touch(port)
AtomLGFX.get_capabilities(port)
```

A function named like `get_*` must behave like a query.

Presentation or strip buffers must not be allocated by query functions. If strip rendering is supported, buffer allocation must happen through an explicit create, prepare, or render path and must have a matching release path.

### Persistent allocation must be explicit

Persistent allocations must use clear names:

```elixir
create_sprite
delete_sprite
create_palette
open
close
```

A function that allocates persistent memory should make that behavior obvious from its name and documentation.

### Every persistent allocation needs a release path

Examples:

```text
open -> close
create_sprite -> delete_sprite
```

If palettes are separate native resources, they must have explicit release operations. If a palette is owned by a sprite, deleting the sprite must release the palette.

### No native render task

V3 must not start a native background renderer.

All drawing is initiated by the caller.

### No retained native scene state

V3 must not keep object buffers, render programs, or scene descriptions alive on the native side.

### Prefer low-bit-depth sprites for UI

The recommended Stack-chan-style face renderer should use:

```text
320x240 4-bit sprite
+ palette colors
+ one push per frame
```

This is the primary practical rendering pattern.

Avoid defaulting to larger buffers such as:

```text
320x240 8-bit sprite      = 76,800 bytes
480x320 RGB565 full frame = 307,200 bytes
```

Full RGB565 frame buffers should be considered optional and board-dependent.

## Safe renderer before fast renderer

Each important example must have a safe rendering path that uses minimal memory and simple operations.

For the face example:

```text
safe path:
  ordinary drawing calls into 4-bit sprite
  push sprite once per frame

optional fast path:
  small batch into 4-bit sprite
  push sprite once per frame
```

The safe path must remain the baseline.

If the fast path becomes OOM-prone or harder to debug, the example should fall back to the safe path.

## Touch response policy

Touch response should be improved by loop structure, not by blindly increasing graphics load.

Recommended application loop:

```text
poll touch frequently
update face state immediately
render at a controlled max FPS
force redraw when touch state changes
push completed sprite frame
```

The protocol should make touch polling cheap and independent from rendering where practical.

## API surface

The initial v3 public API should focus on a compact, useful subset.

### Device lifecycle

```elixir
AtomLGFX.open(opts)
AtomLGFX.close(port)
AtomLGFX.ping(port)
AtomLGFX.init(port)
```

### Display configuration

```elixir
AtomLGFX.set_rotation(port, rotation)
AtomLGFX.width(port)
AtomLGFX.height(port)
AtomLGFX.set_brightness(port, brightness)
```

### Primitive drawing

```elixir
AtomLGFX.fill_screen(port, color)
AtomLGFX.draw_line(port, x0, y0, x1, y1, color)
AtomLGFX.draw_rect(port, x, y, width, height, color)
AtomLGFX.fill_rect(port, x, y, width, height, color)
AtomLGFX.draw_circle(port, x, y, radius, color)
AtomLGFX.fill_circle(port, x, y, radius, color)
AtomLGFX.fill_triangle(port, x0, y0, x1, y1, x2, y2, color)
```

Target-aware variants may accept an optional final target argument.

### Sprites

```elixir
AtomLGFX.create_sprite(port, width, height, depth, target)
AtomLGFX.delete_sprite(port, target)
AtomLGFX.push_sprite(port, target, x, y)
AtomLGFX.set_swap_bytes(port, swap?, target)
```

### Palettes

```elixir
AtomLGFX.create_palette(port, target)
AtomLGFX.set_palette_color(port, target, index, color)
```

If palettes are sprite-owned, `delete_sprite/2` must release the associated palette.

### Touch

```elixir
AtomLGFX.get_touch(port)
```

### Text and images

Text and image APIs should be added conservatively.

They should not force large runtime metadata or hidden allocations into the common path.

### Batch

```elixir
AtomLGFX.submit_binary_batch(port, target, batch)
```

A convenience wrapper may default the target to LCD:

```elixir
AtomLGFX.submit_binary_batch(port, batch)
```

## Code generation

V3 may keep an operation definition file, but generated output should be used carefully.

Generation is useful for:

- native dispatch declarations
- Elixir wrapper generation
- docs
- tests
- arity validation

Generation should not force large runtime metadata maps into the embedded application.

The preferred approach is:

```text
generate at build time
compile only compact dispatch and wrappers
avoid shipping large metadata structures to AtomVM
```

Prefer generated function clauses:

```elixir
def opcode(:fill_rect), do: 0x23
def opcode(:fill_circle), do: 0x24
def opcode(:push_sprite), do: 0x41
```

Avoid large runtime maps containing docs, schemas, and metadata:

```elixir
%{
  fill_rect: %{
    args: [...],
    docs: "...",
    schema: ...
  }
}
```

Full metadata can remain available for host-side documentation generation and tests.

## Examples

### Stack-chan-like face

This becomes the primary v3 smoke example.

Expected rendering model:

```text
create one 320x240 4-bit sprite
create palette
draw face into sprite
push sprite once per frame
read touch frequently
allow remote state changes
```

Required smoke coverage:

- AtomLGFX opens and initializes
- face sprite allocation succeeds
- palette colors are configured
- animation runs without visible flicker
- touch input updates gaze and mouth state
- Wi-Fi connects
- SNTP synchronizes
- Distributed Erlang starts
- remote expression/gaze/mouth controls work
- the app runs for an extended period without OOM or reboot

### MovingIcons

MovingIcons remains a stress test, not the design target.

Allowed modes:

```text
sprite_strips
binary_batch, optional measured mode
direct_lcd_diagnostic, diagnostic only
sprite_full, only when memory diagnostics show enough headroom
```

Disallowed mode:

```text
retained_native
```

MovingIcons should avoid obvious flicker where practical, but it does not need to match native LovyanGFX performance.

The default MovingIcons mode should prioritize running reliably on low-memory boards over visual richness.

## Consequences

### Positive

- Lower memory footprint than v2 retained rendering
- More predictable allocation behavior
- Simpler native implementation
- Smaller public API
- Easier cleanup semantics
- Better chance of stable Wi-Fi coexistence
- Easier examples and documentation
- Clearer project identity

### Negative

- MovingIcons will not target native-like performance
- v2 retained-render code and tests will be removed or abandoned
- Some v2 protocol flexibility will be intentionally dropped
- Future advanced animation support would need a separate design

### Neutral

`BinaryBatch` remains available, but only as an explicit synchronous optimization.

It is not a retained renderer.

## Alternatives considered

### Continue v2 retained rendering

Rejected.

It preserves the MovingIcons experiment but keeps too much memory pressure and lifecycle complexity in the core library.

### Optimize v2 retained rendering

Rejected for the core library.

Reducing strip height, improving cleanup, using PSRAM, and shrinking task stack size would help, but the design would still be too specialized for the main API.

### Hide retained rendering behind a compile-time option

Deferred.

This may be useful later for experiments, but it should not define the core v3 protocol.

### Return fully to v1 protocol

Rejected.

V1 had better memory behavior, but v2 taught useful lessons about naming, generated consistency, documentation, validation, and explicit batching. V3 should keep those lessons without keeping v2’s retained renderer.

### Remove batching entirely

Rejected for now.

Small explicit batches can improve responsiveness for examples such as Stack-chan face rendering. The problem is not batching itself; the problem is unbounded or retained rendering state.

## Implementation plan

### 1. Define v3 operation list

Create a compact v3 operation definition file.

Each operation should define:

- operation atom
- arity
- argument types
- return type
- whether it can allocate
- whether it targets LCD/sprite
- whether it is safe as a query

### 2. Implement v3 native decoder

Implement a flat tuple decoder for:

```elixir
{:lgfx, 3, op, ...args}
```

The decoder should:

- validate protocol tag
- validate version
- dispatch by operation atom
- validate arity
- decode positional arguments
- return compact errors

If measurements show that numeric opcodes are materially better, keep the public API unchanged and translate operation atoms to compact opcodes in the Elixir wrapper.

### 3. Remove retained rendering from the core

Remove or exclude:

- object buffers
- render programs
- native render task
- retained render stats
- retained render capability
- retained render Elixir modules
- MovingIcons retained-native mode

### 4. Keep or adapt BinaryBatch

Keep `BinaryBatch` as a separate explicit path.

Review its memory behavior and avoid validation, decode, or summary helpers in hot loops.

### 5. Port Stack-chan-like face to v3

Port the v1 face example to the v3 API.

Use it as the primary memory and reliability smoke test.

### 6. Add memory diagnostics

Add optional debug logging for:

- free heap
- largest free block
- after AtomLGFX open
- after display init
- after sprite creation
- before Wi-Fi start
- after Wi-Fi start
- after got IP
- after Distributed Erlang start

### 7. Update documentation

Document:

- v3 protocol shape
- public API
- memory rules
- recommended sprite-based rendering pattern
- BinaryBatch as optional optimization
- retained rendering removal

## Acceptance criteria

This ADR is implemented when:

- v3 protocol uses a compact flat tuple shape, or an equivalent compact opcode form behind the same public API
- ordinary drawing calls work through v3
- sprite creation, palette setup, and `push_sprite` work through v3
- touch works through v3
- `BinaryBatch` works as an explicit synchronous path, or is intentionally deferred
- retained render program APIs are absent from v3
- no query operation performs hidden large allocation
- Stack-chan-like face animation runs without visible flicker
- Stack-chan-like face animation runs with Wi-Fi enabled
- SNTP and Distributed Erlang can run with the face example
- long-running face animation does not OOM under normal usage
- MovingIcons no longer depends on retained native rendering

## Summary

AtomLGFX v3 will be a pragmatic low-memory protocol.

It will keep the useful lessons from v2:

```text
clear API naming
generated consistency
explicit binary batches
better documentation
```

while returning to the practical memory behavior proven by v1:

```text
ordinary calls
+ explicit sprites
+ palette rendering
+ touch
+ Wi-Fi coexistence
+ stable long-running applications
```

V3 should make the common embedded graphics use case reliable first. Performance experiments can exist separately, but they should not shape the core protocol.

The core principle is:

```text
AtomLGFX v3 is not an animation engine.
It is a low-memory LovyanGFX wrapper for practical AtomVM applications.
```
