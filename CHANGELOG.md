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
- Added generated operation metadata, protocol reference tables, and Elixir validation metadata derived from `ops.def`.
- Added explicit binary batching through `AtomLGFX.BinaryBatch` and `AtomLGFX.submit_binary_batch/2`.
- Added capability discovery for binary batch support.

### Changed

- Reworked the native port layer around flatter metadata-driven dispatch.
- Clarified protocol semantics for targets, colors, palettes, sprites, text, JPEG, and image payloads.
- Simplified the example application around smoke checks, performance checks, and binary frame-strip animation.

### Removed

- Removed the experimental retained native render-program API to reduce OOM risk.
- Removed the old tuple/list batch runtime and command-builder modules.
- Removed the old `AtomLGFX.batch/0`, `AtomLGFX.Batch`, and `AtomLGFX.submit_batch/2` path.
- Removed the separate native `lgfx_runtime` command-dispatch layer.

### Compatibility

- v2 is not wire-compatible with v1 batch usage.
- The native driver and Elixir wrapper must be updated together.
- Public low-level Elixir operation names use canonical `snake_case` atoms.
- LovyanGFX-style `camelCase` atoms are not part of the v2 Elixir-facing protocol.

## v1 baseline

The v1 baseline is the pre-v2 implementation that now lives on the `v1` branch. Historically, it was on `main` around commit `67e487f`.

Use the migration guide when moving example applications or downstream code from the v1 branch to the v2 `main` branch.
