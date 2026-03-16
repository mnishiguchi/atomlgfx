# Architecture

## Summary

`atomlgfx` is an ESP-IDF component that exposes LovyanGFX to AtomVM Elixir through a tuple-based port protocol.

Core ideas:

- Host ↔ driver messages are Erlang terms.
- Large payloads use binaries.
- Protocol-visible behavior is metadata-driven from `ops.def`.
- `getCaps` is derived from protocol metadata plus the enabled dispatch surface.
- Protocol work and device work are split into separate C-side execution paths.
- Build defaults come from a generated config header shared by the port and device layers.
- Open-time config may override selected runtime values per port before `init`.
- The device layer is a thin C ABI around the pinned LovyanGFX surface used by this repository.
- Touch is advertised only when support is both enabled and effectively attached.
- `setTextSize` uses positive x256 fixed-point integers on the wire and converts to the pinned LovyanGFX float API only at the final device boundary.

The wire contract itself is documented in `docs/LGFX_PORT_PROTOCOL.md`.

## Big picture

```text
Elixir / AtomVM
    |
    | {lgfx, ProtoVer, Op, Target, Flags, ...}
    v
+------------------------------+
| port thread                  |
| - mailbox drain              |
| - request decode             |
| - metadata validation        |
| - handler wire decode        |
| - reply encode               |
+---------------+--------------+
                |
                | plain C job
                v
+------------------------------+
| worker task                  |
| - execute lgfx_device_*      |
| - free copied payloads       |
| - notify waiting caller      |
+---------------+--------------+
                |
                v
+------------------------------+
| src/ device adapter          |
| - pinned LovyanGFX surface   |
| - device semantics           |
+---------------+--------------+
                |
                v
           LovyanGFX
```

## Repository map

Main areas:

- `include/lgfx_port/`
  - public ESP-IDF headers

- `lgfx_port/`
  - AtomVM-facing port implementation
  - request decode, metadata validation, dispatch
  - worker bridge

- `lgfx_port/include_internal/lgfx_port/`
  - internal metadata and helper headers
  - protocol helpers
  - worker job definitions

- `src/`
  - device-facing adapter layer
  - protocol-agnostic C/C++ around the pinned LovyanGFX submodule

- `examples/elixir/`
  - example Elixir client

- `docs/`
  - protocol, worker, and architecture docs

Minimal layout view:

```text
.
├── include/lgfx_port/
├── lgfx_port/
│   ├── handlers/
│   ├── include_internal/lgfx_port/
│   ├── lgfx_port.c
│   ├── lgfx_worker_core.c
│   ├── lgfx_worker_device.c
│   └── proto_term.c
├── src/
├── docs/
└── examples/elixir/
```

## Build defaults and open-time overrides

This component uses a generated config header so the protocol layer and device layer see the same build-default wiring, geometry, and LovyanGFX settings.

Relevant files:

- template:
  - `lgfx_port/cmake/lgfx_port_config.h.in`

- generated output:
  - `<build>/.../generated/lgfx_port/lgfx_port_config.h`

How it works:

- `CMakeLists.txt` exposes cache variables and options.
- The parent ESP-IDF project can set them directly or via `idf.py -D...`.
- CMake generates the header with `configure_file(... @ONLY)`.
- The generated include directory is exported so all component code sees the same defaults.

Open-time overrides are separate from build defaults:

- build defaults come from the generated config header
- selected fields may be overridden per port at `open_port/2`
- those overrides are stored on the port context
- they are applied when that same port calls `init`

Important rule:

- The generated config header is the source of truth for build defaults.
- Do not rewrite the `@VAR@` tokens in `lgfx_port_config.h.in`.

## Metadata-driven protocol surface

The protocol-visible operation surface is declared in:

- `lgfx_port/include_internal/lgfx_port/ops.def`

That metadata drives:

- op atom registration

- dispatch table entries

- validation rules
  - arity
  - allowed flags
  - target policy
  - init-state policy

- capability linkage used by `getCaps`

- generated tables in `docs/LGFX_PORT_PROTOCOL.md`

Worker jobs follow the same pattern:

- `lgfx_port/include_internal/lgfx_port/worker_jobs.def`

That job list drives:

- worker job enum values
- union members in `lgfx_job_t`
- worker dispatch bodies

The design goal is simple: one declarative list, many synchronized outputs.

## Execution model

There are two C-side execution paths.

### Port thread path

Main files:

- `lgfx_port/lgfx_port.c`
- `lgfx_port/handlers/*.c`
- `lgfx_port/proto_term.c`

Responsibilities:

- run as the AtomVM native handler
- drain mailbox messages
- decode request tuples
- apply metadata-driven validation
- dispatch to handlers
- encode `{ok, Result}` or `{error, Reason}`
- own protocol-facing state such as `last_error`

Handlers stay intentionally narrow:

- decode op-specific wire arguments
- enforce obvious wire-level constraints
- call synchronous `lgfx_worker_device_*` wrappers
- keep protocol-owned fixed-point units intact until the device boundary

### Worker and device path

Main files:

- `lgfx_port/lgfx_worker_core.c`
- `lgfx_port/lgfx_worker_device.c`
- `src/`

Responsibilities:

