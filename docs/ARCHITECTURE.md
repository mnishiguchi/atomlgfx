# Architecture

## Summary

- Host ↔ driver messages are Erlang terms (tuples / atoms / integers / binaries).
- Large payloads are carried as binaries for throughput.
- The protocol contract is documented in `docs/LGFX_PORT_PROTOCOL.md`.
- Protocol-visible behavior is driven from metadata (`include/lgfx_port/ops.def`) to reduce drift.
- Hardware wiring and LovyanGFX tuning are configured at build time via a generated config header.

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
| - handler dispatch           |
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
+---------------+--------------+
                |
                v
           LovyanGFX
```

## Repository map

This repository focuses on the port driver and its protocol boundary.

- `include/lgfx_port/`
  - Public headers for the ESP-IDF component (what other components include)
- `include/lgfx_port/lgfx_port_config.h.in`
  - Build-time config template (CMake `configure_file` input)
- `lgfx_port/`
  - Protocol-facing port driver implementation (AtomVM mailbox, term decode, validation, dispatch, replies)
- `src/`
  - Device-facing LovyanGFX adapter layer (protocol-agnostic)
- `docs/`
  - Protocol and architecture documentation
- `examples/elixir/`
  - example Elixir client

Minimal layout view:

```text
.
├── CMakeLists.txt
├── include/
│   └── lgfx_port/
│       ├── lgfx_port.h
│       ├── lgfx_port_config.h.in
│       ├── ops.def
│       ├── ops.h
│       ├── proto_term.h
│       ├── validate.h
│       └── worker.h
├── lgfx_port/
│   ├── handlers/
│   │   ├── setup.c
│   │   ├── primitives.c
│   │   ├── text.c
│   │   ├── images.c
│   │   ├── sprites.c
│   │   └── touch.c
│   ├── lgfx_port.c
│   ├── lgfx_worker.c
│   └── proto_term.c
├── src/
└── docs/
    ├── ARCHITECTURE.md
    └── LGFX_PORT_PROTOCOL.md
```

Notes:

- Public headers live under `include/lgfx_port/` following the common ESP-IDF convention:
  - `<component_root>/include/<component_name>/*.h`

- Port implementation C sources live under `lgfx_port/`.

## Build-time configuration

This component uses a generated config header to keep **wiring**, **panel geometry**, and **LovyanGFX Bus/Panel knobs**
consistent across the protocol layer (`lgfx_port/*`) and the device layer (`src/*`).

- Template (committed)
  - `include/lgfx_port/lgfx_port_config.h.in`

- Generated output:
  - `<build>/esp-idf/atomlgfx/generated/lgfx_port/lgfx_port_config.h`

How it works:

- `CMakeLists.txt` exposes cache variables / options (set by the parent project or via `idf.py -D...`).
- During configure, CMake runs `configure_file(... @ONLY)` to substitute `@VARS@` into the generated header.
- The component adds the generated include directory as a **PUBLIC** include path so consumers that include
  `lgfx_port/lgfx_port.h` also see the config macros.

Important rule:

- Deterministic capability gating
  - `CAP_TOUCH` advertisement is computed in CMake (`LGFX_PORT_SUPPORTS_TOUCH`) so protocol behavior does not depend on
    ad-hoc compile definitions.

- Formatter hazard (real-world footgun)
  - The `@VAR@` tokens in `lgfx_port_config.h.in` must remain intact. If a formatter rewrites them (e.g. `@VAR @`),
    CMake substitution breaks and you get stray `@` in the generated header.

## High-level execution model

The driver is intentionally split into two C-side paths:

- **Port thread path** (`lgfx_port/lgfx_port.c`)
  - Runs as the AtomVM native handler for the port context
  - Drains mailbox messages
  - Decodes request tuples into internal request structs
  - Applies metadata-driven validation
  - Dispatches to operation handlers
  - Sends `{ok, Result}` / `{error, Reason}` replies

- **Worker/device path** (`lgfx_port/lgfx_worker.c` + `src/`)
  - Executes device work through a worker task/queue
  - Calls the device adapter layer in `src/`
  - Keeps protocol-facing code separated from device-facing code

This split keeps protocol logic deterministic while isolating hardware behavior behind a single execution path.

## Data flow

```text
[Host app (Elixir)]
  |
  v
[Request tuple]
  # {lgfx, ProtoVer, Op, Target, Flags, ...}
  |
  v
[AtomVM port:call]
  |
  v
[lgfx_port/lgfx_port.c]
  |
  +--> decode request tuple (lgfx_port/proto_term.c)
  +--> metadata validation (ops.def-driven)
  +--> dispatch to handler (lgfx_port/handlers/*.c)
          |
          v
    [lgfx_port/lgfx_worker.c]
          |
          v
    [src/ device adapter]
          |
          v
      [LovyanGFX]

(reply returns through the same path)
```

## Metadata-driven protocol surface

The protocol-visible operation surface is defined by:

- `include/lgfx_port/ops.def`

That metadata drives:

- Operation atoms (registered into the port’s atom table)
- Dispatch table mapping Op → handler function
- Shared validation rules (arity range, allowed flags, target policy, init-state policy)
- Capability linkage (`feature_cap_bit`) used by `getCaps`
- Documentation tables generated in `docs/LGFX_PORT_PROTOCOL.md`

When `ops.def` changes, the expectation is that:

- the implementation compiles with updated metadata, and
- the protocol documentation is re-synchronized via the existing script.

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

### Worker / device path

Files:

- `lgfx_port/lgfx_worker_core.c`
- `lgfx_port/lgfx_worker_device.c`
- `lgfx_port/lgfx_worker_mailbox.c`
- `src/`

Responsibilities:

- execute device work through a worker task and queue
- bridge handlers to plain C job payloads
- call the device adapter in `src/`
- keep protocol code separate from hardware code

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

Some operations carry binary payloads (pixel data, UTF-8 strings).

Architecture rule:

- The driver must not retain pointers into Erlang binaries past the request-handling boundary unless lifetime is explicitly managed.

Current model:

- Payloads are deep-copied into job-owned memory before enqueueing work to the worker.
- The worker frees the copied payload after the device call completes (and after any required completion barrier).

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

- Protocol metadata source of truth
  - `include/lgfx_port/ops.def`

- port entrypoint, lifecycle, dispatch:
  - `lgfx_port/lgfx_port.c`

- Worker task/queue and device call bridge
  - `lgfx_port/lgfx_worker.c`

- Request decode + reply tuple constructors
  - `lgfx_port/proto_term.c`

- handlers:
  - `lgfx_port/handlers/*.c`

- worker core:
  - `lgfx_port/lgfx_worker_core.c`

- worker wrappers:
  - `lgfx_port/lgfx_worker_device.c`

- mailbox drain bridge:
  - `lgfx_port/lgfx_worker_mailbox.c`

- device-facing adapter:
  - `src/`
