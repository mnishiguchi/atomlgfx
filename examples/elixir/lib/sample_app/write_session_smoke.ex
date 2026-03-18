# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.WriteSessionSmoke do
  @moduledoc false

  @bg 0x000000
  @fg 0xFFFFFF

  def run(port) do
    with :ok <- LGFXPort.start_write(port),
         :ok <- LGFXPort.start_write(port),
         :ok <- LGFXPort.fill_screen(port, @bg),
         :ok <- LGFXPort.set_text_color(port, @fg, nil, 0),
         :ok <- LGFXPort.draw_string(port, 8, 8, "write smoke", 0),
         :ok <- LGFXPort.end_write(port),
         :ok <- LGFXPort.end_write(port),
         :ok <- LGFXPort.end_write(port) do
      IO.puts("write session smoke ok")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("write session smoke failed: #{LGFXPort.format_error(reason)}")
        err
    end
  end
end
