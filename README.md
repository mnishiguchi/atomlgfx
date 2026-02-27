# LovyanGFX AtomVM Port Driver

ESP-IDF component that exposes a **LovyanGFX** display driver to **AtomVM Elixir** via an **Erlang term (tuple) port protocol**.

- Host and driver communicate using Erlang terms (tuples / atoms / integers / binaries).
- Large payloads (pixel data, text strings) use binaries for throughput.
- The protocol surface is documented and kept aligned with source metadata.

## Documentation

- [Protocol specification](docs/LGFX_PORT_PROTOCOL.md)
- [Architecture](docs/ARCHITECTURE.md)

## Repository layout

- `include/lgfx_port/`
  - Public headers for this ESP-IDF component
- `lgfx_port/`
  - Port driver implementation (protocol decode/validation/dispatch + worker integration)
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

See the full contract and operation matrix in:

- [docs/LGFX_PORT_PROTOCOL.md](docs/LGFX_PORT_PROTOCOL.md)

## Protocol doc sync

Generated tables inside `docs/LGFX_PORT_PROTOCOL.md` are synchronized from source metadata.

- Script
  - `scripts/sync_lgfx_protocol_doc.exs`
- Typical usage
  - `elixir scripts/sync_lgfx_protocol_doc.exs`
  - `elixir scripts/sync_lgfx_protocol_doc.exs --check`

## Status

This repository is under active development.

The term protocol is usable and intended to be the stable integration boundary for host-side Elixir code.
