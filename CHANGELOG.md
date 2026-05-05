<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Changelog

All notable user-facing changes for `atomlgfx` are tracked here.

This project is still pre-release. Until a versioned release is cut, entries describe branch-level milestones rather than published packages.

## Unreleased

### Added

- Added the v2 call-based protocol for ordinary LovyanGFX operations.
- Added generated operation metadata around `lgfx_port/include_internal/lgfx_port/ops.def`.
- Added generated protocol reference tables in `docs/protocol-reference.md`.
- Added generated Elixir operation metadata in `lib/atom_lgfx/generated.ex`.
- Derived Elixir protocol validation metadata from generated `ops.def` metadata instead of hand-maintained maps.
- Added explicit binary batching through `AtomLGFX.BinaryBatch` and `AtomLGFX.submit_binary_batch/2`.
- Added `AtomLGFX.BinaryBatch.batch/1` to combine packed command fragments into one binary stream.
- Added capability discovery for binary batch support.
- Added protocol freeze and generated metadata consistency tests.
- Added hardware smoke and performance notes for v2 validation.

### Changed

- Reworked the native port layer around a flatter metadata-driven dispatch structure.
- Kept ordinary operations synchronous: the call that performs the operation returns its success or failure.
- Moved grouped rendering work away from tuple/list command batches toward one explicit render command stream.
- Clarified target, color, palette, sprite, text, JPEG, and image payload semantics in the protocol documentation.
- Simplified the example application around smoke checks and performance checks.

### Removed

- Removed the old tuple/list batch runtime and command-builder modules.
- Removed the old `AtomLGFX.batch/0`, `AtomLGFX.Batch`, and `AtomLGFX.submit_batch/2` path.
- Removed the separate native `lgfx_runtime` command-dispatch layer.

### Compatibility

- v2 is not wire-compatible with v1 batch usage.
- The native driver and Elixir wrapper must be updated together.
- Public Elixir-facing low-level operation names are canonical `snake_case` atoms.
- LovyanGFX-style `camelCase` atoms are not part of the v2 Elixir-facing protocol; use canonical `snake_case` operation names.

## v1 baseline

The v1 baseline is the pre-v2 implementation that now lives on the `v1` branch. Historically, it was on `main` around commit `67e487f`.

Use the migration guide when moving example applications or downstream code from the v1 branch to the v2 `main` branch.
