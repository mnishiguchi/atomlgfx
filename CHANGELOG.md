<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Changelog

All notable user-facing changes for `atomlgfx` are tracked here.

This project is still pre-release. Entries describe branch-level milestones,
not published package versions. Wire protocol v3 is the maintained versioned
compatibility contract.

## Unreleased

### Added

- Added the v3 low-memory call protocol for ordinary LovyanGFX operations.
- Added generated operation metadata, protocol reference tables, and Elixir validation metadata derived from `ops.def`.
- Added explicit binary batching through `AtomLGFX.BinaryBatch` and `AtomLGFX.submit_binary_batch/2`.
- Added render-first helpers `AtomLGFX.render/3`, `AtomLGFX.render_lcd/3`,
  `AtomLGFX.render_to/4`, and `AtomLGFX.render_sprite/4`.
- Added render command normalization for RGB tuple colors, palette colors, and
  named text datum values.
- Added a render-first sprite example mode for drawing into sprites and pushing
  them to the LCD.
- Added capability discovery for binary batch support.
- Added the familiar direct `AtomLGFX.draw_pixel/4` and `/5` facade while
  retaining render-first guidance for pixel loops.

### Changed

- Reworked the native port layer around flatter metadata-driven dispatch.
- Clarified protocol semantics for targets, colors, palettes, sprites, text, JPEG, and image payloads.
- Simplified the example application around smoke checks, performance checks, and a separate advanced MovingIcons renderer.
- Updated the text examples to use the render-first API for ordinary drawing.
- Split `AtomLGFX.BinaryBatch` into a stable public facade backed by narrower internal codec, submission, validation, and diagnostics modules.
- Isolated the MovingIcons hot loop in an example-local renderer that submits compact transformed-sprite lists without adding animation-specific commands to the friendly API. The renderer uses overlap-aware dynamic cleanup bounds to reduce visible flicker without allocating a frame buffer.
- Hardened native render-batch execution with compact stream argument checks before hot-path execution.
- Simplified native render-batch dispatch so public boundaries validate streams once and the inner stream walker only parses or executes commands.
- Replaced the native `term_from_int32` compatibility macro with an explicit `lgfx_term_from_i32` helper.
- Removed submission, validation, and diagnostics delegates from the internal `BinaryBatch.Codec` module so it only owns codec responsibilities.
- Split normalized render-command encoding into a focused internal encoder,
  keeping the public render helpers centered on encode/submit flow.
- Simplified the internal render encoder so unsupported normalized commands are
  rejected explicitly instead of relying on function-clause exceptions.
- Made render option validation explicit and replaced custom display-command scanning with a direct `:lists.member/2` check.
- Tightened internal render submission so `validate:` must be a boolean instead
  of relying on truthiness.
- Harmonized direct `set_text_datum/3` with render commands so common datum
  atoms and numeric LovyanGFX values are both accepted.
- Pinned the default AtomVM release revision and stabilized dual-core ESP32-S3
  builds with ESP-IDF's software-backed atomics and an 8 KB scheduler stack.
- Replaced the malformed JPEG smoke fixture and made direct and scaled JPEG
  rendering mandatory in hardware validation.

### Removed

- Removed the experimental retained native render-program API to reduce OOM risk.
- Removed the old tuple/list batch runtime and command-builder modules.
- Removed the old tuple/list batch builder and submission path.
- Removed the separate native `lgfx_runtime` command-dispatch layer.

### Compatibility

- Wire protocol v3 is not compatible with v1 or v2 native drivers.
- The native driver and Elixir wrapper must come from the same Git revision.
- Public low-level Elixir operation names use canonical `snake_case` atoms.
- LovyanGFX-style `camelCase` atoms are not part of the v3 Elixir-facing API.

## v1 baseline

The v1 baseline is the original implementation that now lives on the `v1`
branch. Historically, it was on `main` around commit `67e487f`.

Use the current migration guide when moving example applications or downstream
code to the v3 API. The original v1-to-v2 guide is retained as a historical
record.
