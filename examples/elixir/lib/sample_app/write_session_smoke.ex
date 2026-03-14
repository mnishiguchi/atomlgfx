defmodule SampleApp.WriteSessionSmoke do
  @moduledoc false

  alias LGFXPort, as: Port

  @bg 0x000000
  @fg 0xFFFFFF

  def run(port) do
    with :ok <- Port.start_write(port),
         :ok <- Port.start_write(port),
         :ok <- Port.fill_screen(port, @bg),
         :ok <- Port.set_text_color(port, @fg, nil, 0),
         :ok <- Port.draw_string(port, 8, 8, "write smoke", 0),
         :ok <- Port.end_write(port),
         :ok <- Port.end_write(port),
         :ok <- Port.end_write(port) do
      IO.puts("write session smoke ok")
      :ok
    else
      {:error, reason} = err ->
        IO.puts("write session smoke failed: #{Port.format_error(reason)}")
        err
    end
  end
end
