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

- `control.cpp`
  - display control operations such as init, close, rotation, brightness, and color depth

- `text.cpp`
  - text operations
  - font preset handling
  - cursor and wrap behavior
  - text-scale conversion near the LovyanGFX call boundary

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
- final conversion from protocol-owned fixed-point values to the pinned LovyanGFX call shape

## Design intent

This layer is intentionally thin.

Policy:

- prefer direct calls to the pinned LovyanGFX surface
- keep protocol concerns out of the adapter
- keep AtomVM and term handling out of the adapter
- keep compatibility scaffolding minimal

The goal is not to mirror all of LovyanGFX. The goal is to provide a small, explicit adapter surface that matches the protocol exposed by this repository.

## Current model

The current native model separates configuration persistence from live device ownership:

- per-port configuration is stored by the port layer
- the live LCD device remains singleton-backed
- this layer executes against that singleton-backed device state

That means this layer should stay focused on device truth, not caller identity or protocol envelope handling.

## When changing this layer

When adding or changing device behavior:

- keep protocol tuple rules in `lgfx_port/` and `../docs/protocol.md`
- keep worker job definitions in `../lgfx_port/include_internal/lgfx_port/worker_jobs.def`
- keep this layer focused on target resolution and device semantics
- update protocol docs only when the externally visible contract changes
