# Architecture

## Summary

`atomlgfx` is an ESP-IDF component that exposes LovyanGFX to AtomVM Elixir through a tuple-based port protocol.

Key ideas:

- Host ↔ driver messages are Erlang terms.
- Large payloads use binaries.
- Protocol-visible behavior is metadata-driven from `ops.def`.
- `getCaps` is derived from protocol metadata plus the enabled dispatch surface.
- Protocol work and device work are split into separate C-side execution paths.
- Build-time wiring and LovyanGFX tuning are shared through a generated config header.
- The device layer is a thin C ABI around the pinned LovyanGFX surface used by this repository.
- Touch is advertised only when touch support is both enabled and attached.

The protocol contract itself is documented in `docs/LGFX_PORT_PROTOCOL.md`.

## Big picture

```text
Elixir / AtomVM
    |
    | {lgfx, ProtoVer, Op, Target, Flags, ...}
    v
+------------------------------+
| port thread                  |
| - mailbox drain              |
| - term decode                |
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

This repository focuses on the port driver and its protocol boundary.

- `include/lgfx_port/`
  - public ESP-IDF headers

- `lgfx_port/cmake/lgfx_port_config.h.in`
  - template for the generated config header

- `lgfx_port/include_internal/lgfx_port/`
  - internal protocol metadata and helper headers
  - `ops.def`, `ops.h`
  - protocol helpers
  - capability/error headers
  - worker headers and canonical job definitions

- `lgfx_port/`
  - AtomVM-facing port implementation
  - request decode, metadata validation, dispatch
  - worker bridge

- `src/`
  - device-facing adapter layer
  - protocol-agnostic C/C++ around the pinned LovyanGFX submodule

- `docs/`
  - protocol and architecture docs

- `examples/elixir/`
  - example Elixir client

Minimal layout view:

```text
.
├── include/lgfx_port/
├── lgfx_port/
│   ├── cmake/
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

## Build-time configuration

This component uses a generated config header so the protocol layer and device layer see the same wiring, geometry, and LovyanGFX settings.

- Template:
  - `lgfx_port/cmake/lgfx_port_config.h.in`

- Generated output:
  - `<build>/.../generated/lgfx_port/lgfx_port_config.h`

How it works:

- `CMakeLists.txt` exposes cache variables and options.
- The parent ESP-IDF project can set them directly or via `idf.py -D...`.
- CMake runs `configure_file(... @ONLY)` to generate the header.
- The generated include directory is exported so all component code sees the same build knobs.

Important rule:

- The generated config header is the source of truth for build knobs used by this component.
- Do not rewrite the `@VAR@` tokens in `lgfx_port_config.h.in`.
- If a formatter changes them, CMake substitution breaks.

## Metadata-driven protocol surface

The protocol-visible operation surface is defined by:

- `lgfx_port/include_internal/lgfx_port/ops.def`

That metadata drives:

- operation atom registration

- dispatch table entries

- validation rules
  - arity
  - allowed flags
  - target policy
  - init-state policy

- capability linkage used by `getCaps`

- op gating based on real build/runtime support

- generated documentation tables in `docs/LGFX_PORT_PROTOCOL.md`

Worker jobs follow the same pattern:

- `lgfx_port/include_internal/lgfx_port/worker_jobs.def`

That job list drives:

- worker job enum values
- union members in `lgfx_job_t`
- worker dispatch bodies

The goal is simple: one declarative list, many synchronized outputs.

## Execution model

There are two C-side execution paths.

### Port thread path

Files:

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

Handlers are intentionally narrow:

- decode op-specific wire arguments
- enforce obvious wire-level constraints
- call synchronous `lgfx_worker_device_*` wrappers

### Worker / device path

Files:

- `lgfx_port/lgfx_worker_core.c`
- `lgfx_port/lgfx_worker_device.c`
- `src/`

Responsibilities:

- execute device work through a worker task and queue
- bridge handlers to plain C job payloads
- call the device adapter in `src/`
- keep protocol code separate from hardware code

The device layer is the authority for device-facing semantics such as:

- target existence and resolution
- deterministic sprite allocation at a chosen handle
- destination-aware sprite push behavior
- `pushImage` payload validity
- rotate/zoom semantic validity

This split keeps protocol behavior deterministic while isolating device execution.

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
- start worker state via `lgfx_worker_start(...)`

Teardown:

- happens when AtomVM marks the context as `Killed`
- per-context teardown runs from the native handler
- device close is attempted before worker stop
- worker state is stopped and freed
- per-port state is then released

This avoids freeing per-port state from the driver-global `destroy` callback.

## Binary payload ownership

Some operations carry binary payloads such as UTF-8 text or RGB565 pixel data.

Architecture rule:

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
