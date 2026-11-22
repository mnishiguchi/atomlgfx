# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

# Optional shortcuts for building AtomLGFX firmware and flashing the Elixir demo.
# The source of truth remains scripts/atomvm_esp32.exs and Mix/ExAtomVM.
set shell := ["sh", "-eu", "-c"]

target := env_var_or_default("ATOMLGFX_TARGET", "esp32s3")
port := env_var_or_default("ATOMLGFX_PORT", "/dev/ttyACM0")

@default:
    just --list

# Fetch the pinned LovyanGFX submodule (does not build or flash).
@setup:
    git submodule sync --recursive
    git submodule update --init --recursive

# Inspect the ESP-IDF/EIM, AtomVM, target, and component configuration.
@info:
    elixir scripts/atomvm_esp32.exs info --target {{ target }} --component .

# Build AtomVM firmware with the native AtomLGFX NIF; do not flash.
@build-firmware:
    elixir scripts/atomvm_esp32.exs build --target {{ target }} --component .

# Build and flash custom AtomVM firmware (overwrites existing firmware).
@install-firmware:
    elixir scripts/atomvm_esp32.exs install --target {{ target }} --component . --port {{ port }}

# Flash already-built AtomVM firmware (no rebuild).
@flash-firmware:
    elixir scripts/atomvm_esp32.exs flash --target {{ target }} --port {{ port }}

# Create a distributable custom AtomVM image.
@build-image:
    elixir scripts/atomvm_esp32.exs mkimage --target {{ target }} --component . --out-dir ./build/atomvm-images

# Show the native component symlink in the AtomVM source tree.
@component-status:
    elixir scripts/atomvm_esp32.exs component-status --component .

# Fetch dependencies of the bundled Elixir example application.
@app-deps:
    cd examples/elixir && mix deps.get

# Flash only the Elixir application (.avm); requires custom firmware first.
# Cleaning is intentional: SAMPLE_APP_MODE is a compile-time choice.
@flash-app mode="nif":
    cd examples/elixir && mix deps.get && mix clean && SAMPLE_APP_MODE={{ mode }} mix atomvm.esp32.flash --port {{ port }}

# Open the ESP32 serial monitor (requires tio).
@monitor:
    tio -b 115200 {{ port }}

# Run local Elixir tests (does not compile ESP-IDF or check real hardware).
@test:
    mix test

# Check Elixir formatting, compilation, tests and generated protocol docs.
@check:
    mix deps.get
    mix format --check-formatted
    mix compile --warnings-as-errors
    mix test
    elixir scripts/sync_lgfx_protocol_doc.exs --check
    cd examples/elixir && mix deps.get && mix format --check-formatted && mix compile --warnings-as-errors
