# Architecture

This repository is an **ESP-IDF component** that exposes **LovyanGFX** to **AtomVM Elixir** through an **Erlang term (tuple) port protocol**.

- Host ↔ driver messages are Erlang terms (tuples / atoms / integers / binaries).
- Large payloads are carried as binaries for throughput.
- The protocol contract is documented in `docs/LGFX_PORT_PROTOCOL.md`.
- Protocol-visible behavior is driven from metadata (`include/lgfx_port/ops.def`) to reduce drift.
- Hardware wiring and LovyanGFX tuning are configured at build time via a generated config header.

## Scope

This repository focuses on the **port driver** and its protocol boundary.

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
  - Example Elixir client for exercising the driver

## Layout

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

- Generated output (build directory; not committed)
  - `<build>/esp-idf/atomlgfx/generated/lgfx_port/lgfx_port_config.h`

How it works:

- `CMakeLists.txt` exposes cache variables / options (set by the parent project or via `idf.py -D...`).
- During configure, CMake runs `configure_file(... @ONLY)` to substitute `@VARS@` into the generated header.
- The component adds the generated include directory as a **PUBLIC** include path so consumers that include
  `lgfx_port/lgfx_port.h` also see the config macros.

Key properties:

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

The protocol-visible operation surface is defined by a single x-macro list:

- `include/lgfx_port/ops.def`

That metadata is used to keep these in sync:

- Operation atoms (registered into the port’s atom table)
- Dispatch table mapping Op → handler function
- Shared validation rules (arity range, allowed flags, target policy, init-state policy)
- Capability linkage (`feature_cap_bit`) used by `getCaps`
- Documentation tables generated in `docs/LGFX_PORT_PROTOCOL.md`

When `ops.def` changes, the expectation is that:

- the implementation compiles with updated metadata, and
- the protocol documentation is re-synchronized via the existing script.

## Port lifecycle model

### Driver-global lifecycle

`REGISTER_PORT_DRIVER(...)` registers driver-global hooks:

- `init`
- `destroy`
- `create_port`

Important distinction:

- `init` / `destroy` are driver-global lifecycle hooks
- they are not called once per port instance

### Per-context lifecycle

Per-port-instance lifecycle is managed in `lgfx_port/lgfx_port.c`:

- Port creation allocates:
  - an AtomVM `Context`
  - a per-port `lgfx_port_t` stored in `ctx->platform_data`
  - worker task/queue state via `lgfx_worker_start(...)`

- Teardown happens when AtomVM marks the context as `Killed`:
  - per-context teardown runs from the native handler
  - device close is attempted before stopping the worker
  - worker is stopped and per-port state is freed

This avoids freeing per-port state from the driver-global `destroy` callback.

## Binary payload ownership

Some operations carry binary payloads (pixel data, UTF-8 strings).

Architecture-level rule:

- The driver must not retain pointers into Erlang binaries beyond the request handling boundary unless lifetime is explicitly managed.

Current implementation approach:

- Payloads are deep-copied into job-owned memory before enqueueing work to the worker.
- The worker frees the copied payload after the device call completes (and after any required completion barrier).

## Documentation sync

Generated tables in `docs/LGFX_PORT_PROTOCOL.md` are synchronized from source metadata.

- Script
  - `scripts/sync_lgfx_protocol_doc.exs`

- Typical usage
  - `elixir scripts/sync_lgfx_protocol_doc.exs`
  - `elixir scripts/sync_lgfx_protocol_doc.exs --check`

## Where to find things

- Protocol specification
  - `docs/LGFX_PORT_PROTOCOL.md`

- Protocol metadata source of truth
  - `include/lgfx_port/ops.def`

- Port entrypoint, lifecycle, dispatch, metadata validation
  - `lgfx_port/lgfx_port.c`

- Worker task/queue and device call bridge
  - `lgfx_port/lgfx_worker.c`

- Request decode + reply tuple constructors
  - `lgfx_port/proto_term.c`

- Handlers grouped by feature
  - `lgfx_port/handlers/*.c`

- Device-facing LovyanGFX adapter layer
  - `src/`
