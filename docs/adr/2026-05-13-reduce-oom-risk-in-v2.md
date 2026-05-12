<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 2026-05-13: Reduce OOM risk in AtomLGFX v2

## Status

Accepted

## Context

AtomLGFX v2 simplified the native port implementation and added a generic call-based protocol plus explicit binary batches.

During v2 performance work, we also added experimental retained native rendering for the MovingIcons example. That path kept object state and the hot display loop on the native side to approach native LovyanGFX animation behavior.

However, v2 became more OOM-prone than v1.

V1 already proved the practical target we care about: a Stack-chan-like face animation running together with touch input, Wi-Fi, SNTP, Distributed Erlang, and remote control. That example uses a modest rendering model: a `320x240` 4-bit sprite, palette colors, ordinary drawing calls, and one `push_sprite` per frame.

V2 should preserve that reliability. Native-like MovingIcons performance is useful as an experiment, but it is not the main product requirement.

## Problem

The retained native rendering path increases persistent memory pressure and lifecycle complexity.

It introduces or requires:

- native object buffers
- retained render program state
- native presentation strip buffers
- native render task stack
- retained renderer stats
- exclusive display ownership checks
- additional cleanup paths
- additional Elixir wrapper modules
- additional generated protocol surface

This is too much memory and API surface for a feature that is unlikely to be used often.

The main risk is not only total memory use. Large contiguous allocations can fail even when total free heap appears acceptable. This matters when AtomLGFX must coexist with Wi-Fi, TCP/IP buffers, SNTP, and Distributed Erlang.

## Decision

Remove experimental retained native rendering from the normal AtomLGFX v2 library surface.

Keep v2 focused on:

- ordinary LovyanGFX-style calls
- sprite creation and drawing
- palette support
- touch support
- image and text operations
- explicit `BinaryBatch` submission for useful drawing bursts

Remove or disable:

- native object buffers
- retained render programs
- native retained render task
- retained render stats
- retained renderer exclusive display mode
- retained renderer Elixir helper modules
- MovingIcons retained-native mode

The MovingIcons and face examples should still avoid flicker by drawing into offscreen buffers or native presentation strips before pushing to the LCD.

## Rationale

V1 proved the practical target. The Stack-chan-like face example runs a useful AtomVM application with graphics and networking together.

That example uses a small persistent graphics footprint:

```text
320 * 240 * 4 bits = 38,400 bytes
```

This is much more realistic for AtomVM applications than retaining large RGB565 presentation strips plus native renderer state.

For embedded use, a slightly slower but stable renderer is preferable to a faster renderer that makes Wi-Fi or long-running applications unreliable.

## Consequences

### Positive

- Lower native memory footprint
- Fewer large persistent allocations
- Fewer cleanup failure paths
- Smaller public API
- Smaller Elixir wrapper surface
- Easier documentation
- Better chance of stable coexistence with Wi-Fi and Distributed Erlang

### Negative

- MovingIcons no longer targets native-like performance through retained native rendering
- Experimental performance code is removed
- Any future native animation engine would need to be built separately

### Neutral

`BinaryBatch` remains available as the preferred optimization path for many small drawing commands.

It is simpler than retained native rendering because it is submitted explicitly, executed synchronously, and does not keep a native render task or retained scene alive.

## Flicker policy

Examples should avoid flicker even after retained native rendering is removed.

Preferred patterns:

```text
draw into sprite
push sprite once
```

or:

```text
draw into strip sprite
push strip
repeat for remaining strips
```

Avoid this pattern for animation:

```text
clear LCD
draw objects directly to LCD one by one
```

If full-frame buffering is too large, use smaller strips instead of direct LCD drawing.

## Implementation notes

This decision removes the retained render program protocol operations and native implementation.

The MovingIcons example moves to the existing binary frame-strip path using `AtomLGFX.BinaryBatch.push_rotate_zoom_frame_strips/2`. Elixir owns object updates, while native code owns strip clearing, transformed sprite drawing, and strip presentation for each frame.

`get_presentation_strip_height/1` is treated as a query and must not allocate native strip buffers. Allocation should happen only during explicit render or preparation paths.

## Acceptance criteria

This ADR is implemented when:

- retained render program APIs are removed from the public Elixir API
- retained render program native handlers are removed
- retained render program protocol operations are removed
- generated protocol docs no longer describe retained rendering
- MovingIcons no longer depends on retained native rendering
- face animation still renders without visible flicker
- MovingIcons does not regress to obvious flicker
- v2 can run the Stack-chan-like face example with Wi-Fi enabled
- long-running face animation does not OOM under normal usage
