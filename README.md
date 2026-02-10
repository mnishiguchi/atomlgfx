# LovyanGFX AtomVM Port Driver

ESP-IDF component that exposes a **LovyanGFX** display driver to **AtomVM
Elixir** via an **Erlang term (tuple) port protocol**.

- Host and driver communicate with Erlang terms (tuples / atoms / integers / binaries).
- Large payloads (`pushImage`, text strings) use binaries for throughput.
- The protocol surface is documented and kept aligned with source metadata.

## Documentation

- [Protocol specification](docs/LGFX_PORT_PROTOCOL.md)
- [Architecture](docs/ARCHITECTURE.md)

## What this repository provides

This repository focuses on the **port driver** and its protocol boundary.

- `ports/`
  - AtomVM port driver implementation (term protocol, dispatch, validation, worker integration)
- `src/`
  - Device-facing LovyanGFX adapter layer (protocol-agnostic)
- `docs/`
  - Protocol and architecture documentation
- `examples/elixir/`
  - Example Elixir client for exercising the port driver

## Protocol overview

The wire contract is tuple-based and synchronous (`port:call` style).

- Request
  - `{lgfx, ProtoVer, Op, Target, Flags, ...}`
- Response
  - `{ok, Result}` or `{error, Reason}`

See the full protocol contract and operation matrix in:

- [docs/LGFX_PORT_PROTOCOL.md](docs/LGFX_PORT_PROTOCOL.md)

## Runtime model

The protocol surface is synchronous, but device-facing execution is performed
through an internal worker task/queue.

- `ports/lgfx_port.c`
  - AtomVM port entrypoint
  - Envelope decode, request decode/validation, dispatch, reply flow
  - Per-context teardown when the AtomVM context is marked `Killed`
- `ports/lgfx_worker.c`
  - Worker queue/task lifecycle
  - Device execution helpers
  - Mailbox draining support

Lifecycle notes:

- `close()` is implemented and idempotent
- Per-port cleanup is also enforced during AtomVM context teardown (even if the host never calls `close()`)

## Source-of-truth metadata

The protocol-visible operation surface is defined from metadata:

- `ports/include/lgfx_port/ops.def`
  - Operation list
  - Arity rules
  - Flags mask rules
  - Target/state policy
  - Capability linkage (`feature_cap_bit`)

This keeps dispatch behavior, capability advertisement (`getCaps`), and protocol documentation aligned.

## Binary payload ownership model

Binary-backed operations that execute through the worker use a driver-owned payload model.

- Payloads are deep-copied before enqueue to the worker
- The worker owns and frees the copied payload
- Payload lifetime ends after the device call completes (and after any required completion barrier)

This prevents retaining pointers into Erlang term binaries after request handling returns.

## Documentation sync

Generated protocol tables in `docs/LGFX_PORT_PROTOCOL.md` are synchronized from source metadata.

- Sync script
  - `scripts/sync_lgfx_protocol_doc.exs`
- CI check
  - `.github/workflows/docs-protocol-check.yml`

Typical usage:

- `elixir scripts/sync_lgfx_protocol_doc.exs`
- `elixir scripts/sync_lgfx_protocol_doc.exs --check`

## Current focus

- Keep the protocol surface and `getCaps` capability advertisement aligned from metadata (`ops.def`)
- Keep the protocol documentation synchronized from source metadata
- Keep lifecycle and teardown behavior explicit and stable
- Keep the port driver behavior stable at the protocol boundary

## Status

This repository is under active development.

The term protocol is usable and intended to be the stable integration boundary for host-side Elixir code.
