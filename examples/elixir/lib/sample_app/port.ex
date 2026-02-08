defmodule SampleApp.Port do
  @compile {:no_warn_undefined, :port}

  @port_name "lgfx_port"

  @opcode_ping 0x01
  @opcode_init 0x10
  @opcode_fill_screen 0x11

  def open do
    :erlang.open_port({:spawn_driver, @port_name}, [])
  end

  def ping(port) do
    :port.call(port, <<@opcode_ping>>, 5_000)
  end

  def init_display(port, rotation) when rotation in 0..7 do
    :port.call(port, <<@opcode_init, rotation::unsigned-8>>, 10_000)
  end

  def fill_screen(port, rgb565) when is_integer(rgb565) and rgb565 in 0..0xFFFF do
    :port.call(port, <<@opcode_fill_screen, rgb565::unsigned-16-big>>, 10_000)
  end
end
