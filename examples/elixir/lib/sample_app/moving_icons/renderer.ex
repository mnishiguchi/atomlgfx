# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.MovingIcons.Renderer do
  @moduledoc false

  alias AtomLGFX.BinaryBatch

  @lcd_target 0
  # The bundled icons are 32x32. Their half-diagonal rounds up to 23 pixels,
  # which safely bounds any rotation before applying the fixed-point zoom.
  @source_half_diagonal 23
  @erase_padding 2

  # The LCD is write-through, so clearing every previous icon before drawing any
  # current icon produces visible blank intervals. Isolated icons are erased and
  # redrawn immediately. Icons whose old/new bounds intersect are grouped so one
  # icon's erase cannot cut into another icon that was already redrawn.
  def render(handle, [], objects, _background, transparent) do
    instances = build_instances(objects, [])
    render_commands(handle, [transform_command(instances, transparent)])
  end

  def render(handle, previous_objects, objects, background, transparent) do
    entries = build_entries(previous_objects, objects, 0, [])
    {conflicting, isolated} = partition_entries(entries, entries, [], [])

    commands = [
      build_conflicting_commands(conflicting, background, transparent),
      build_isolated_commands(isolated, background, transparent, [])
    ]

    render_commands(handle, commands)
  end

  defp render_commands(handle, commands) do
    BinaryBatch.render(handle, [
      BinaryBatch.target(@lcd_target),
      commands,
      BinaryBatch.display()
    ])
  end

  defp transform_command(instances, transparent) do
    BinaryBatch.push_rotate_zoom_list(instances,
      transparent: transparent,
      approx_cull: true
    )
  end

  defp build_entries([], [], _index, acc), do: :lists.reverse(acc)

  defp build_entries([previous | previous_rest], [current | current_rest], index, acc) do
    entry =
      {index, previous, current, object_bounds(previous), object_bounds(current)}

    build_entries(previous_rest, current_rest, index + 1, [entry | acc])
  end

  defp partition_entries([], _all, conflicting, isolated) do
    {:lists.reverse(conflicting), :lists.reverse(isolated)}
  end

  defp partition_entries([entry | rest], all, conflicting, isolated) do
    if entry_conflicts?(entry, all) do
      partition_entries(rest, all, [entry | conflicting], isolated)
    else
      partition_entries(rest, all, conflicting, [entry | isolated])
    end
  end

  defp entry_conflicts?(_entry, []), do: false

  defp entry_conflicts?(
         {index, _previous, _current, _previous_bounds, _current_bounds} = entry,
         [
           {index, _other_previous, _other_current, _other_previous_bounds, _other_current_bounds}
           | rest
         ]
       ) do
    entry_conflicts?(entry, rest)
  end

  defp entry_conflicts?(
         {_index, _previous, _current, previous_bounds, current_bounds} = entry,
         [
           {_other_index, _other_previous, _other_current, other_previous_bounds,
            other_current_bounds}
           | rest
         ]
       ) do
    if bounds_intersect?(previous_bounds, other_current_bounds) or
         bounds_intersect?(other_previous_bounds, current_bounds) or
         bounds_intersect?(current_bounds, other_current_bounds) do
      true
    else
      entry_conflicts?(entry, rest)
    end
  end

  defp build_conflicting_commands([], _background, _transparent), do: []

  defp build_conflicting_commands(entries, background, transparent) do
    {clear_commands, instances} = build_conflicting_payload(entries, background, [], [])
    [clear_commands, transform_command(instances, transparent)]
  end

  defp build_conflicting_payload([], _background, clear_acc, instance_acc) do
    {:lists.reverse(clear_acc), :lists.reverse(instance_acc)}
  end

  defp build_conflicting_payload(
         [{_index, previous, current, _previous_bounds, _current_bounds} | rest],
         background,
         clear_acc,
         instance_acc
       ) do
    build_conflicting_payload(
      rest,
      background,
      [dynamic_clear_command(previous, background) | clear_acc],
      [build_instance(current) | instance_acc]
    )
  end

  defp build_isolated_commands([], _background, _transparent, acc) do
    :lists.reverse(acc)
  end

  defp build_isolated_commands(
         [{_index, previous, current, _previous_bounds, _current_bounds} | rest],
         background,
         transparent,
         acc
       ) do
    commands = [
      dynamic_clear_command(previous, background),
      transform_command([build_instance(current)], transparent)
    ]

    build_isolated_commands(rest, background, transparent, [commands | acc])
  end

  defp dynamic_clear_command(object, background) do
    {x0, y0, x1, y1} = object_bounds(object)
    BinaryBatch.fill_rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1, background)
  end

  defp object_bounds({x, y, _dx, _dy, _source, _angle, zoom_x1024, _dangle, _dzoom}) do
    radius =
      div(@source_half_diagonal * zoom_x1024 + 1_023, 1_024) + @erase_padding

    {x - radius, y - radius, x + radius, y + radius}
  end

  defp bounds_intersect?({ax0, ay0, ax1, ay1}, {bx0, by0, bx1, by1}) do
    not (ax1 < bx0 or bx1 < ax0 or ay1 < by0 or by1 < ay0)
  end

  defp build_instances([], acc), do: :lists.reverse(acc)

  defp build_instances([object | rest], acc) do
    build_instances(rest, [build_instance(object) | acc])
  end

  defp build_instance(
         {x, y, _dx, _dy, source, angle_cdeg, zoom_x1024, _dangle_cdeg, _dzoom_x1024}
       ) do
    {source, x, y, angle_cdeg, zoom_x1024, zoom_x1024}
  end
end
