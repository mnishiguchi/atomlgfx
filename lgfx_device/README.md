<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# lgfx_device

`lgfx_device/` is the LovyanGFX-facing native adapter layer.

It owns:

- target resolution
- device-facing validation and semantics
- sprite, palette, image, and text behavior at the device boundary
- the thin integration layer around the pinned LovyanGFX submodule

It does not decode AtomVM terms or build protocol replies.

See [the protocol spec](../docs/protocol.md) for wire-level rules and
[the architecture overview](../docs/architecture.md) for the top-level repository map.

## File map

- `lgfx_device.h`
  - device-facing public adapter surface used by the port layer

- `lgfx_device_internal.hpp`
  - internal declarations and shared adapter details

- `state.cpp`
  - device state
  - initialization
  - panel and touch setup
  - target resolution helpers
  - native strip-presentation state and lazy strip allocation

- `control.cpp`
  - display control operations such as init, close, rotation, brightness, and color depth

- `text.cpp`
  - text operations
  - font preset handling
  - cursor and wrap behavior
  - direct LovyanGFX-style text-scale handling at the device boundary

- `primitives.cpp`
  - primitive drawing operations

- `images.cpp`
  - JPEG and RGB565 image operations

- `sprites.cpp`
  - sprite lifecycle
  - palette lifecycle
  - pivot
  - sprite push and rotate/zoom behavior

- `clip.cpp`
  - clip rectangle operations

- `fonts/generated/`
  - generated font data compiled into the component when enabled

## Responsibility split

This layer is responsible for device semantics that should not be duplicated in handlers.

Examples:

- whether a target exists
- whether a sprite handle is already allocated
- whether a sprite is palette-backed
- whether a destination sprite exists
- `pushImage` stride and payload validity
- rotate/zoom semantic validity
- final validation and forwarding of LovyanGFX-style numeric arguments to the pinned LovyanGFX call shape

## Design intent

This layer is intentionally small.

Policy:

- keep protocol concerns out of the adapter
- keep AtomVM and term handling out of the adapter
- keep raw LCD control separate from logical LCD drawing
- route logical drawing through a render-target seam when needed
- keep compatibility scaffolding minimal

The goal is not to mirror all of LovyanGFX. The goal is to provide a small, explicit adapter surface that matches the protocol exposed by this repository while preserving room for native presentation policy.

## Current model

The native model separates per-port configuration from live device ownership:

- per-port configuration is stored by the port layer
- the live LCD device remains singleton-backed
- this layer resolves owner-aware init, close, and dimension queries using an opaque owner token

For drawing, the layer distinguishes between:

- raw target resolution
  - target `0` means the live LCD device
- render-target resolution
  - target `0` may resolve to an active native presentation strip during strip presentation
  - otherwise it falls back to the live LCD

The native presentation path uses:

- lazy, not eager, allocation
- adaptive double strip buffers
- direct-LCD fallback when native strip allocation is unavailable

At the moment, the Elixir MovingIcons sample may still manage its own strip orchestration. Native strip presentation is therefore available as a device-layer capability without yet being the only active strip path.

That means this layer may care about singleton ownership and native presentation state, but it should not care about protocol envelopes or AtomVM terms.

## When changing this layer

When adding or changing device behavior:

- keep protocol tuple rules in `lgfx_port/` and `../docs/protocol.md`
- keep this layer focused on target resolution, ownership, device semantics, and native presentation behavior
- update protocol docs only when the externally visible contract changes