- execute device work through a worker task and queue
- bridge handlers to plain C job payloads
- call the device adapter in `src/`
- keep protocol code separate from hardware code

The device layer is authoritative for device-facing semantics such as:

- target existence and resolution
- deterministic sprite allocation at a chosen handle
- destination-aware sprite push behavior
- `pushImage` payload validity
- final conversion of protocol-owned text scale x256 values into LovyanGFX float calls
- rotate/zoom semantic validity

## Port lifecycle

### Driver-global lifecycle

`REGISTER_PORT_DRIVER(...)` registers driver-global hooks such as:

- `init`
- `destroy`
- `create_port`

Important distinction:

- `init` and `destroy` are driver-global hooks
- they are not per-port-instance teardown hooks

### Per-context lifecycle

Per-port-instance state lives in `ctx->platform_data` and is managed in `lgfx_port/lgfx_port.c`.

Creation:

- allocate AtomVM `Context`
- allocate per-port `lgfx_port_t`
- parse and store open-time config snapshot
- start worker state via `lgfx_worker_start(...)`

Teardown:

- runs when AtomVM marks the context as `Killed`
- device close is attempted before worker stop
- worker state is stopped and freed
- per-port state is then released

This avoids freeing per-port state from the driver-global `destroy` callback.

## Config persistence vs singleton ownership

The current model separates configuration from live-device ownership.

- multiple port contexts may each keep their own open-config snapshot
- the LCD device remains singleton-backed
- only one port may own the live singleton device at a time
- each `init()` reuses the calling port's persisted snapshot

In other words:

- config persistence is per port
- live device ownership is singleton-global

## Binary payload ownership

Some operations carry binary payloads such as UTF-8 text or RGB565 pixel data.

Rule:

- The driver must not retain pointers into Erlang binaries past the request-handling boundary unless lifetime is explicitly managed.

Current model:

- variable-length payloads are deep-copied into job-owned memory before enqueue
- the worker frees the copied payload after the device call completes
- the worker then notifies the waiting caller

That keeps ownership explicit across the worker boundary.

## Pinned LovyanGFX policy

The adapter intentionally targets the pinned LovyanGFX submodule surface used by this repository.

Policy:

- prefer direct calls over compatibility probing
- update deliberately when the pinned submodule changes
- do not carry compatibility scaffolding for unknown LovyanGFX or M5GFX variants

Current sprite-facing surface used by the adapter:

- `createSprite(w, h)`
- `pushSprite(dst, x, y [, transparent565])`
- `pushRotateZoom(dst, x, y, angle_deg, zoom_x, zoom_y [, transparent565])`

Current protocol implications:

- `createSprite` is deterministic: the caller chooses the sprite handle
- `pushSprite` is a destination-aware whole-sprite blit
- `pushRotateZoom` is a destination-aware rotate/zoom blit
- `pushSpriteRegion` is not part of the current protocol surface

## Host API surface policy

This port does not try to expose LovyanGFX one method at a time.

Policy:

- prefer explicit rendering semantics over convenience aliases
- prefer explicit arguments over hidden mutable workflow state
- prefer small coherent feature slices over isolated helper methods
- expose APIs only when their protocol contract is clear, stable, and easy to test

Decision rule for exposing a LovyanGFX-facing API:

- it must enable a real use case that is awkward or impossible with the current surface
- it must be useful on its own, not only as part of a larger missing cluster
- its behavior must be easy to describe across target types, font selection, text size, datum, and wrap settings
- it must fit the current explicit draw model without introducing surprising hidden state
- it must be straightforward to validate in protocol smoke tests and sample apps

If those conditions are not met, the API should be deferred.

## Capability advertisement

`getCaps` is metadata-driven.

Current model:

- walk ops declared in `ops.def`
- ignore ops with `feature_cap_bit == 0`
- require the op to be enabled in the real dispatch surface
- OR enabled bits into `FeatureBits`
- mask to known protocol bits before returning

Important consequence:

- a capability bit is advertised only when backed by a real enabled operation
- touch is advertised only when touch support is compiled in and attached

## Documentation sync

Generated tables in `docs/LGFX_PORT_PROTOCOL.md` are synchronized from source metadata.

- script:
  - `scripts/sync_lgfx_protocol_doc.exs`

- typical usage:
  - `elixir scripts/sync_lgfx_protocol_doc.exs`
  - `elixir scripts/sync_lgfx_protocol_doc.exs --check`

## Where to look

- protocol spec:
  - `docs/LGFX_PORT_PROTOCOL.md`

- worker/concurrency model:
  - `docs/WORKER_MODEL.md`

- protocol metadata:
  - `lgfx_port/include_internal/lgfx_port/ops.def`

- worker job metadata:
  - `lgfx_port/include_internal/lgfx_port/worker_jobs.def`

- port entrypoint, lifecycle, mailbox drain, dispatch:
  - `lgfx_port/lgfx_port.c`

- request decode and reply helpers:
  - `lgfx_port/proto_term.c`

- handlers:
  - `lgfx_port/handlers/*.c`

- worker core:
  - `lgfx_port/lgfx_worker_core.c`

- worker wrappers:
  - `lgfx_port/lgfx_worker_device.c`

- device-facing adapter:
  - `src/`
