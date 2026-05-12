# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.MovingIcons do
  @moduledoc false

  import Bitwise
  import SampleApp.AtomVMCompat, only: [yield: 0]

  alias SampleApp.Assets

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

  # Retained/native presentation strips were removed from the normal example path
  # because they make memory ownership harder to reason about. Keep animation
  # non-flickering by drawing each vertical strip into an ordinary offscreen
  # sprite and then pushing the completed strip to the LCD.
  #
  # Strip height is the main speed/memory tradeoff. Taller strips mean fewer
  # clear/push cycles and smoother motion, but require a larger contiguous sprite
  # allocation. For a 480x320 landscape viewport, split_factor=8 gives a
  # 480x40x16bpp sprite, about 38 KiB. Setup still falls back to smaller strips
  # if needed.
  @initial_split_factor 8

  # Icon sprites are authored with a solid background color. Use a transparent-color key so
  # that background pixels do not overwrite what is already in the destination.
  @use_transparent_key true
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

  # Destination sprite handle for the strip renderer.
  #
  # Keep one strip sprite in the low-memory sample. The completed strip is pushed
  # to the LCD before the buffer is reused for the next strip.
  @sprite_buf0 10

  # MovingIcons intentionally uses non-rotated sprite pushes in the low-memory
  # sample path. `pushRotateZoom` is attractive for visual parity with upstream
  # LovyanGFX, but it can allocate too much on no-PSRAM AtomVM targets.
  #
  # Make the visual motion faster without increasing persistent memory. A higher
  # FPS target helps when the board has headroom; if rendering is slower than the
  # target, the loop simply skips the extra sleep. A higher speed multiplier makes
  # motion look faster without adding buffers, but it does not reduce render cost.
  @target_fps 20
  @speed_multiplier 2
  @frame_interval_ms div(1000, @target_fps)
  @stats_interval_ms 1_000

  def run(port, w, h) when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
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

    with {:ok, feature_bits} <- AtomLGFX.get_caps(port),
         :ok <- ensure_required_caps(feature_bits),
         :ok <- AtomLGFX.fill_screen(port, @bg),
         {:ok, icon_handles} <- prepare_icon_sprites(port, icons, icon_w, icon_h),
         {:ok, strip_h} <- prepare_frame_sprites(port, w, h) do
      try do
        {_seed, objects} = init_objects(1, @obj_count, w, h, icon_handles)

        IO.puts(
          "moving_icons render mode=strip_buffers " <>
            "submit_mode=sync " <>
            "draw_mode=push_sprite " <>
            "target_fps=#{@target_fps} " <>
            "strip_h=#{strip_h} " <>
            "frame=#{w}x#{h}"
        )

        render_loop(port, w, h, strip_h, icon_h, 0, objects, 0, monotonic_ms(), false)
      after
        cleanup_frame_sprites(port)
        cleanup_icon_sprites(port)
      end
    else
      {:error, reason} ->
        IO.puts("moving_icons setup failed: #{AtomLGFX.format_error(reason)}")
        {:error, reason}
    end
  end

  defp render_loop(
         port,
         w,
         h,
         strip_h,
         icon_h,
         flip,
         objects,
         frame_count,
         last_log_ms,
         target_announced?
       ) do
    frame_started_ms = monotonic_ms()

    with {:ok, next_flip} <- render_frame(port, h, strip_h, icon_h, flip, objects),
         :ok <- AtomLGFX.display(port) do
      next_objects = update_objects(objects, w, h)
      now_ms = monotonic_ms()

      {next_frame_count, next_last_log_ms, next_target_announced?} =
        maybe_log_stats(frame_count + 1, last_log_ms, now_ms, target_announced?, strip_h)

      sleep_until_next_frame(frame_started_ms)
      yield()

      render_loop(
        port,
        w,
        h,
        strip_h,
        icon_h,
        next_flip,
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

  defp render_frame(port, h, strip_h, icon_h, flip, objects) do
    render_strips(port, h, strip_h, icon_h, 0, flip, objects)
  end

  defp render_strips(_port, h, _strip_h, _icon_h, y0, flip, _objects) when y0 >= h do
    {:ok, flip}
  end

  defp render_strips(port, h, strip_h, icon_h, y0, flip, objects) do
    with :ok <- AtomLGFX.clear(port, @bg, @sprite_buf0),
         :ok <- draw_visible_objects_to_strip(port, @sprite_buf0, y0, strip_h, icon_h, objects),
         :ok <- AtomLGFX.push_sprite(port, @sprite_buf0, 0, y0) do
      render_strips(port, h, strip_h, icon_h, y0 + strip_h, flip, objects)
    else
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp draw_visible_objects_to_strip(port, buf, y0, strip_h, icon_h, objects) do
    draw_visible_objects_to_strip_i(port, buf, y0, strip_h, icon_h, objects)
  end

  defp draw_visible_objects_to_strip_i(_port, _buf, _y0, _strip_h, _icon_h, []), do: :ok

  defp draw_visible_objects_to_strip_i(
         port,
         buf,
         y0,
         strip_h,
         icon_h,
         [
           {x, y, _dx, _dy, src, _angle_cdeg, _zoom_x1024, _dangle_cdeg, _dzoom_x1024} = object
           | rest
         ]
       ) do
    if object_touches_strip?(object, y0, strip_h, icon_h) do
      draw_y = y - y0

      with :ok <- push_sprite_to_strip(port, src, buf, x, draw_y) do
        draw_visible_objects_to_strip_i(port, buf, y0, strip_h, icon_h, rest)
      end
    else
      draw_visible_objects_to_strip_i(port, buf, y0, strip_h, icon_h, rest)
    end
  end

  defp push_sprite_to_strip(port, src, buf, x, y) do
    case transparent_key() do
      nil -> AtomLGFX.push_sprite_to(port, src, buf, x, y)
      transparent -> AtomLGFX.push_sprite_to(port, src, buf, x, y, transparent)
    end
  end

  defp object_touches_strip?(
         {_x, y, _dx, _dy, _src, _angle_cdeg, _zoom_x1024, _dangle_cdeg, _dzoom_x1024},
         y0,
         strip_h,
         icon_h
       ) do
    strip_y1 = y0 + strip_h - 1
    icon_y1 = y + icon_h - 1
    not (icon_y1 < y0 or y > strip_y1)
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

    updated =
      {
        next_x,
        next_y,
        next_dx,
        next_dy,
        src,
        angle_cdeg,
        zoom_x1024,
        dangle_cdeg,
        dzoom_x1024
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

  defp maybe_log_stats(frame_count, last_log_ms, now_ms, target_announced?, strip_h) do
    elapsed_ms = now_ms - last_log_ms

    if elapsed_ms >= @stats_interval_ms do
      fps = div(frame_count * 1000 + div(elapsed_ms, 2), elapsed_ms)
      maybe_log_target_fps(fps, target_announced?)

      IO.puts(
        "moving_icons stats " <>
          "renderer=strip_buffers " <>
          "submit_mode=sync " <>
          "draw_mode=push_sprite " <>
          "obj_count=#{@obj_count} " <>
          "strip_h=#{strip_h} " <>
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

  defp transparent_key do
    if @use_transparent_key do
      @transparent_key_color565
    else
      nil
    end
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

  defp prepare_icon_sprites(port, icons, icon_w, icon_h) do
    info_bin = elem(icons, 0)
    alert_bin = elem(icons, 1)
    close_bin = elem(icons, 2)
    piyopiyo_bin = elem(icons, 3)

    with :ok <- create_and_load_icon_sprite(port, @sprite_info, icon_w, icon_h, info_bin),
         :ok <- create_and_load_icon_sprite(port, @sprite_alert, icon_w, icon_h, alert_bin),
         :ok <- create_and_load_icon_sprite(port, @sprite_close, icon_w, icon_h, close_bin),
         :ok <- create_and_load_icon_sprite(port, @sprite_piyopiyo, icon_w, icon_h, piyopiyo_bin) do
      {:ok, {@sprite_info, @sprite_alert, @sprite_close, @sprite_piyopiyo}}
    else
      {:error, _} = err ->
        cleanup_icon_sprites(port)
        err
    end
  end

  defp create_and_load_icon_sprite(port, sprite_target, icon_w, icon_h, pixels) do
    pivot_x = div(icon_w, 2)
    pivot_y = div(icon_h, 2)

    with :ok <- AtomLGFX.create_sprite(port, icon_w, icon_h, sprite_target),
         :ok <- AtomLGFX.clear(port, @transparent_key_color565, sprite_target),
         :ok <- AtomLGFX.set_swap_bytes(port, true, sprite_target),
         :ok <- AtomLGFX.push_image_rgb565(port, 0, 0, icon_w, icon_h, pixels, 0, sprite_target),
         :ok <- AtomLGFX.set_swap_bytes(port, false, sprite_target),
         :ok <- AtomLGFX.set_pivot(port, sprite_target, pivot_x, pivot_y) do
      :ok
    else
      {:error, reason} ->
        _ = AtomLGFX.set_swap_bytes(port, false, sprite_target)
        _ = AtomLGFX.delete_sprite(port, sprite_target)
        {:error, {:sprite_setup_failed, sprite_target, reason}}
    end
  end

  defp prepare_frame_sprites(port, w, h) do
    prepare_frame_sprites_i(port, w, h, @initial_split_factor)
  end

  defp prepare_frame_sprites_i(port, w, h, split_factor) do
    strip_h = max(1, div_ceil(h, split_factor))

    with :ok <- create_frame_sprite(port, @sprite_buf0, w, strip_h) do
      {:ok, strip_h}
    else
      {:error, reason} ->
        cleanup_frame_sprites(port)

        if strip_h == 1 do
          {:error, {:frame_sprite_alloc_failed, w, h, split_factor, reason}}
        else
          prepare_frame_sprites_i(port, w, h, split_factor + 1)
        end
    end
  end

  defp create_frame_sprite(port, target, w, h) do
    color_depth = 16
    AtomLGFX.create_sprite(port, w, h, color_depth, target)
  end

  defp cleanup_icon_sprites(port) do
    _ = safe_delete_sprite(port, @sprite_info)
    _ = safe_delete_sprite(port, @sprite_alert)
    _ = safe_delete_sprite(port, @sprite_close)
    _ = safe_delete_sprite(port, @sprite_piyopiyo)
    :ok
  end

  defp cleanup_frame_sprites(port) do
    _ = safe_delete_sprite(port, @sprite_buf0)
    :ok
  end

  defp safe_delete_sprite(port, sprite_target) do
    case AtomLGFX.delete_sprite(port, sprite_target) do
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

  defp div_ceil(a, b) when is_integer(a) and is_integer(b) and b > 0 do
    div(a + b - 1, b)
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
        "renderer=strip_buffers " <>
        "submit_mode=sync " <>
        "draw_mode=push_sprite " <>
        "target_fps=#{@target_fps} " <>
        "speed_multiplier=#{@speed_multiplier} " <>
        "initial_split_factor=#{@initial_split_factor}"
    )
  end
end
