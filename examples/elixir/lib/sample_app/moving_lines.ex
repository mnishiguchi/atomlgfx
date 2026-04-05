# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.MovingLines do
  @moduledoc false

  import SampleApp.AtomVMCompat, only: [yield: 0]

  alias AtomLGFX.Batch
  alias AtomLGFX.Batch.Command

  @bg 0x0000
  @front 0xFFFF
  @back 0x7BEF
  @edge 0x07FF
  @frame 0x4208
  @horizon 0x2104

  @frame_delay_ms 16

  @strip_sprite_target 30
  @strip_sprite_depth 16
  @initial_split_factor 2

  @angle_y_step 0.09
  @angle_z_step 0.05

  @cube_vertices [
    {-1.0, -1.0, -1.0},
    {1.0, -1.0, -1.0},
    {1.0, 1.0, -1.0},
    {-1.0, 1.0, -1.0},
    {-1.0, -1.0, 1.0},
    {1.0, -1.0, 1.0},
    {1.0, 1.0, 1.0},
    {-1.0, 1.0, 1.0}
  ]

  @cube_edges [
    {0, 1, @front},
    {1, 2, @front},
    {2, 3, @front},
    {3, 0, @front},
    {4, 5, @back},
    {5, 6, @back},
    {6, 7, @back},
    {7, 4, @back},
    {0, 4, @edge},
    {1, 5, @edge},
    {2, 6, @edge},
    {3, 7, @edge}
  ]

  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    base_size = max_i(18, div(min_i(w, h), 5))
    padding = base_size + 20

    cx = div(w, 2)
    cy = div(h, 2)

    vx = max_i(2, div(w, 120))
    vy = max_i(2, div(h, 160))

    try do
      with {:ok, true} <- AtomLGFX.supports_sprite?(port),
           {:ok, strip_h} <- prepare_strip_sprite(port, w, h) do
        IO.puts("moving_lines running mode=batch strip_sprite strip_h=#{strip_h}")

        loop(
          port,
          w,
          h,
          strip_h,
          base_size,
          padding,
          {cx, cy, vx, vy, 0.0, 0.0}
        )
      else
        {:ok, false} ->
          {:error, :cap_sprite_missing}

        {:error, reason} = err ->
          IO.puts("moving_lines failed: #{AtomLGFX.format_error(reason)}")
          err
      end
    after
      _ = safe_delete_sprite(port, @strip_sprite_target)
    end
  end

  defp prepare_strip_sprite(port, w, h) do
    _ = safe_delete_sprite(port, @strip_sprite_target)
    prepare_strip_sprite_i(port, w, h, @initial_split_factor)
  end

  defp prepare_strip_sprite_i(port, w, h, split_factor) do
    strip_h = max_i(1, div_ceil(h, split_factor))

    case AtomLGFX.create_sprite(port, w, strip_h, @strip_sprite_depth, @strip_sprite_target) do
      :ok ->
        {:ok, strip_h}

      {:error, reason} ->
        _ = safe_delete_sprite(port, @strip_sprite_target)

        if strip_h == 1 do
          {:error, {:strip_sprite_alloc_failed, w, h, split_factor, reason}}
        else
          prepare_strip_sprite_i(port, w, h, split_factor + 1)
        end
    end
  end

  defp loop(port, w, h, strip_h, base_size, padding, state) do
    with :ok <- render_frame_strips(port, w, h, strip_h, base_size, state),
         :ok <- AtomLGFX.display(port) do
      sleep_ms(@frame_delay_ms)
      yield()

      loop(port, w, h, strip_h, base_size, padding, next_state(w, h, padding, state))
    else
      {:error, reason} = err ->
        IO.puts("moving_lines failed: #{AtomLGFX.format_error(reason)}")
        err
    end
  end

  defp render_frame_strips(port, w, h, strip_h, base_size, {cx, cy, _vx, _vy, angle_y, angle_z}) do
    points = project_cube_points(cx, cy, base_size, angle_y, angle_z)
    render_frame_strips_i(port, w, h, strip_h, 0, points, cx, cy, base_size)
  end

  defp render_frame_strips_i(_port, _w, h, _strip_h, y0, _points, _cx, _cy, _base_size)
       when y0 >= h do
    :ok
  end

  defp render_frame_strips_i(port, w, h, strip_h, y0, points, cx, cy, base_size) do
    actual_h = min_i(strip_h, h - y0)

    with {:ok, batch} <- build_strip_batch(w, h, actual_h, y0, points, cx, cy, base_size),
         :ok <- submit_batch_ok(port, batch),
         :ok <- AtomLGFX.push_sprite(port, @strip_sprite_target, 0, y0) do
      render_frame_strips_i(port, w, h, strip_h, y0 + actual_h, points, cx, cy, base_size)
    end
  end

  defp build_strip_batch(w, panel_h, strip_h, y0, points, _cx, cy, base_size) do
    horizon_y = cy + base_size + 18
    local_horizon_y = horizon_y - y0
    right_x = w - 1
    top_local_y = 0 - y0
    bottom_local_y = panel_h - 1 - y0

    batch0 = Batch.new()

    with {:ok, batch1} <- add_batch_entry(batch0, Command.fill_screen(@bg, @strip_sprite_target)),
         {:ok, batch2} <-
           add_batch_entry(
             batch1,
             Command.line(0, top_local_y, right_x, top_local_y, @frame, @strip_sprite_target)
           ),
         {:ok, batch3} <-
           add_batch_entry(
             batch2,
             Command.line(
               right_x,
               top_local_y,
               right_x,
               bottom_local_y,
               @frame,
               @strip_sprite_target
             )
           ),
         {:ok, batch4} <-
           add_batch_entry(
             batch3,
             Command.line(
               right_x,
               bottom_local_y,
               0,
               bottom_local_y,
               @frame,
               @strip_sprite_target
             )
           ),
         {:ok, batch5} <-
           add_batch_entry(
             batch4,
             Command.line(0, bottom_local_y, 0, top_local_y, @frame, @strip_sprite_target)
           ),
         {:ok, batch6} <-
           add_batch_entry(
             batch5,
             Command.line(
               0,
               local_horizon_y,
               right_x,
               local_horizon_y,
               @horizon,
               @strip_sprite_target
             )
           ),
         {:ok, batch7} <- add_cube_edges(batch6, points, @cube_edges, y0) do
      {:ok, batch7}
    end
  end

  defp add_cube_edges(%Batch{} = batch, _points, [], _y0), do: {:ok, batch}

  defp add_cube_edges(%Batch{} = batch, points, [{a, b, color} | rest], y0) do
    {x0, y0_abs} = point_at(points, a)
    {x1, y1_abs} = point_at(points, b)

    with {:ok, next_batch} <-
           add_batch_entry(
             batch,
             Command.line(x0, y0_abs - y0, x1, y1_abs - y0, color, @strip_sprite_target)
           ) do
      add_cube_edges(next_batch, points, rest, y0)
    end
  end

  defp project_cube_points(cx, cy, base_size, angle_y, angle_z) do
    sin_y = :math.sin(angle_y)
    cos_y = :math.cos(angle_y)
    sin_z = :math.sin(angle_z)
    cos_z = :math.cos(angle_z)

    project_cube_points_i(@cube_vertices, cx, cy, base_size, sin_y, cos_y, sin_z, cos_z, [])
  end

  defp project_cube_points_i([], _cx, _cy, _base_size, _sin_y, _cos_y, _sin_z, _cos_z, acc) do
    :lists.reverse(acc)
  end

  defp project_cube_points_i(
         [{x, y, z} | rest],
         cx,
         cy,
         base_size,
         sin_y,
         cos_y,
         sin_z,
         cos_z,
         acc
       ) do
    x1 = x * cos_y + z * sin_y
    z1 = -x * sin_y + z * cos_y

    x2 = x1 * cos_z - y * sin_z
    y2 = x1 * sin_z + y * cos_z

    factor = base_size / (z1 + 4.0)

    px = round(cx + x2 * factor)
    py = round(cy + y2 * factor)

    project_cube_points_i(rest, cx, cy, base_size, sin_y, cos_y, sin_z, cos_z, [{px, py} | acc])
  end

  defp next_state(w, h, padding, {cx, cy, vx, vy, angle_y, angle_z}) do
    {cx2, vx2} = bounce_axis(cx + vx, vx, padding, w - 1 - padding)
    {cy2, vy2} = bounce_axis(cy + vy, vy, padding, h - 1 - padding)

    {cx2, cy2, vx2, vy2, angle_y + @angle_y_step, angle_z + @angle_z_step}
  end

  defp bounce_axis(pos, velocity, min_pos, max_pos) do
    cond do
      pos < min_pos ->
        {min_pos, abs_i(velocity)}

      pos > max_pos ->
        {max_pos, -abs_i(velocity)}

      true ->
        {pos, velocity}
    end
  end

  defp point_at(points, index) when is_list(points) and is_integer(index) and index >= 0 do
    :lists.nth(index + 1, points)
  end

  defp add_batch_entry(%Batch{} = batch, {:ok, command}) do
    add_batch_entry(batch, command)
  end

  defp add_batch_entry(%Batch{} = batch, {:cmd, _, _, _, _} = command) do
    case Batch.add(batch, command) do
      %Batch{} = next_batch -> {:ok, next_batch}
      {:error, _reason} = error -> error
    end
  end

  defp add_batch_entry(%Batch{}, {:error, _reason} = error), do: error

  defp submit_batch_ok(port, %Batch{} = batch) do
    case AtomLGFX.submit_batch(port, batch) do
      {:ok, _payload} ->
        :ok

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp safe_delete_sprite(port, sprite_target) do
    case AtomLGFX.delete_sprite(port, sprite_target) do
      :ok -> :ok
      {:error, _reason} -> :ok
    end
  end

  defp sleep_ms(ms) when is_integer(ms) and ms > 0 do
    receive do
    after
      ms -> :ok
    end
  end

  defp div_ceil(a, b) when is_integer(a) and is_integer(b) and a >= 0 and b > 0 do
    div(a + b - 1, b)
  end

  defp abs_i(v) when v < 0, do: -v
  defp abs_i(v), do: v

  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
