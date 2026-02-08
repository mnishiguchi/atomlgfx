defmodule SampleApp do
  def start do
    port = SampleApp.Port.open()
    IO.puts("Port opened")

    case SampleApp.Port.ping(port) do
      <<0x00, _rest::binary>> -> IO.puts("ping ok")
      other -> IO.puts("ping failed: #{inspect(other)}")
    end

    case SampleApp.Port.init_display(port, 1) do
      <<0x00, _rest::binary>> -> IO.puts("init ok")
      other -> IO.puts("init failed: #{inspect(other)}")
    end

    case SampleApp.Port.fill_screen(port, 0xF800) do
      <<0x00, _rest::binary>> -> IO.puts("fill ok")
      other -> IO.puts("fill failed: #{inspect(other)}")
    end

    loop(port, 0)
  end

  defp loop(port, n) do
    colors = [
      {"R", 0xF800},
      {"G", 0x07E0},
      {"B", 0x001F}
    ]

    {label, color} = Enum.at(colors, rem(n, length(colors)))
    IO.puts("fill #{label} (rgb565=0x#{Integer.to_string(color, 16)})")

    _ =
      case SampleApp.Port.fill_screen(port, color) do
        <<0x00, _rest::binary>> -> :ok
        other -> IO.puts("fill failed: #{inspect(other)}")
      end

    Process.sleep(1000)
    loop(port, n + 1)
  end
end
