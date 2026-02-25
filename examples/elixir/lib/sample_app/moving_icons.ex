defmodule SampleApp.MovingIcons do
  @moduledoc false

  import Bitwise

  alias SampleApp.Assets
  alias SampleApp.Port
  import SampleApp.AtomVMCompat, only: [yield: 0]

  # -----------------------------------------------------------------------------
  # Tuning knobs
  # -----------------------------------------------------------------------------
  # Toggle this for quick fps testing.
  # - true  => fewer objects, slower HUD refresh, transform throttling
  # - false => normal demo feel
  @benchmark_mode false

  # Fine-grained object timing is helpful for profiling, but it costs fps.
  # Keep false for normal runs.
  @measure_object_timings false

  # Faster default: avoid transparent-key blending on a black background.
  # (Sprite background and playfield are both black.)
  @use_transparent_push_sprite false

  # Update angle/zoom every N frames (position still updates every frame).
  # This can improve fps while keeping motion smooth enough.
  @rotate_zoom_every_n_frames if @benchmark_mode, do: 2, else: 1

  # HUD refresh interval (seconds)
  @hud_refresh_s if @benchmark_mode, do: 2, else: 1

  # Bench knob
  @obj_count if @benchmark_mode, do: 10, else: 5

  # Keep frame_index bounded over very long runs.
  @frame_index_wrap 1_000_000

  # HUD
  @hud_h 32
  @hud_bg 0x101010
  @hud_fg 0xFFFFFF
  @hud_dim 0xA0A0A0

  # Playfield background
  @bg 0x000000

  # Transparent key used for sprite atlas background (matches playfield)
  @transparent_key 0x000000

  # Capability bits (LGFX_PORT_PROTOCOL.md)
  @cap_sprite 1 <<< 0

  # Sprite handles reserved for icon atlases
  @sprite_info 1
  @sprite_alert 2
  @sprite_close 3

  # Zoom animation (Q8)
  @zoom_min_q8 256
  @zoom_max_q8 512

  # Dirty-rect padding for rotated corners
  @clear_pad_px 8

  # Expects display already initialized and rotated, with normalized logical dims.
  def run(port, w, h)
      when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    icon_w = Assets.icon_w()
    icon_h = Assets.icon_h()

    icons = {Assets.icon(:info), Assets.icon(:alert), Assets.icon(:close)}
    log_icon_sizes(icons, icon_w, icon_h)
    log_runtime_tuning()

    max_x = max_i(0, w - icon_w)
    min_y = @hud_h
    max_y = max_i(min_y, h - icon_h)

    with {:ok, caps} <- Port.get_caps(port),
         :ok <- ensure_sprite_support(caps),
         :ok <- Port.fill_screen(port, @bg) do
      case prepare_icon_sprites(port, icons, icon_w, icon_h) do
        {:ok, sprite_handles} ->
          try do
            # HUD failure should not crash the demo loop on constrained devices.
            case draw_hud(port, w, @obj_count, 0, 0, 0, 0, true) do
              :ok ->
                :ok

              {:error, reason} ->
                _ = reason
                IO.puts("moving_icons hud init failed; continuing")
                :ok
            end

            {_seed, objects} =
              init_objects(1, @obj_count, max_x, min_y, max_y, sprite_handles)

            # State tuple:
            # {w, h, max_x, min_y, max_y, icon_w, icon_h, objects,
            #  fps, frame_count, prev_sec, hud_sec, rotate_enabled, frame_index}
            state =
              {w, h, max_x, min_y, max_y, icon_w, icon_h, objects, 0, 0, nil, nil, true, 0}

            loop(port, state)
          after
            cleanup_icon_sprites(port)
          end

        {:error, reason} ->
          IO.puts("prepare_icon_sprites failed: #{Port.format_error(reason)}")
          {:error, reason}
      end
    else
      {:error, reason} ->
        IO.puts("moving_icons setup failed: #{Port.format_error(reason)}")
        {:error, reason}
    end
  end

  # -----------------------------------------------------------------------------
  # Setup / capabilities
  # -----------------------------------------------------------------------------

  defp ensure_sprite_support(%{feature_bits: feature_bits, max_sprites: max_sprites}) do
    cond do
      (feature_bits &&& @cap_sprite) == 0 ->
        {:error, :cap_sprite_missing}

      max_sprites < 3 ->
        {:error, {:insufficient_sprite_capacity, max_sprites, 3}}

      true ->
        :ok
    end
  end

  defp prepare_icon_sprites(port, icons, icon_w, icon_h) do
    info_bin = elem(icons, 0)
    alert_bin = elem(icons, 1)
    close_bin = elem(icons, 2)

    with :ok <- create_and_load_sprite(port, @sprite_info, icon_w, icon_h, info_bin),
         :ok <- create_and_load_sprite(port, @sprite_alert, icon_w, icon_h, alert_bin),
         :ok <- create_and_load_sprite(port, @sprite_close, icon_w, icon_h, close_bin) do
      {:ok, {@sprite_info, @sprite_alert, @sprite_close}}
    else
      {:error, _reason} = err ->
        cleanup_icon_sprites(port)
        err
    end
  end

  defp create_and_load_sprite(port, sprite_target, icon_w, icon_h, pixels) do
    pivot_x = div(icon_w, 2)
    pivot_y = div(icon_h, 2)

    with :ok <- Port.create_sprite(port, icon_w, icon_h, sprite_target),
         :ok <- Port.clear(port, @transparent_key, sprite_target),
         :ok <- Port.push_image_rgb565(port, 0, 0, icon_w, icon_h, pixels, 0, sprite_target),
         :ok <- Port.set_pivot(port, sprite_target, pivot_x, pivot_y) do
      :ok
    else
      {:error, reason} ->
        _ = Port.delete_sprite(port, sprite_target)
        {:error, {:sprite_setup_failed, sprite_target, reason}}
    end
  end

  defp cleanup_icon_sprites(port) do
    _ = safe_delete_sprite(port, @sprite_info)
    _ = safe_delete_sprite(port, @sprite_alert)
    _ = safe_delete_sprite(port, @sprite_close)
    :ok
  end

  defp safe_delete_sprite(port, sprite_target) do
    case Port.delete_sprite(port, sprite_target) do
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
  # Setup helpers
  # -----------------------------------------------------------------------------

  defp log_icon_sizes(icons, icon_w, icon_h) do
    expected = icon_w * icon_h * 2

    i0 = byte_size(elem(icons, 0))
    i1 = byte_size(elem(icons, 1))
    i2 = byte_size(elem(icons, 2))

    IO.puts("icon bytes info=#{i0} alert=#{i1} close=#{i2} expected=#{expected}")
  end

  defp log_runtime_tuning do
    IO.puts(
      "moving_icons cfg obj=#{@obj_count} bench=#{bool_i(@benchmark_mode)} " <>
        "tm=#{bool_i(@measure_object_timings)} rz_step=#{@rotate_zoom_every_n_frames} " <>
        "hud=#{@hud_refresh_s}s tr_fallback=#{bool_i(@use_transparent_push_sprite)}"
    )
  end

  # objects are tuples:
  # {x, y, dx, dy, sprite_target, angle_tenths, d_angle_tenths, zoom_q8, d_zoom_q8}
  defp init_objects(seed, count, max_x, min_y, max_y, sprite_handles) do
    init_objects_i(seed, count, max_x, min_y, max_y, sprite_handles, [])
  end

  defp init_objects_i(seed, 0, _max_x, _min_y, _max_y, _sprite_handles, acc),
    do: {seed, :lists.reverse(acc)}

  defp init_objects_i(seed, n, max_x, min_y, max_y, sprite_handles, acc) do
    {seed, r1} = rand_u32(seed)
    {seed, r2} = rand_u32(seed)
    {seed, r3} = rand_u32(seed)
    {seed, r4} = rand_u32(seed)
    {seed, r5} = rand_u32(seed)
    {seed, r6} = rand_u32(seed)

    x = rem(r1, max_x + 1)
    y = min_y + rem(r2, max_i(1, max_y - min_y + 1))

    base_dx = rem(r3, 4) + 1
    base_dy = rem(r4, 4) + 1

    dx = if rem(r1, 2) == 0, do: base_dx, else: -base_dx
    dy = if rem(r2, 2) == 0, do: base_dy, else: -base_dy

    sprite_index = rem(n, 3)

    sprite_target =
      case sprite_index do
        0 -> elem(sprite_handles, 0)
        1 -> elem(sprite_handles, 1)
        _ -> elem(sprite_handles, 2)
      end

    angle_tenths = rem(r5, 3600)
    d_angle_tenths0 = (rem(r6, 8) + 2) * 10
    d_angle_tenths = if rem(r5, 2) == 0, do: d_angle_tenths0, else: -d_angle_tenths0

    zoom_q8 = @zoom_min_q8 + rem(r3, @zoom_max_q8 - @zoom_min_q8 + 1)
    d_zoom_q80 = rem(r4, 5) + 2
    d_zoom_q8 = if rem(r6, 2) == 0, do: d_zoom_q80, else: -d_zoom_q80

    object =
      {x, y, dx, dy, sprite_target, angle_tenths, d_angle_tenths, zoom_q8, d_zoom_q8}

    init_objects_i(seed, n - 1, max_x, min_y, max_y, sprite_handles, [object | acc])
  end

  # -----------------------------------------------------------------------------
  # Main loop
  # -----------------------------------------------------------------------------

  defp loop(port, state) do
    now_ms = :erlang.monotonic_time(:millisecond)
    sec = div(now_ms, 1000)

    {w, _h, _max_x, _min_y, _max_y, _iw, _ih, _objects, fps0, frame_count0, prev_sec0, hud_sec0,
     _rotate_enabled0, frame_index0} = state

    {fps, frame_count, prev_sec} = tick_fps(sec, fps0, frame_count0, prev_sec0)

    advance_transforms = should_advance_transforms?(frame_index0)

    case render_frame(port, state, advance_transforms) do
      {:ok, objects_next, clr_ms, blit_ms, draw_ms, rotate_enabled} ->
        hud_sec =
          if should_refresh_hud?(hud_sec0, sec) do
            case draw_hud(port, w, @obj_count, fps, draw_ms, clr_ms, blit_ms, rotate_enabled) do
              :ok ->
                sec

              {:error, reason} ->
                # Avoid throw/catch in the hot loop on AtomVM to reduce pressure from exception stacktraces.
                _ = reason
                IO.puts("moving_icons hud update failed; continuing")
                sec
            end
          else
            hud_sec0
          end

        yield()

        {w1, h1, max_x1, min_y1, max_y1, iw1, ih1, _objects1, _fps1, _fc1, _ps1, _hs1, _rz1, _fi1} =
          state

        loop(
          port,
          {w1, h1, max_x1, min_y1, max_y1, iw1, ih1, objects_next, fps, frame_count, prev_sec,
           hud_sec, rotate_enabled, bump_frame_index(frame_index0)}
        )

      {:error, reason} ->
        IO.puts("moving_icons render failed: #{Port.format_error(reason)}")
        {:error, reason}
    end
  end

  defp tick_fps(sec, fps, frame_count, prev_sec) do
    case prev_sec do
      nil ->
        {fps, 1, sec}

      ^sec ->
        {fps, frame_count + 1, sec}

      _ ->
        {frame_count, 1, sec}
    end
  end

  defp should_refresh_hud?(nil, _sec), do: true

  defp should_refresh_hud?(hud_sec, sec) do
    sec - hud_sec >= @hud_refresh_s
  end

  defp should_advance_transforms?(frame_index) do
    if @rotate_zoom_every_n_frames <= 1 do
      true
    else
      rem(frame_index, @rotate_zoom_every_n_frames) == 0
    end
  end

  defp bump_frame_index(frame_index) do
    if frame_index >= @frame_index_wrap do
      0
    else
      frame_index + 1
    end
  end

  # -----------------------------------------------------------------------------
  # Render
  # -----------------------------------------------------------------------------

  defp render_frame(port, state, advance_transforms) do
    {w, h, max_x, min_y, max_y, icon_w, icon_h, objects, _fps, _fc, _ps, _hs, rotate_enabled0,
     _fi} =
      state

    t0 = :erlang.monotonic_time(:millisecond)

    case render_objects_i(
           port,
           objects,
           w,
           h,
           max_x,
           min_y,
           max_y,
           icon_w,
           icon_h,
           0,
           0,
           rotate_enabled0,
           advance_transforms,
           []
         ) do
      {:ok, objects_rev, clr_ms, blit_ms, rotate_enabled} ->
        t1 = :erlang.monotonic_time(:millisecond)
        objects_next = :lists.reverse(objects_rev)
        draw_ms = t1 - t0
        {:ok, objects_next, clr_ms, blit_ms, draw_ms, rotate_enabled}

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp render_objects_i(
         _port,
         [],
         _screen_w,
         _screen_h,
         _max_x,
         _min_y,
         _max_y,
         _icon_w,
         _icon_h,
         clr_ms,
         blit_ms,
         rotate_enabled,
         _advance_transforms,
         acc
       ) do
    {:ok, acc, clr_ms, blit_ms, rotate_enabled}
  end

  defp render_objects_i(
         port,
         [{x, y, dx, dy, sprite_target, angle_tenths, d_angle_tenths, zoom_q8, d_zoom_q8} | rest],
         screen_w,
         screen_h,
         max_x,
         min_y,
         max_y,
         icon_w,
         icon_h,
         clr_ms,
         blit_ms,
         rotate_enabled,
         advance_transforms,
         acc
       ) do
    if @measure_object_timings do
      render_objects_i_measured(
        port,
        {x, y, dx, dy, sprite_target, angle_tenths, d_angle_tenths, zoom_q8, d_zoom_q8},
        rest,
        screen_w,
        screen_h,
        max_x,
        min_y,
        max_y,
        icon_w,
        icon_h,
        clr_ms,
        blit_ms,
        rotate_enabled,
        advance_transforms,
        acc
      )
    else
      render_objects_i_fast(
        port,
        {x, y, dx, dy, sprite_target, angle_tenths, d_angle_tenths, zoom_q8, d_zoom_q8},
        rest,
        screen_w,
        screen_h,
        max_x,
        min_y,
        max_y,
        icon_w,
        icon_h,
        clr_ms,
        blit_ms,
        rotate_enabled,
        advance_transforms,
        acc
      )
    end
  end

  defp render_objects_i_fast(
         port,
         {x, y, dx, dy, sprite_target, angle_tenths, d_angle_tenths, zoom_q8, d_zoom_q8},
         rest,
         screen_w,
         screen_h,
         max_x,
         min_y,
         max_y,
         icon_w,
         icon_h,
         clr_ms,
         blit_ms,
         rotate_enabled,
         advance_transforms,
         acc
       ) do
    with :ok <-
           clear_object_box(
             port,
             x,
             y,
             zoom_q8,
             icon_w,
             icon_h,
             screen_w,
             screen_h,
             min_y,
             rotate_enabled
           ),
         {:ok, x2, dx2} <- bounce_axis(x, dx, 0, max_x),
         {:ok, y2, dy2} <- bounce_axis(y, dy, min_y, max_y),
         {:ok, angle2, d_angle2, zoom2, d_zoom2} <-
           advance_object_transform(
             angle_tenths,
             d_angle_tenths,
             zoom_q8,
             d_zoom_q8,
             advance_transforms
           ),
         {:ok, rotate_enabled2} <-
           draw_object(
             port,
             sprite_target,
             x2,
             y2,
             angle2,
             zoom2,
             icon_w,
             icon_h,
             rotate_enabled
           ) do
      render_objects_i(
        port,
        rest,
        screen_w,
        screen_h,
        max_x,
        min_y,
        max_y,
        icon_w,
        icon_h,
        clr_ms,
        blit_ms,
        rotate_enabled2,
        advance_transforms,
        [{x2, y2, dx2, dy2, sprite_target, angle2, d_angle2, zoom2, d_zoom2} | acc]
      )
    else
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp render_objects_i_measured(
         port,
         {x, y, dx, dy, sprite_target, angle_tenths, d_angle_tenths, zoom_q8, d_zoom_q8},
         rest,
         screen_w,
         screen_h,
         max_x,
         min_y,
         max_y,
         icon_w,
         icon_h,
         clr_ms,
         blit_ms,
         rotate_enabled,
         advance_transforms,
         acc
       ) do
    t0 = :erlang.monotonic_time(:millisecond)

    with :ok <-
           clear_object_box(
             port,
             x,
             y,
             zoom_q8,
             icon_w,
             icon_h,
             screen_w,
             screen_h,
             min_y,
             rotate_enabled
           ),
         {:ok, x2, dx2} <- bounce_axis(x, dx, 0, max_x),
         {:ok, y2, dy2} <- bounce_axis(y, dy, min_y, max_y),
         {:ok, angle2, d_angle2, zoom2, d_zoom2} <-
           advance_object_transform(
             angle_tenths,
             d_angle_tenths,
             zoom_q8,
             d_zoom_q8,
             advance_transforms
           ) do
      t1 = :erlang.monotonic_time(:millisecond)

      case draw_object(
             port,
             sprite_target,
             x2,
             y2,
             angle2,
             zoom2,
             icon_w,
             icon_h,
             rotate_enabled
           ) do
        {:ok, rotate_enabled2} ->
          t2 = :erlang.monotonic_time(:millisecond)

          render_objects_i(
            port,
            rest,
            screen_w,
            screen_h,
            max_x,
            min_y,
            max_y,
            icon_w,
            icon_h,
            clr_ms + (t1 - t0),
            blit_ms + (t2 - t1),
            rotate_enabled2,
            advance_transforms,
            [{x2, y2, dx2, dy2, sprite_target, angle2, d_angle2, zoom2, d_zoom2} | acc]
          )

        {:error, reason} ->
          {:error, reason}
      end
    else
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp advance_object_transform(
         angle_tenths,
         d_angle_tenths,
         zoom_q8,
         d_zoom_q8,
         advance_transforms
       ) do
    if advance_transforms do
      with {:ok, angle2, d_angle2} <- advance_angle(angle_tenths, d_angle_tenths),
           {:ok, zoom2, d_zoom2} <- advance_zoom(zoom_q8, d_zoom_q8) do
        {:ok, angle2, d_angle2, zoom2, d_zoom2}
      end
    else
      {:ok, angle_tenths, d_angle_tenths, zoom_q8, d_zoom_q8}
    end
  end

  defp clear_object_box(
         port,
         x,
         y,
         zoom_q8,
         icon_w,
         icon_h,
         screen_w,
         screen_h,
         min_y,
         rotate_enabled
       ) do
    if rotate_enabled do
      center_x = x + div(icon_w, 2)
      center_y = y + div(icon_h, 2)

      scaled_w = max_i(icon_w, div(icon_w * zoom_q8, 256))
      scaled_h = max_i(icon_h, div(icon_h * zoom_q8, 256))

      # Generous padding so rotated corners are erased cleanly.
      half = div(max_i(scaled_w, scaled_h), 2) + @clear_pad_px

      rect_x0 = center_x - half
      rect_y0 = center_y - half
      rect_x1 = center_x + half
      rect_y1 = center_y + half

      fill_clipped_rect(port, rect_x0, rect_y0, rect_x1, rect_y1, screen_w, screen_h, min_y)
    else
      # Tight box when rotate path is unavailable and we are falling back to pushSprite.
      rect_x0 = x
      rect_y0 = y
      rect_x1 = x + icon_w
      rect_y1 = y + icon_h

      fill_clipped_rect(port, rect_x0, rect_y0, rect_x1, rect_y1, screen_w, screen_h, min_y)
    end
  end

  defp fill_clipped_rect(port, rect_x0, rect_y0, rect_x1, rect_y1, screen_w, screen_h, min_y) do
    clip_x0 = max_i(0, rect_x0)
    clip_y0 = max_i(min_y, rect_y0)
    clip_x1 = min_i(screen_w, rect_x1)
    clip_y1 = min_i(screen_h, rect_y1)

    rect_w = max_i(0, clip_x1 - clip_x0)
    rect_h = max_i(0, clip_y1 - clip_y0)

    if rect_w > 0 and rect_h > 0 do
      Port.fill_rect(port, clip_x0, clip_y0, rect_w, rect_h, @bg)
    else
      :ok
    end
  end

  defp bounce_axis(pos, delta, min_value, max_value) do
    if pos + delta < min_value do
      {:ok, min_value, abs(delta)}
    else
      if pos + delta > max_value do
        {:ok, max_value, -abs(delta)}
      else
        {:ok, pos + delta, delta}
      end
    end
  end

  defp advance_angle(angle_tenths, d_angle_tenths) do
    angle2 = angle_tenths + d_angle_tenths

    angle_wrapped =
      cond do
        angle2 < 0 -> angle2 + 3600
        angle2 >= 3600 -> angle2 - 3600
        true -> angle2
      end

    {:ok, angle_wrapped, d_angle_tenths}
  end

  defp advance_zoom(zoom_q8, d_zoom_q8) do
    zoom2 = zoom_q8 + d_zoom_q8

    cond do
      zoom2 < @zoom_min_q8 ->
        {:ok, @zoom_min_q8, abs(d_zoom_q8)}

      zoom2 > @zoom_max_q8 ->
        {:ok, @zoom_max_q8, -abs(d_zoom_q8)}

      true ->
        {:ok, zoom2, d_zoom_q8}
    end
  end

  defp draw_object(
         port,
         sprite_target,
         x,
         y,
         angle_tenths,
         zoom_q8,
         icon_w,
         icon_h,
         rotate_enabled
       ) do
    if rotate_enabled do
      center_x = x + div(icon_w, 2)
      center_y = y + div(icon_h, 2)

      # Fast path: non-transparent rotate/zoom.
      # Background is already black, so transparent-key blending is usually unnecessary.
      case Port.push_rotate_zoom(
             port,
             sprite_target,
             center_x,
             center_y,
             angle_tenths,
             zoom_q8,
             zoom_q8
           ) do
        :ok ->
          {:ok, true}

        # If the driver build does not support rotate/zoom yet, gracefully fall back.
        {:error, :unsupported} ->
          draw_object_fallback_push_sprite(port, sprite_target, x, y)

        {:error, :bad_args} ->
          draw_object_fallback_push_sprite(port, sprite_target, x, y)

        {:error, reason} ->
          {:error, reason}
      end
    else
      draw_object_fallback_push_sprite(port, sprite_target, x, y)
    end
  end

  defp draw_object_fallback_push_sprite(port, sprite_target, x, y) do
    if @use_transparent_push_sprite do
      case Port.push_sprite(port, sprite_target, x, y, @transparent_key) do
        :ok -> {:ok, false}
        {:error, reason} -> {:error, reason}
      end
    else
      case Port.push_sprite(port, sprite_target, x, y) do
        :ok -> {:ok, false}
        {:error, reason} -> {:error, reason}
      end
    end
  end

  # -----------------------------------------------------------------------------
  # HUD
  # -----------------------------------------------------------------------------

  defp draw_hud(port, screen_w, object_count, fps, draw_ms, clr_ms, blit_ms, rotate_enabled) do
    rotate_label =
      if rotate_enabled do
        "rz:on"
      else
        "rz:off"
      end

    timing_label =
      if @measure_object_timings do
        "tm:on"
      else
        "tm:off"
      end

    line1 =
      <<"obj:", i2b(object_count)::binary, " fps:", i2b(fps)::binary, " draw:",
        i2b(draw_ms)::binary>>

    line2 =
      <<"clr:", i2b(clr_ms)::binary, " blt:", i2b(blit_ms)::binary, " ", rotate_label::binary,
        " ", timing_label::binary>>

    with :ok <- Port.fill_rect(port, 0, 0, screen_w, @hud_h, @hud_bg),
         :ok <- Port.draw_string_bg(port, 4, 0, @hud_fg, @hud_bg, 2, line1),
         :ok <- Port.draw_string_bg(port, 4, 16, @hud_dim, @hud_bg, 1, line2) do
      :ok
    end
  end

  # -----------------------------------------------------------------------------
  # Tiny helpers (AtomVM-safe)
  # -----------------------------------------------------------------------------

  defp i2b(i), do: :erlang.integer_to_binary(i)

  defp bool_i(true), do: 1
  defp bool_i(false), do: 0

  defp min_i(a, b) when a <= b, do: a
  defp min_i(_a, b), do: b

  defp max_i(a, b) when a >= b, do: a
  defp max_i(_a, b), do: b
end
