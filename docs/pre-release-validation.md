<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# Pre-release validation checklist

AtomLGFX has not assigned a package release version. This checklist validates a
development milestone without implying a SemVer release. The Elixir wrapper
and native ESP-IDF component must come from the same Git revision and implement
the same wire-protocol version.

## Prepare the milestone

- Confirm the current API milestone and wire-protocol version are documented.
- Keep user-facing work under the changelog's `Unreleased` heading.
- Treat the `0.1.0` value required by Mix as placeholder metadata, not a
  published project version.
- Confirm generated protocol files are synchronized.
- Confirm the worktree is clean and CI is green.

## Validate the Elixir package

```bash
mix deps.get
mix format --check-formatted
mix compile --warnings-as-errors
mix test
mix docs --warnings-as-errors
mix hex.build
```

Run the example gates independently because the example project has its own
formatter and dependencies:

```bash
cd examples/elixir
mix deps.get
mix format --check-formatted
mix compile --warnings-as-errors
```

Inspect the locally built Hex archive as a packaging smoke test. It should
contain the Elixir source, README, changelog, licenses, and current user
documentation. It must not imply that the native component is embedded in the
archive or that the package has been released.

## Validate the native component

Initialize the pinned LovyanGFX source and build a complete AtomVM firmware
image. A wrapper-only application flash does not validate native changes.

```bash
git submodule sync --recursive
git submodule update --init --recursive
./scripts/atomvm_esp32.exs build --target esp32s3 --component .
```

The helper defaults to an exact AtomVM `release-0.7` commit, not the moving
branch head. Update that pin deliberately and repeat the complete native and
hardware gates whenever the AtomVM revision changes. On dual-core Xtensa
targets, the helper also selects ESP-IDF's software-backed `stdatomic`
implementation so AtomVM SMP remains enabled with the ESP-IDF 5.5 toolchain.
The component's `sdkconfig.defaults` raises the scheduler stack to 8 KB; the
ESP-IDF default overflows in the pinned AtomVM build before the application
starts on the tested dual-core ESP32-S3.

Use the board-specific target and configuration for the validation device. Review
native compiler warnings and confirm the generated configuration includes the
expected panel, bus, touch, and PSRAM settings.

## Validate hardware

Flash the newly built native firmware before flashing the example application.
Run at least:

- `SAMPLE_APP_MODE=all`
- `SAMPLE_APP_MODE=moving_icons`

Record:

- exact Git commit
- AtomVM revision and ESP-IDF version
- board, panel, resolution, and wiring/config profile
- PSRAM availability and whether native sprite PSRAM support is enabled
- smoke-test results and any capability-based skips
- MovingIcons object count, target FPS, measured FPS, and visual result
- known limitations such as a JPEG decoder or panel-specific issue

## Preserve the pre-release model

After all gates pass:

1. Commit the validation report without dating the `Unreleased` changelog
   heading.
2. Push the development branch when desired and confirm CI on the exact commit.
3. Keep Git dependencies pinned to that commit for both the wrapper and native
   component.
4. Do not create a SemVer tag or publish Hex until the project adopts an
   explicit package-version policy.

Historical date tags may continue to identify snapshots, but they are not
package releases and do not assign an API version.
