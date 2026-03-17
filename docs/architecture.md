# Architecture

`atomlgfx` is split into two closely related deliverables:

- a native ESP-IDF component built around the `lgfx_port` AtomVM port driver
- an Elixir package that provides the `LGFXPort` wrapper for that driver
  This document gives the top-level map only. Detailed protocol rules live in
  [the protocol spec](protocol.md). Port-thread and worker details live in
  [the `lgfx_port` README](../lgfx_port/README.md).

## Big picture

```text
Elixir / AtomVM
    |
    | {lgfx, ProtoVer, Op, Target, Flags, ...}
    v
+------------------------------+
| lgfx_port/                   |
| - request decode             |
| - metadata validation        |
| - handler dispatch           |
| - reply encode               |
+---------------+--------------+
                |
                | plain C job
                v
+------------------------------+
| worker task                  |
| - execute device work        |
| - free copied payloads       |
| - notify waiting caller      |
+---------------+--------------+
                |
                v
+------------------------------+
| lgfx_device/                 |
| - thin LovyanGFX adapter     |
| - target resolution          |
| - device semantics           |
+---------------+--------------+
                |
                v
           LovyanGFX
```

## Repository roles

- `include/lgfx_port/`
  - public native headers

- `lgfx_port/`
  - AtomVM-facing port layer
  - request envelope handling
  - metadata-driven validation
  - handler dispatch
  - worker bridge

- `lgfx_device/`
  - LovyanGFX-facing adapter layer
  - protocol-agnostic device operations
  - pinned-submodule integration

- `lib/`
  - root Elixir wrapper package
  - high-level `LGFXPort` API

- `examples/elixir/`
  - example application that consumes the root package

## Design boundaries

### `lgfx_port/`

This layer owns protocol-facing responsibilities:

- request tuple decoding
- op lookup and validation
- handler dispatch
- reply encoding
- protocol-visible error mapping

It should not own detailed LovyanGFX semantics.

### `lgfx_device/`

This layer owns device-facing responsibilities:

- target resolution
- sprite existence and allocation rules
- palette-backed behavior
- image and JPEG device semantics
- final conversion to the pinned LovyanGFX call surface

It should not decode AtomVM terms or build protocol replies.

### `lib/`

This layer owns Elixir-facing responsibilities:

- wrapper API shape
- Elixir-side validation and normalization
- convenience helpers
- wrapper-local caching and ergonomics

It should not redefine the native protocol contract.

## Build defaults and runtime overrides

Build defaults come from CMake cache variables and are emitted into the generated config header used by the native component.

- template:
  - `lgfx_port/cmake/lgfx_port_config.h.in`

- generated output:
  - `<build>/.../generated/lgfx_port/lgfx_port_config.h`

Selected values may also be overridden per port at `open_port/2`. In practice:

- build defaults define the baseline
- open-time config may override selected fields per port
- `init` applies the calling port's stored snapshot

## Metadata-driven surface

The protocol-visible operation surface is declared in:

- `lgfx_port/include_internal/lgfx_port/ops.def`

That metadata drives:

- operation registration
- validation rules
- dispatch surface
- capability linkage
- generated tables in `docs/protocol.md`

Worker jobs follow the same pattern:

- `lgfx_port/include_internal/lgfx_port/worker_jobs.def`

The design goal is one declarative source for each surface, with generated or synchronized outputs around it.

## Ownership model

The current native design separates configuration persistence from live device ownership:

- open-time configuration is stored per port context
- the live LCD device remains singleton-backed
- only one port may own the live device at a time

This keeps per-port configuration explicit without pretending the underlying hardware is multi-instance.

## Binary payload rule

Variable-length payloads such as text, JPEG data, and RGB565 image data must not cross the worker boundary by borrowing caller-owned term memory.

Current rule:

- copy variable-length payloads before enqueue
- execute device work
- free the copied payload after completion

The detailed worker and lifetime model is documented in `lgfx_port/README.md`.

## Pinned LovyanGFX policy

`atomlgfx` targets the pinned LovyanGFX submodule used by this repository.

Policy:

- prefer direct integration over compatibility probing
- update deliberately when the pinned submodule changes
- keep compatibility scaffolding minimal

## Where to read next

- [`docs/protocol.md`](protocol.md)
  - tuple protocol contract

- [`lgfx_port/README.md`](../lgfx_port/README.md)
  - port layer, worker model, ownership, and job flow

- [`lgfx_device/README.md`](../lgfx_device/README.md)
  - device adapter responsibilities and file map
