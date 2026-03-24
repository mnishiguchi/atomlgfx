# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.WriteSessionSmoke do
  @moduledoc false

  @bg 0x0000
  @fg 0xFFFF

  def run(port) do
    with :ok <- AtomLGFX.start_write(port),
         :ok <- AtomLGFX.start_write(port),
         :ok <- AtomLGFX.fill_screen(port, @bg),
         :ok <- AtomLGFX.set_text_color(port, @fg, nil, 0),
         :ok <- AtomLGFX.draw_string(port, 8, 8, "write smoke", 0),
         :ok <- AtomLGFX.end_write(port),
         :ok <- AtomLGFX.end_write(port),
         :ok <- AtomLGFX.end_write(port) do
      IO.puts("write session smoke ok")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("write session smoke failed: #{AtomLGFX.format_error(reason)}")
        err
    end
  end
end
