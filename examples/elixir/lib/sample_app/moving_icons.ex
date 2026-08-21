# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.MovingIcons do
  @moduledoc false

  import Bitwise
  import SampleApp.AtomVMCompat, only: [yield: 0]

  alias SampleApp.Assets
  alias SampleApp.MovingIcons.Renderer

  # V3 treats MovingIcons as a low-memory stress demo, not as the main
  # performance target. The flagship target is a Stack-chan-like face that stays
  # smooth while Wi-Fi and the rest of AtomVM are running.
  #
  # Upstream LovyanGFX MovingIcons uses 50 objects. Keep this sample modest so it
  # leaves heap headroom for the VM, sprites, touch, networking, and application
  # code on no-PSRAM boards.
  @obj_count 6

  # Local demo keeps four icons available:
  #
  # - info
  # - alert
  # - close
  # - piyopiyo
  #
  # Set this to 3 when comparing more strictly with upstream LovyanGFX MovingIcons.
  @obj_icon_count 3

  # MovingIcons is intentionally separate from the standard render API. Its
  # renderer submits one compact transformed-sprite list per frame, avoiding a
  # full-frame or strip sprite and keeping memory use realistic on AtomVM.

  # Icon sprites are authored with a solid background color. Use a transparent-color key so
  # that background pixels do not overwrite what is already in the destination.
  @transparent_key_color565 0x0000

  # Background fill color for the playfield and frame buffers (RGB565).
  @bg 0x0000

  # Capability bits.
  @cap_sprite 1 <<< 0

  # Source sprite handles (icons).
  @sprite_info 1
  @sprite_alert 2
  @sprite_close 3
  @sprite_piyopiyo 4

  # Five frames per second is a measured, sustainable target for six transformed
  # icons on the connected 480x320 ESP32-S3 device. Applications can raise this
  # cap after measuring their own display bus and workload.
  @target_fps 5
  @speed_multiplier 2
  @frame_interval_ms div(1000, @target_fps)
  @stats_interval_ms 1_000

  def run(handle, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    icon_w = Assets.icon_w()
    icon_h = Assets.icon_h()

    icons = {
      Assets.icon(:info),
      Assets.icon(:alert),
      Assets.icon(:close),
      Assets.icon(:piyopiyo)
    }

    log_icon_sizes(icons, icon_w, icon_h)
    log_render_config()

    with {:ok, feature_bits} <- AtomLGFX.get_caps(handle),
         :ok <- ensure_required_caps(feature_bits),
         :ok <- AtomLGFX.fill_screen(handle, @bg),
         {:ok, icon_handles} <- prepare_icon_sprites(handle, icons, icon_w, icon_h) do
      try do
        {_seed, objects} = init_objects(1, @obj_count, w, h, icon_handles)

        IO.puts(
          "moving_icons render mode=transformed_sprite_list " <>
            "erase_mode=overlap_aware " <>
            "submit_mode=binary_batch " <>
            "draw_mode=push_rotate_zoom_list " <>
            "target_fps=#{@target_fps} " <>
            "frame=#{w}x#{h}"
        )

        render_loop(handle, w, h, [], objects, 0, monotonic_ms(), false)
      after
        cleanup_icon_sprites(handle)
      end
    else
      {:error, reason} ->
        IO.puts("moving_icons setup failed: #{AtomLGFX.format_error(reason)}")
        {:error, reason}
    end
  end

  defp render_loop(
         handle,
         w,
         h,
         previous_objects,
         objects,
         frame_count,
         last_log_ms,
         target_announced?
       ) do
    frame_started_ms = monotonic_ms()

    with :ok <-
           Renderer.render(
             handle,
             previous_objects,
             objects,
             @bg,
             @transparent_key_color565
           ) do
      next_objects = update_objects(objects, w, h)
      now_ms = monotonic_ms()

      {next_frame_count, next_last_log_ms, next_target_announced?} =
        maybe_log_stats(frame_count + 1, last_log_ms, now_ms, target_announced?)

      sleep_until_next_frame(frame_started_ms)
      yield()

      render_loop(
        handle,
        w,
        h,
        objects,
        next_objects,
        next_frame_count,
        next_last_log_ms,
        next_target_announced?
      )
    else
      {:error, reason} ->
        IO.puts("moving_icons render failed: #{AtomLGFX.format_error(reason)}")
        {:error, reason}
    end
  end

  defp update_objects(objects, w, h), do: update_objects_i(objects, w, h, [])

  defp update_objects_i([], _w, _h, acc), do: :lists.reverse(acc)

  defp update_objects_i(
         [{x, y, dx, dy, src, angle_cdeg, zoom_x1024, dangle_cdeg, dzoom_x1024} | rest],
         w,
         h,
         acc
       ) do
    {next_x, next_dx} = bounce_i16(x + dx, dx, 0, max(w - 1, 0))
    {next_y, next_dy} = bounce_i16(y + dy, dy, 0, max(h - 1, 0))
    next_angle_cdeg = rem(angle_cdeg + dangle_cdeg + 36_000, 36_000)

    {next_zoom_x1024, next_dzoom_x1024} =
      bounce_i16(zoom_x1024 + dzoom_x1024, dzoom_x1024, 512, 2_048)

    updated =
      {
        next_x,
        next_y,
        next_dx,
        next_dy,
        src,
        next_angle_cdeg,
        next_zoom_x1024,
        dangle_cdeg,
        next_dzoom_x1024
      }

    update_objects_i(rest, w, h, [updated | acc])
  end

  defp bounce_i16(value, delta, min_value, _max_value) when value < min_value do
    {min_value, abs(delta)}
  end

  defp bounce_i16(value, delta, _min_value, max_value) when value > max_value do
    {max_value, -abs(delta)}
  end

  defp bounce_i16(value, delta, _min_value, _max_value), do: {value, delta}

  defp maybe_log_stats(frame_count, last_log_ms, now_ms, target_announced?) do
    elapsed_ms = now_ms - last_log_ms

    if elapsed_ms >= @stats_interval_ms do
      fps = div(frame_count * 1000 + div(elapsed_ms, 2), elapsed_ms)
      maybe_log_target_fps(fps, target_announced?)

      IO.puts(
        "moving_icons stats " <>
          "renderer=transformed_sprite_list " <>
          "erase_mode=overlap_aware " <>
          "submit_mode=binary_batch " <>
          "draw_mode=push_rotate_zoom_list " <>
          "obj_count=#{@obj_count} " <>
          "fps=#{fps} " <>
          "target_fps=#{@target_fps}"
      )

      {0, now_ms, target_announced? or fps >= @target_fps}
    else
      {frame_count, last_log_ms, target_announced?}
    end
  end

  defp maybe_log_target_fps(_fps, true), do: :ok

  defp maybe_log_target_fps(fps, false) when fps >= @target_fps do
    IO.puts("moving_icons target_fps=#{@target_fps} reached fps=#{fps}")
    :ok
  end

  defp maybe_log_target_fps(_fps, false), do: :ok

  defp sleep_until_next_frame(frame_started_ms) do
    elapsed_ms = monotonic_ms() - frame_started_ms
    sleep_ms = max(@frame_interval_ms - elapsed_ms, 0)
    Process.sleep(sleep_ms)
  end

  # -----------------------------------------------------------------------------
  # Setup / capabilities
  # -----------------------------------------------------------------------------

  defp ensure_required_caps(feature_bits) when is_integer(feature_bits) do
    if (feature_bits &&& @cap_sprite) == 0 do
      {:error, :cap_sprite_missing}
    else
      :ok
    end
  end

  defp prepare_icon_sprites(handle, icons, icon_w, icon_h) do
    info_bin = elem(icons, 0)
    alert_bin = elem(icons, 1)
    close_bin = elem(icons, 2)
    piyopiyo_bin = elem(icons, 3)

    with :ok <- create_and_load_icon_sprite(handle, @sprite_info, icon_w, icon_h, info_bin),
         :ok <- create_and_load_icon_sprite(handle, @sprite_alert, icon_w, icon_h, alert_bin),
         :ok <- create_and_load_icon_sprite(handle, @sprite_close, icon_w, icon_h, close_bin),
         :ok <-
           create_and_load_icon_sprite(handle, @sprite_piyopiyo, icon_w, icon_h, piyopiyo_bin) do
      {:ok, {@sprite_info, @sprite_alert, @sprite_close, @sprite_piyopiyo}}
    else
      {:error, _} = err ->
        cleanup_icon_sprites(handle)
        err
    end
  end

  defp create_and_load_icon_sprite(handle, sprite_target, icon_w, icon_h, pixels) do
    pivot_x = div(icon_w, 2)
    pivot_y = div(icon_h, 2)

    with :ok <- AtomLGFX.create_sprite(handle, icon_w, icon_h, sprite_target),
         :ok <- AtomLGFX.clear(handle, @transparent_key_color565, sprite_target),
         :ok <- AtomLGFX.set_swap_bytes(handle, true, sprite_target),
         :ok <- AtomLGFX.push_image_rgb565(handle, 0, 0, icon_w, icon_h, pixels, 0, sprite_target),
         :ok <- AtomLGFX.set_swap_bytes(handle, false, sprite_target),
         :ok <- AtomLGFX.set_pivot(handle, sprite_target, pivot_x, pivot_y) do
      :ok
    else
      {:error, reason} ->
        _ = AtomLGFX.set_swap_bytes(handle, false, sprite_target)
        _ = AtomLGFX.delete_sprite(handle, sprite_target)
        {:error, {:sprite_setup_failed, sprite_target, reason}}
    end
  end

  defp cleanup_icon_sprites(handle) do
    _ = safe_delete_sprite(handle, @sprite_info)
    _ = safe_delete_sprite(handle, @sprite_alert)
    _ = safe_delete_sprite(handle, @sprite_close)
    _ = safe_delete_sprite(handle, @sprite_piyopiyo)
    :ok
  end

  defp safe_delete_sprite(handle, sprite_target) do
    case AtomLGFX.delete_sprite(handle, sprite_target) do
      :ok -> :ok
      {:error, _} -> :ok
    end
  end

  # -----------------------------------------------------------------------------
  # RNG (AtomVM-friendly; no :rand)
  # -----------------------------------------------------------------------------

  defp rand_u32(seed) when is_integer(seed) do
    seed2 = rem(seed * 1_664_525 + 1_013_904_223, 4_294_967_296)
    {seed2, seed2}
  end

  # -----------------------------------------------------------------------------
  # Object init
  # -----------------------------------------------------------------------------

  # Internal object tuple:
  # {x, y, dx, dy, src_handle, angle_cdeg, zoom_x1024, dangle_cdeg, dzoom_x1024}
  defp init_objects(seed, count, w, h, icon_handles) do
    init_objects_i(seed, 0, count, w, h, icon_handles, [])
  end

  defp init_objects_i(seed, _i, count, _w, _h, _icon_handles, acc) when count <= 0 do
    {seed, :lists.reverse(acc)}
  end

  defp init_objects_i(seed, i, count, w, h, icon_handles, acc) do
    {seed, r1} = rand_u32(seed)
    {seed, r2} = rand_u32(seed)
    {seed, r3} = rand_u32(seed)
    {seed, r4} = rand_u32(seed)
    {seed, r5} = rand_u32(seed)

    src = src_handle_for_index(rem(i, @obj_icon_count), icon_handles)

    x = rem(r1, w)
    y = rem(r2, h)

    dx0 = (band3(r3) + 1) * @speed_multiplier * sign(i &&& 1)
    dy0 = (band3(r4) + 1) * @speed_multiplier * sign(i &&& 2)

    dr_deg = (band3(r5) + 1) * sign(i &&& 2)
    dr_cdeg = dr_deg * 100

    z10 = rem(r3 >>> 8, 10) + 10
    z_x1024 = div(z10 * 1024, 10)

    dz100 = rem(r4 >>> 8, 10) + 1
    dz_x1024 = div(dz100 * 1024, 100)

    obj = {x, y, dx0, dy0, src, 0, z_x1024, dr_cdeg, dz_x1024}
    init_objects_i(seed, i + 1, count - 1, w, h, icon_handles, [obj | acc])
  end

  defp band3(u32), do: u32 >>> 16 &&& 3

  defp sign(0), do: -1
  defp sign(_), do: 1

  defp src_handle_for_index(0, icon_handles), do: elem(icon_handles, 0)
  defp src_handle_for_index(1, icon_handles), do: elem(icon_handles, 1)
  defp src_handle_for_index(2, icon_handles), do: elem(icon_handles, 2)
  defp src_handle_for_index(3, icon_handles), do: elem(icon_handles, 3)

  defp monotonic_ms do
    :erlang.monotonic_time(:millisecond)
  end

  defp log_icon_sizes(icons, icon_w, icon_h) do
    expected = icon_w * icon_h * 2
    i0 = byte_size(elem(icons, 0))
    i1 = byte_size(elem(icons, 1))
    i2 = byte_size(elem(icons, 2))
    i3 = byte_size(elem(icons, 3))

    IO.puts("icon bytes info=#{i0} alert=#{i1} close=#{i2} piyopiyo=#{i3} expected=#{expected}")
  end

  defp log_render_config do
    IO.puts(
      "moving_icons config " <>
        "obj_count=#{@obj_count} " <>
        "icon_count=#{@obj_icon_count} " <>
        "renderer=transformed_sprite_list " <>
        "erase_mode=overlap_aware " <>
        "submit_mode=binary_batch " <>
        "draw_mode=push_rotate_zoom_list " <>
        "target_fps=#{@target_fps} " <>
        "speed_multiplier=#{@speed_multiplier}"
    )
  end
end
