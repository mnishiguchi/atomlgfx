# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.MovingIcons do
  @moduledoc false

  import Bitwise

  alias AtomLGFX.RenderScene
  alias SampleApp.Assets

  # -----------------------------------------------------------------------------
  # Demo config
  # -----------------------------------------------------------------------------

  # Upstream LovyanGFX MovingIcons uses 50 objects.
  @obj_count 50

  # Local demo keeps four icons available:
  #
  # - info
  # - alert
  # - close
  # - piyopiyo
  #
  # Set this to 3 when comparing more strictly with upstream LovyanGFX MovingIcons.
  @obj_icon_count 3

  # Native presentation strip height requested by native init.
  # The actual value may be reduced during native allocation and must be queried
  # before retained rendering starts.
  @requested_native_presentation_strip_h 160

  # Icon sprites are authored with a solid background color. Use a transparent-color key so
  # that background pixels do not overwrite what is already in the destination.
  #
  # - Non-index display colors use RGB565 on the wire.
  # - `0x0000` matches the upstream LovyanGFX demo (`transparent=0`).
  @use_transparent_key true
  @transparent_key_color565 0x0000

  # Background fill color for the playfield and frame buffers (RGB565).
  @bg 0x0000

  # Capability bit: sprite operations available.
  @cap_sprite 1 <<< 0

  # Source sprite handles (icons).
  @sprite_info 1
  @sprite_alert 2
  @sprite_close 3
  @sprite_piyopiyo 4

  # Internal animation zoom units (x1024 fixed-point).
  #
  # - 512  = 0.5x
  # - 2048 = 2.0x
  @zoom_min_x1024 512
  @zoom_max_x1024 2048

  @retained_update_policy :bounce
  @retained_stats_poll_ms 1_000
  @target_fps 10

  # -----------------------------------------------------------------------------
  # Public entry
  # -----------------------------------------------------------------------------

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

    with {:ok, caps} <- AtomLGFX.get_caps(port),
         :ok <- ensure_sprite_support(caps, required_sprite_count()),
         {:ok, true} <- AtomLGFX.supports_retained_render?(port),
         :ok <- AtomLGFX.fill_screen(port, @bg),
         {:ok, icon_handles} <- prepare_icon_sprites(port, icons, icon_w, icon_h) do
      try do
        run_retained_demo(port, w, h, icon_handles)
      after
        cleanup_icon_sprites(port)
      end
    else
      {:ok, false} ->
        reason = :cap_retained_render_missing
        IO.puts("moving_icons setup failed: #{AtomLGFX.format_error(reason)}")
        {:error, reason}

      {:error, reason} ->
        IO.puts("moving_icons setup failed: #{AtomLGFX.format_error(reason)}")
        {:error, reason}
    end
  end

  # -----------------------------------------------------------------------------
  # Retained native renderer
  # -----------------------------------------------------------------------------

  defp run_retained_demo(port, _w, h, icon_handles) do
    {_seed, objects} = init_objects(1, @obj_count, _w, h, icon_handles)

    case prepare_retained_renderer(port, h, objects, icon_handles) do
      {:ok, instance_buffer, scene} ->
        try do
          with :ok <- RenderScene.start(port, scene, mode: :exclusive) do
            monitor_retained_renderer(port, scene, 0, monotonic_ms(), false)
          end
        after
          cleanup_retained_renderer(port, instance_buffer, scene)
        end

      {:error, reason} ->
        IO.puts("moving_icons setup failed: #{AtomLGFX.format_error(reason)}")
        {:error, reason}
    end
  end

  defp prepare_retained_renderer(port, h, objects, icon_handles) do
    with {:ok, native_strip_h} <- AtomLGFX.get_presentation_strip_height(port),
         :ok <- validate_native_presentation_strip_height(native_strip_h),
         {:ok, retained_instances} <- retained_instance_records(objects, icon_handles) do
      strip_h = min(h, native_strip_h)
      sources = retained_source_handles(icon_handles)

      IO.puts(
        "moving_icons render mode=retained_native " <>
          "target_fps=#{@target_fps} " <>
          "requested_native_strip_h=#{@requested_native_presentation_strip_h} " <>
          "native_strip_h=#{native_strip_h} " <>
          "frame_strip_h=#{strip_h}"
      )

      create_retained_renderer_resources(port, strip_h, retained_instances, sources)
    end
  end

  defp validate_native_presentation_strip_height(value)
       when is_integer(value) and value > 0,
       do: :ok

  defp validate_native_presentation_strip_height(other),
    do: {:error, {:bad_native_presentation_strip_height, other}}

  defp create_retained_renderer_resources(port, strip_h, retained_instances, sources) do
    case AtomLGFX.create_instance_buffer(port, layout: :sprite_transform_2d, capacity: @obj_count) do
      {:ok, instance_buffer} ->
        case AtomLGFX.write_instances(port, instance_buffer, retained_instances) do
          :ok ->
            case create_retained_render_scene(port, instance_buffer, strip_h, sources) do
              {:ok, scene} ->
                {:ok, instance_buffer, scene}

              {:error, reason} ->
                cleanup_instance_buffer(port, instance_buffer)
                {:error, reason}
            end

          {:error, reason} ->
            cleanup_instance_buffer(port, instance_buffer)
            {:error, reason}
        end

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp create_retained_render_scene(port, instance_buffer, strip_h, sources) do
    RenderScene.create(
      port,
      renderer: :sprite_transform,
      instance_buffer: instance_buffer,
      sprites: sources,
      strip_height: strip_h,
      background_color: @bg,
      transparent_color: retained_transparent_color(),
      motion: @retained_update_policy,
      zoom_min: zoom_from_x1024(@zoom_min_x1024),
      zoom_max: zoom_from_x1024(@zoom_max_x1024)
    )
  end

  defp retained_transparent_color do
    if @use_transparent_key do
      @transparent_key_color565
    else
      nil
    end
  end

  defp retained_source_handles(icon_handles) do
    case @obj_icon_count do
      1 ->
        [elem(icon_handles, 0)]

      2 ->
        [elem(icon_handles, 0), elem(icon_handles, 1)]

      3 ->
        [elem(icon_handles, 0), elem(icon_handles, 1), elem(icon_handles, 2)]

      _ ->
        [
          elem(icon_handles, 0),
          elem(icon_handles, 1),
          elem(icon_handles, 2),
          elem(icon_handles, 3)
        ]
    end
  end

  defp retained_instance_records(objects, icon_handles) do
    retained_instance_records_i(objects, retained_source_handles(icon_handles), [])
  end

  defp retained_instance_records_i([], _sources, acc), do: {:ok, :lists.reverse(acc)}

  defp retained_instance_records_i(
         [{x, y, dx, dy, src, angle_cdeg, zoom_x1024, dangle_cdeg, dzoom_x1024} | rest],
         sources,
         acc
       ) do
    with {:ok, source_index} <- source_index_for_handle(sources, src, 0) do
      instance = {
        source_index,
        x,
        y,
        dx,
        dy,
        deg_from_cdeg(angle_cdeg),
        zoom_from_x1024(zoom_x1024),
        deg_from_cdeg(dangle_cdeg),
        zoom_from_x1024(dzoom_x1024)
      }

      retained_instance_records_i(rest, sources, [instance | acc])
    end
  end

  defp source_index_for_handle([], src_handle, _index) do
    {:error, {:bad_retained_render_sources, src_handle}}
  end

  defp source_index_for_handle([current_handle | _rest], src_handle, index)
       when current_handle == src_handle,
       do: {:ok, index}

  defp source_index_for_handle([_other | rest], src_handle, index) do
    source_index_for_handle(rest, src_handle, index + 1)
  end

  defp monitor_retained_renderer(port, scene, last_frame_count, last_poll_ms, target_announced?) do
    receive do
    after
      @retained_stats_poll_ms ->
        case RenderScene.stats(port, scene) do
          {:ok, %{running: true} = stats} ->
            now_ms = monotonic_ms()
            fps = retained_fps(stats.frame_count - last_frame_count, now_ms - last_poll_ms)
            maybe_log_target_fps(fps, target_announced?)
            log_retained_stats(stats, fps)

            monitor_retained_renderer(
              port,
              scene,
              stats.frame_count,
              now_ms,
              target_announced? or fps >= @target_fps
            )

          {:ok, %{running: false}} ->
            {:error, :retained_renderer_stopped}

          {:error, reason} ->
            IO.puts("moving_icons render failed: #{AtomLGFX.format_error(reason)}")
            {:error, reason}
        end
    end
  end

  defp retained_fps(frame_delta, elapsed_ms)
       when is_integer(frame_delta) and frame_delta > 0 and is_integer(elapsed_ms) and
              elapsed_ms > 0 do
    div(frame_delta * 1000 + div(elapsed_ms, 2), elapsed_ms)
  end

  defp retained_fps(_frame_delta, _elapsed_ms), do: 0

  defp maybe_log_target_fps(fps, true) when is_integer(fps), do: :ok

  defp maybe_log_target_fps(fps, false) when is_integer(fps) and fps >= @target_fps do
    IO.puts("moving_icons target_fps=#{@target_fps} reached fps=#{fps}")
    :ok
  end

  defp maybe_log_target_fps(_fps, false), do: :ok

  defp log_retained_stats(stats, fps) do
    IO.puts(
      "moving_icons stats " <>
        "renderer=retained_native " <>
        "obj_count=#{stats.object_count} " <>
        "fps=#{fps} " <>
        "target_fps=#{@target_fps} " <>
        "frame_ms=#{round_us_to_ms(stats.last_frame_us)} " <>
        "update_ms=#{round_us_to_ms(stats.last_update_us)} " <>
        "draw_ms=#{round_us_to_ms(stats.last_draw_us)} " <>
        "present_ms=#{round_us_to_ms(stats.last_present_us)} " <>
        "drawn=#{stats.drawn_count} " <>
        "culled=#{stats.culled_count} " <>
        "strip_h=#{stats.strip_height}"
    )
  end

  defp round_us_to_ms(value) when is_integer(value) and value >= 0 do
    div(value + 500, 1000)
  end

  defp round_us_to_ms(_value), do: 0

  defp cleanup_retained_renderer(port, instance_buffer, scene) do
    _ = safe_stop_render_scene(port, scene)
    _ = safe_destroy_render_scene(port, scene)
    _ = cleanup_instance_buffer(port, instance_buffer)
    :ok
  end

  defp safe_stop_render_scene(port, handle) do
    case RenderScene.stop(port, handle) do
      :ok -> :ok
      {:error, _} -> :ok
    end
  end

  defp safe_destroy_render_scene(port, handle) do
    case RenderScene.destroy(port, handle) do
      :ok -> :ok
      {:error, _} -> :ok
    end
  end

  defp cleanup_instance_buffer(port, handle) do
    case AtomLGFX.delete_instance_buffer(port, handle) do
      :ok -> :ok
      {:error, _} -> :ok
    end
  end

  # -----------------------------------------------------------------------------
  # Setup / capabilities
  # -----------------------------------------------------------------------------

  defp required_sprite_count, do: 4

  defp ensure_sprite_support(%{feature_bits: feature_bits, max_sprites: max_sprites}, needed) do
    cond do
      (feature_bits &&& @cap_sprite) == 0 ->
        {:error, :cap_sprite_missing}

      max_sprites < needed ->
        {:error, {:insufficient_sprite_capacity, max_sprites, needed}}

      true ->
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

  defp cleanup_icon_sprites(port) do
    _ = safe_delete_sprite(port, @sprite_info)
    _ = safe_delete_sprite(port, @sprite_alert)
    _ = safe_delete_sprite(port, @sprite_close)
    _ = safe_delete_sprite(port, @sprite_piyopiyo)
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

    dx0 = (band3(r3) + 1) * sign(i &&& 1)
    dy0 = (band3(r4) + 1) * sign(i &&& 2)

    dr_deg = (band3(r5) + 1) * sign(i &&& 2)
    dr_cdeg = dr_deg * 100

    # Internal zoom state:
    #
    # - z_x1024: 1.0..1.9 (step 0.1)
    # - dz_x1024: 0.01..0.10 (step 0.01)
    #
    # Use higher bits for small random ranges. Low bits of simple LCGs are highly
    # patterned and can make same-type icons move or rotate in lockstep.
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

  # -----------------------------------------------------------------------------
  # Misc
  # -----------------------------------------------------------------------------

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
        "renderer=retained_native " <>
        "update=#{@retained_update_policy} " <>
        "target_fps=#{@target_fps} " <>
        "requested_native_strip_h=#{@requested_native_presentation_strip_h}"
    )
  end

  defp deg_from_cdeg(angle_cdeg) when is_integer(angle_cdeg) do
    angle_cdeg / 100.0
  end

  defp zoom_from_x1024(zoom_x1024) when is_integer(zoom_x1024) do
    zoom_x1024 / 1024.0
  end
end
