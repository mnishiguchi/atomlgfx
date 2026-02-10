# Architecture

This repository is an **ESP-IDF component** that exposes **LovyanGFX** to **AtomVM Elixir** through an **Erlang term (tuple) port protocol**.

- Host ↔ driver messages are Erlang terms (tuples / atoms / integers / binaries / lists)
- Large pixel payloads are carried as binaries for throughput
- The protocol contract is documented in `docs/LGFX_PORT_PROTOCOL.md`
- Generated protocol tables (ops, capabilities, error reasons) are synchronized from C metadata

---

## Scope

This repository focuses on the **port driver** and its protocol boundary.

- `ports/` handles protocol decoding, validation, dispatch, lifecycle, and reply encoding
- `src/` handles device-facing LovyanGFX calls
- `docs/` defines and documents the protocol contract
- `examples/elixir/` provides a client example for exercising the driver

---

## Layout

```text
.
- .gitmodules
- CMakeLists.txt
- README.md
- docs/
  - ARCHITECTURE.md
  - LGFX_PORT_PROTOCOL.md
- examples/
  - elixir/
- ports/
  - include/
    - lgfx_port/
  - handlers/
  - lgfx_port.c
  - lgfx_worker.c
  - lgfx_atoms.c
  - proto_term_decode.c
  - proto_term_encode.c
  - dispatch_table.c
  - op_meta.c
  - validate.c
- scripts/
  - sync_lgfx_protocol_doc.exs
- src/
  - (device-facing LovyanGFX adapter files)
- third_party/
  - LovyanGFX/
```

Notes:

- `ports/` is the protocol-facing side (AtomVM mailbox, term decode, validation, dispatch, replies)
- `src/` is the device-facing side (LovyanGFX wrapper calls)
- `third_party/LovyanGFX/` is the vendored LovyanGFX dependency

---

## High-level execution model

The driver uses a **two-part C-side model**:

- **Port thread path** (`ports/lgfx_port.c`)
  - Runs as AtomVM native handler for the port context
  - Drains mailbox messages
  - Decodes request tuples
  - Applies shared validation from op metadata
  - Dispatches to operation handlers
  - Encodes and sends replies

- **Worker/device path** (`ports/lgfx_worker.c` + `src/`)
  - Executes device work through the worker layer
  - Owns worker task/queue lifecycle
  - Calls the device adapter layer in `src/`
  - Keeps protocol-facing code separated from device-facing code

This split keeps protocol logic deterministic and easy to reason about, while isolating hardware/device behavior behind the worker and device adapter layers.

---

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
    # synchronous request/response envelope
    |
    v
[ports/lgfx_port.c]
    |
    +--> [Envelope decode]
    +--> [Request decode]
    +--> [Shared validation (ops.def metadata)]
    +--> [Dispatch lookup]
    +--> [Handler]
              |
              v
        [ports/lgfx_worker.c]
              |
              v
        [src/ device adapter]
              |
              v
          [LovyanGFX]

