defmodule SampleApp.TouchCalibrate do
  @moduledoc false

  alias LGFXPort, as: Port

  def run(port, _w, _h) do
    with {:ok, true} <- Port.supports_touch?(port),
         {:ok, params} <- Port.calibrate_touch(port),
         :ok <- Port.set_touch_calibrate(port, params) do
      IO.puts("touch_calibrate ok params=#{inspect(params)}")
      :ok
    else
      {:ok, false} ->
        {:error, :cap_touch_missing}

      {:error, reason} ->
        {:error, reason}
    end
  end
end
