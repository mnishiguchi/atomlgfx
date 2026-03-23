<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Elixir package

This repository provides an Elixir package named `AtomLGFX` for AtomVM.

It is a wrapper around the native `lgfx_port` driver in this repository. The
package provides the Elixir-facing API, convenience helpers, and wrapper-side
normalization on top of the shared tuple protocol.

## Requirements

This package depends on AtomVM firmware that includes the native `lgfx_port` driver.

The native driver and this Elixir package are developed together in the same
repository and should be kept in sync, especially while the project remains
pre-release.

## Installation

Add `atomlgfx` to your dependencies in `mix.exs`.

```elixir
defp deps do
  [
    {:atomlgfx, git: "https://github.com/mnishiguchi/atomlgfx.git", branch: "main"}
  ]
end
```

Then fetch dependencies.

```bash
mix deps.get
```

## Basic usage

```elixir
{:ok, port} = AtomLGFX.open(panel_driver: :ili9488, width: 320, height: 480)

:ok = AtomLGFX.init(port)
:ok = AtomLGFX.display(port)

:ok = AtomLGFX.fill_screen(port, 0x000000)
:ok = AtomLGFX.set_text_font_preset(port, :jp)
:ok = AtomLGFX.set_text_size(port, 2)
:ok = AtomLGFX.set_text_color(port, 0xFFFFFF, 0x000000)

:ok = AtomLGFX.draw_string(port, 16, 16, "こんにちは")
:ok = AtomLGFX.draw_string(port, 16, 56, "日本語テキスト")

:ok = AtomLGFX.display(port)
```

Adjust options and function calls to match your board and target device.

## API shape

The package wraps the shared native protocol rather than redefining it.

At a high level:

- Elixir code calls `AtomLGFX`
- the wrapper builds and sends protocol requests
- the native driver executes the request
- replies are returned as `{ok, result}` or `{error, reason}`

The underlying request shape is:

```erlang
{lgfx, ProtoVer, Op, Target, Flags, ...}
```

For the full native contract, see [the protocol document](protocol.md).

## Scope boundary

This package owns:

- Elixir-facing API shape
- convenience helpers
- wrapper-side validation and normalization
- wrapper-local caching and ergonomics

This package does not own:

- native request dispatch
- LovyanGFX device semantics
- protocol contract definition

For those, see:

- [Architecture](architecture.md)
- [Protocol](protocol.md)
- [Port layer](../lgfx_port/README.md)
- [Device adapter layer](../lgfx_device/README.md)

## Status

The project is usable but still pre-release.

Until the first tagged release, update the Elixir package and the native driver together.