(reply returns through the same path)
```

Notes:

- AtomVM `port:call/2` uses an envelope like `{'$call', {Pid, Ref}, Message}` and expects a reply `{Ref, Reply}` sent to `Pid`
- AtomVM v0.7.0-dev does not currently provide `$cast` semantics, so all requests are treated as `$call`

---

## Port lifecycle model

### Driver-global lifecycle

`REGISTER_PORT_DRIVER(...)` registers three hooks:

- `init`
- `destroy`
- `create_port`

Important distinction:

- `init` / `destroy` are **driver-global** lifecycle hooks
- They are **not** called once per port instance

This matters because the actual port instance state (`lgfx_port_t`) is per AtomVM `Context` and is stored in:

- `ctx->platform_data`

### Per-context lifecycle

Per-port-instance lifecycle is managed in `ports/lgfx_port.c`:

- `lgfx_port_create_port(...)`
  - Allocates `Context`
  - Allocates `lgfx_port_t`
  - Initializes atoms / last-error state
  - Starts the worker (`lgfx_worker_start`)
  - Stores `lgfx_port_t *` in `ctx->platform_data`
  - Sets `ctx->native_handler`

- `lgfx_port_native_handler(...)`
  - Normal path: drains the mailbox via `lgfx_worker_drain_mailbox(port)`
  - Teardown path: when `ctx->flags & Killed`, performs **per-context teardown**

This avoids the common mistake of trying to free per-port state from the driver-global `destroy` callback.

---

## Per-context teardown semantics

Per-context teardown is handled in `lgfx_port_teardown(Context *ctx)` and is intentionally ordered.

### Teardown order

1. Detach `ctx->platform_data`
   - Guards against accidental double teardown

2. Best-effort device close (if initialized)
   - Calls `lgfx_worker_device_close(port)` while the worker is still alive
   - Teardown continues even if device close fails

3. Reset local lifecycle/cache state
   - `initialized = false`
   - `width = 0`
   - `height = 0`
   - clear `last_error`

4. Stop worker resources
   - `lgfx_worker_stop(port)` handles worker task/queue shutdown

5. Free `lgfx_port_t`
   - Also clears the back-reference `port->ctx` before free (debug hygiene)

### Why this order matters

The device close path runs through the worker queue, so the worker must still be running when close is requested. Stopping the worker too early can make device cleanup impossible or racy.

---

## Port driver layer (`ports/`)

The `ports/` layer is the C-side protocol boundary for:

- AtomVM envelope handling
- Tuple decoding and type checks
- Metadata-driven validation
- Operation dispatch
- Reply encoding
- Per-context lifecycle and teardown
- Worker coordination

### Source-of-truth metadata

The protocol surface is metadata-driven.

- `ports/include/lgfx_port/ops.def`
  - Canonical x-macro list of supported operations
  - Defines protocol-visible metadata:
    - operation atom
    - handler function
    - arity range
    - flags mask
    - target policy
    - state policy
    - `feature_cap_bit`

  - Drives dispatch behavior and `getCaps` feature advertisement

- `ports/include/lgfx_port/caps.h`
  - Canonical capability bit definitions (`LGFX_CAP_*`)
  - Used by `getCaps` and protocol documentation generation

- `ports/include/lgfx_port/errors.h`
  - Canonical protocol error atoms (`LGFX_ERR_*`)
  - Optional detail tags
  - Used by reply encoding and protocol documentation generation

### Key source files

- `ports/lgfx_port.c`
  - Port entrypoint glue and message processing
  - Per-context create/teardown lifecycle
  - Native handler (`Killed` teardown path + mailbox draining)

- `ports/proto_term_decode.c`
  - Request tuple decoding helpers and argument type checks
  - Decodes `{lgfx, ProtoVer, Op, Target, Flags, ...}` into an internal request struct

- `ports/validate.c`
  - Shared validation helpers driven by op metadata
  - Arity / flags / target / state policy enforcement

- `ports/dispatch_table.c`
  - Maps op atoms to handler functions via `ops.def`

- `ports/op_meta.c`
  - Protocol operation metadata lookup (`ops.def`-driven)

- `ports/handlers/*.c`
  - Operation handlers grouped by feature area (control / setup / primitives / text / images)

- `ports/proto_term_encode.c`
  - Reply tuple constructors:
    - `{ok, Value}`
    - `{error, Reason}`
    - `{error, {Reason, Detail}}`

- `ports/lgfx_worker.c`
  - Worker task/queue lifecycle
  - Worker job execution
  - Bridge from handlers to device-facing calls

### Header namespace

All public headers for the port driver live under `lgfx_port/`:

- `ports/include/lgfx_port/lgfx_port.h`
- `ports/include/lgfx_port/worker.h`
- `ports/include/lgfx_port/term_decode.h`
- `ports/include/lgfx_port/term_encode.h`
- `ports/include/lgfx_port/dispatch_table.h`
- `ports/include/lgfx_port/op_meta.h`
- `ports/include/lgfx_port/validate.h`
- `ports/include/lgfx_port/errors.h`
- `ports/include/lgfx_port/caps.h`
- `ports/include/lgfx_port/handlers/*.h`

---

## Worker and device boundary

The worker layer exists to keep the port thread focused on protocol concerns.

### Responsibilities of the worker layer

- Start/stop worker task and queue
- Accept device work requests from handlers
- Execute device-facing calls in a controlled path
- Support lifecycle-safe cleanup (`device_close` before worker stop during teardown)

### Responsibilities of the device layer (`src/`)

- LovyanGFX-specific behavior
- Drawing operations
- Display/device initialization and close
- Device error translation at the boundary

Rule of thumb:

- If code is about **wire protocol shape, request validation, dispatch, or replies**, it belongs in `ports/`
- If code is about **drawing behavior or LovyanGFX calls**, it belongs in `src/`

This separation keeps the protocol contract stable while allowing device internals to evolve.

---

## Error handling model

The port layer normalizes protocol failures into a stable reply shape.

### Reply shape invariant

Responses are always protocol terms:

- `{ok, Result}`
- `{error, Reason}`

### OOM-safe reply fallback

`ports/lgfx_port.c` includes a best-effort safeguard (`ensure_valid_reply(...)`) for reply construction failures:

- If a handler/validator returns an invalid term (typically OOM during reply encoding),
  the port layer attempts to convert it to `{error, no_memory}`
- If even that allocation fails, the reply is dropped (invalid term cannot be sent)

This preserves protocol behavior under memory pressure as much as possible.

### Last-error tracking

The port state tracks protocol-facing error context (operation, reason, flags, target, optional `esp_err`), and the `getLastError` behavior is documented in `docs/LGFX_PORT_PROTOCOL.md`.

---

## Binary payloads and ownership

The protocol uses binaries for performance (for example `pushImage` pixel data and string payloads).

Architecture-level rules:

- Protocol decode happens on the port side
- The driver must not rely on Erlang binary pointers remaining valid beyond request handling unless lifetime is explicitly extended
- The worker/device path must ensure payload lifetime is safe for the duration of device consumption

The normative binary lifetime requirements are documented in:

- `docs/LGFX_PORT_PROTOCOL.md` → **Binary payload lifetime (implementation requirement)**

---

## Documentation sync (`scripts/`)

### `scripts/sync_lgfx_protocol_doc.exs`

Synchronizes generated sections in `docs/LGFX_PORT_PROTOCOL.md` from source metadata.

Generated tables:

- Operation matrix (from `ports/include/lgfx_port/ops.def`)
- Capability bits table (from `ports/include/lgfx_port/caps.h`)
- Error reasons table (from `ports/include/lgfx_port/errors.h`)

This reduces protocol/documentation drift and keeps the spec aligned with the implementation metadata.

---

## Example client (`examples/elixir/`)

`examples/elixir/` contains an example AtomVM Elixir client that exercises the port driver.

Useful for:

- Basic integration verification
- Protocol smoke checks
- Manual demo usage

The example client is **not** the protocol source of truth. The protocol contract is defined by:

- `ports/include/lgfx_port/ops.def`
- `docs/LGFX_PORT_PROTOCOL.md`

---

## Where is X?

- Protocol specification
  - `docs/LGFX_PORT_PROTOCOL.md`

- Architecture overview
  - `docs/ARCHITECTURE.md`

- Protocol doc sync script
  - `scripts/sync_lgfx_protocol_doc.exs`

- Canonical op metadata (x-macro)
  - `ports/include/lgfx_port/ops.def`

- Canonical error atoms / detail tags
  - `ports/include/lgfx_port/errors.h`

- Canonical capability bits
  - `ports/include/lgfx_port/caps.h`

- AtomVM port entrypoint / lifecycle / mailbox handling
  - `ports/lgfx_port.c`

- Worker task/queue and device call bridge
  - `ports/lgfx_worker.c`

- Tuple decode + validation
  - `ports/proto_term_decode.c`
  - `ports/validate.c`
  - `ports/handlers/*.c` (per-op checks)

- Dispatch logic
  - `ports/dispatch_table.c`
  - `ports/op_meta.c`

- Reply encoding
  - `ports/proto_term_encode.c`

- Device-facing LovyanGFX logic
  - `src/`

- Example Elixir client
  - `examples/elixir/`
