# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.NativeMovingIcons do
  @moduledoc false

  alias AtomLGFX.Native
  alias SampleApp.Assets

  # This is a native rendering baseline, not a replacement for the regular
  # Elixir/AtomLGFX animation. Native drawing runs on a FreeRTOS task.
  @object_count 50
  @stats_interval_ms 1_000

  def run(_handle, _w, _h) do
    case Native.moving_icons_start(
           Assets.icon(:info),
           Assets.icon(:alert),
           Assets.icon(:close),
           @object_count
         ) do
      :ok ->
        try do
          poll_stats(0, 0)
        after
          :ok = Native.moving_icons_stop()
        end

      error ->
        error
    end
  end

  defp poll_stats(previous_frames, previous_elapsed_ms) do
    Process.sleep(@stats_interval_ms)

    case Native.moving_icons_stats() do
      {:ok, {frames, elapsed_ms, last_frame_us}} ->
        interval_ms = max(elapsed_ms - previous_elapsed_ms, 1)
        fps = div(max(frames - previous_frames, 0) * 1000, interval_ms)
        average_fps = if elapsed_ms > 0, do: div(frames * 1000, elapsed_ms), else: 0

        IO.puts(
          "moving_icons_native stats " <>
            "obj_count=#{@object_count} fps=#{fps} average_fps=#{average_fps} " <>
            "frames=#{frames} elapsed_ms=#{elapsed_ms} " <>
            "last_frame_us=#{last_frame_us}"
        )

        poll_stats(frames, elapsed_ms)

      error ->
        error
    end
  end
end
