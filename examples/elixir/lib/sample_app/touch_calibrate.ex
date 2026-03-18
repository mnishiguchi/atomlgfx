# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.TouchCalibrate do
  @moduledoc false

  def run(port, _w, _h) do
    with {:ok, true} <- LGFXPort.supports_touch?(port),
         {:ok, params} <- LGFXPort.calibrate_touch(port),
         :ok <- LGFXPort.set_touch_calibrate(port, params) do
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
