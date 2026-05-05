# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.MovingIcons do
  @moduledoc false

  import Bitwise
  import SampleApp.AtomVMCompat, only: [yield: 0]

  alias AtomLGFX.BinaryBatch
  alias SampleApp.Assets

  # -----------------------------------------------------------------------------
  # Demo config
  # -----------------------------------------------------------------------------

  # Upstream LovyanGFX MovingIcons uses 50 objects.
  @obj_count 50

  # Local demo keeps four icons by default:
  #
  # - info
  # - alert
  # - close
  # - piyopiyo
  #
  # Set this to 3 when comparing more strictly with upstream LovyanGFX MovingIcons.
  @obj_icon_count 3

  # Frame render mode:
  #
  # - :direct_lcd
  #     Render one whole frame directly to LCD.
  #
  # - :strip_buffers
  #     Render the frame in vertical strips. In binary-batch mode, use native
  #     presentation strips. In sync/batch modes, use public sprite buffers.
  #     This mirrors the upstream MovingIcons structure more closely.
  #
  # - :auto
  #     Try strip buffers first, then fall back to direct LCD.
  @frame_render_mode :strip_buffers

  # Frame submit mode:
  #
  # - :binary_batch
  #     Use one multi-target binary-batch submission per frame.
  #
  # - :batch
  #     Use the compact v2 pushRotateZoomList path while keeping per-strip port calls.
  #
  # - :sync
  #     Use one ordinary synchronous pushRotateZoom call per object.
  @frame_submit_mode :binary_batch

  # Binary-batch draw mode:
  #
  # - :push_rotate_zoom_list
  #     Faithful MovingIcons path: dynamic rotate/zoom per visible object.
  #
  # - :push_sprite_list
  #     Benchmark ceiling path: no dynamic rotate/zoom, whole-sprite list blits.
  #
  # - :push_sprite_region_list
  #     Atlas-oriented benchmark path: no dynamic rotate/zoom, source-region list blits.
  @moving_icons_draw_mode :push_rotate_zoom_list

  # Enable conservative native bounds culling for the transform-list batch path.
  #
  # This keeps the native path safe when a generated record cannot affect the
  # current destination bounds.
  @use_approx_cull true

  # Pre-cull strip-frame instance lists before encoding render batches.
  #
  # Native culling skips expensive LovyanGFX calls, but the command binary still
  # carries every record. Strip pre-culling reduces both batch size and native
  # loop work while keeping a conservative bounding box.
  @use_elixir_strip_cull true

  @initial_split_factor 2

  # Native presentation strip height requested by native init.
  # The actual value may be reduced during native allocation and must be queried
  # before strip-buffer animation starts.
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

  # Render-private extended opcode used by the native frame render command.
  # Keep this example-local fast encoder aligned with AtomLGFX.BinaryBatch.
  @render_op_extended 0xFF
  @render_ext_op_push_rotate_zoom_frame_strips 0x01
  @przf_version 1
  @przf_option_has_transparent 0x01
  @przf_option_approx_cull 0x02
  @przf_max_count 0xFFFF

  # Source sprite handles (icons).
  @sprite_info 1
  @sprite_alert 2
  @sprite_close 3
  @sprite_piyopiyo 4

  # Destination sprite handles (double-buffered strip renderer).
  # These are used by sync/batch strip modes. Binary-batch strip mode uses
  # native presentation strips instead.
  @sprite_buf0 10
  @sprite_buf1 11

  # Internal animation zoom units (x1024 fixed-point).
  #
  # - 512  = 0.5x
  # - 2048 = 2.0x
  @zoom_min_x1024 512
  @zoom_max_x1024 2048

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
         :ok <- AtomLGFX.fill_screen(port, @bg),
         {:ok, icon_handles} <- prepare_icon_sprites(port, icons, icon_w, icon_h) do
      case prepare_render_target(port, w, h) do
        {:ok, render_target} ->
          try do
            {_seed, objects} = init_objects(1, @obj_count, w, h, icon_handles)

            # State tuple:
            # {w, h, render_target, flip, objects, icon_handles, fps, frame_count, psec}
            state = {w, h, render_target, 0, objects, icon_handles, 0, 0, 0}

            loop(port, state)
          after
            cleanup_frame_sprites(port)
            cleanup_icon_sprites(port)
          end

        {:error, reason} ->
          cleanup_frame_sprites(port)
          cleanup_icon_sprites(port)
          IO.puts("moving_icons setup failed: #{AtomLGFX.format_error(reason)}")
          {:error, reason}
      end
    else
      {:error, reason} ->
        IO.puts("moving_icons setup failed: #{AtomLGFX.format_error(reason)}")
        {:error, reason}
    end
  end

  # -----------------------------------------------------------------------------
  # Setup / capabilities
  # -----------------------------------------------------------------------------

  defp required_sprite_count do
    case {@frame_render_mode, @frame_submit_mode} do
      {:direct_lcd, _submit_mode} -> 4
      {:strip_buffers, :binary_batch} -> 4
      {:auto, :binary_batch} -> 4
      {:strip_buffers, _submit_mode} -> 6
      {:auto, _submit_mode} -> 6
    end
  end

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

  defp prepare_render_target(port, w, h) do
    case @frame_render_mode do
      :direct_lcd ->
        IO.puts("moving_icons render mode=direct_lcd")
        {:ok, :direct_lcd}

      :strip_buffers ->
        prepare_strip_render_target(port, w, h)

      :auto ->
        case prepare_auto_render_target(port, w, h) do
          {:ok, render_target} ->
            {:ok, render_target}

          {:error, reason} ->
            IO.puts(
              "moving_icons strip buffers unavailable: #{format_local_error(reason)}; falling back to direct_lcd"
            )

            {:ok, :direct_lcd}
        end
    end
  end

  defp prepare_strip_render_target(port, w, h) do
    if native_strip_binary_batch?() do
      with {:ok, native_strip_h} <- AtomLGFX.get_presentation_strip_height(port),
           :ok <- validate_native_presentation_strip_height(native_strip_h) do
        strip_h = min(h, native_strip_h)

        IO.puts(
          "moving_icons render mode=native_strips " <>
            "requested_native_strip_h=#{@requested_native_presentation_strip_h} " <>
            "native_strip_h=#{native_strip_h} " <>
            "frame_strip_h=#{strip_h}"
        )

        {:ok, {:native_strips, strip_h}}
      end
    else
      with {:ok, strip_h} <- prepare_frame_sprites(port, w, h) do
        IO.puts("moving_icons render mode=strip_buffers strip_h=#{strip_h}")
        {:ok, {:strip_buffers, strip_h, @sprite_buf0, @sprite_buf1}}
      end
    end
  end

  defp prepare_auto_render_target(port, w, h) do
    if native_strip_binary_batch?() do
      prepare_strip_render_target(port, w, h)
    else
      with {:ok, strip_h} <- prepare_frame_sprites(port, w, h) do
        IO.puts("moving_icons render mode=strip_buffers strip_h=#{strip_h}")
        {:ok, {:strip_buffers, strip_h, @sprite_buf0, @sprite_buf1}}
      end
    end
  end

  defp validate_native_presentation_strip_height(strip_h)
       when is_integer(strip_h) and strip_h > 0 do
    :ok
  end

  defp validate_native_presentation_strip_height(strip_h) do
    {:error, {:invalid_native_presentation_strip_height, strip_h}}
  end

  defp native_strip_binary_batch? do
    @frame_submit_mode == :binary_batch
  end

  defp prepare_frame_sprites(port, w, h) do
    prepare_frame_sprites_i(port, w, h, @initial_split_factor)
  end

  # Allocate two frame sprites used as strip buffers. If allocation fails, reduce strip height
  # by increasing split_factor until it fits. Stop once strip_h reaches 1.
  defp prepare_frame_sprites_i(port, w, h, split_factor) do
    strip_h = max(1, div_ceil(h, split_factor))

    with :ok <- create_frame_sprite(port, @sprite_buf0, w, strip_h),
         :ok <- create_frame_sprite(port, @sprite_buf1, w, strip_h) do
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
    _ = safe_delete_sprite(port, @sprite_buf1)
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
  # Object init + move
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

  defp move_objects(objects, w, h) do
    move_objects_i(objects, w, h, [])
  end

  defp move_objects_i([], _w, _h, acc), do: :lists.reverse(acc)

  defp move_objects_i(
         [{x, y, dx, dy, src, angle_cdeg, zoom_x1024, dangle_cdeg, dzoom_x1024} | rest],
         w,
         h,
         acc
       ) do
    angle2 = wrap_angle_cdeg(angle_cdeg + dangle_cdeg)

    {x2, dx2} = bounce_i16(x + dx, dx, 0, w - 1)
    {y2, dy2} = bounce_i16(y + dy, dy, 0, h - 1)

    zoom2 = zoom_x1024 + dzoom_x1024
    {zoom3, dzoom2} = bounce_i32(zoom2, dzoom_x1024, @zoom_min_x1024, @zoom_max_x1024)

    move_objects_i(rest, w, h, [{x2, y2, dx2, dy2, src, angle2, zoom3, dangle_cdeg, dzoom2} | acc])
  end

  defp bounce_i16(pos, delta, min_v, max_v) do
    cond do
      pos < min_v -> {min_v, abs(delta)}
      pos > max_v -> {max_v, -abs(delta)}
      true -> {pos, delta}
    end
  end

  defp bounce_i32(pos, delta, min_v, max_v) do
    cond do
      pos < min_v -> {min_v, abs(delta)}
      pos > max_v -> {max_v, -abs(delta)}
      true -> {pos, delta}
    end
  end

  defp wrap_angle_cdeg(a) do
    cond do
      a < 0 -> a + 36_000
      a >= 36_000 -> a - 36_000
      true -> a
    end
  end

  # -----------------------------------------------------------------------------
  # Main loop
  # -----------------------------------------------------------------------------

  defp loop(
         port,
         {w, h, render_target, flip0, objects0, icon_handles, fps0, frame_count0, psec0}
       ) do
    objects = move_objects(objects0, w, h)
    put_last_batch_bytes(0)
    put_last_render_timings(0, 0, 0)
    frame_start_ms = monotonic_ms()

    case render_frame(port, h, render_target, flip0, objects, icon_handles, fps0) do
      {:ok, flip1} ->
        frame_ms = monotonic_ms() - frame_start_ms

        {fps1, frame_count1, psec1} =
          update_fps(
            fps0,
            frame_count0 + 1,
            psec0,
            render_target,
            h,
            frame_ms,
            last_batch_bytes(),
            last_render_timings()
          )

        yield()

        loop(port, {w, h, render_target, flip1, objects, icon_handles, fps1, frame_count1, psec1})

      {:error, reason} ->
        IO.puts("moving_icons render failed: #{AtomLGFX.format_error(reason)}")
        {:error, reason}
    end
  end

  defp render_frame(port, _h, :direct_lcd, _flip0, objects, icon_handles, fps) do
    case @frame_submit_mode do
      :binary_batch ->
        render_frame_direct_lcd_binary_batch(port, objects, icon_handles, fps)

      :batch ->
        render_frame_direct_lcd_batch(port, objects, icon_handles, fps)

      :sync ->
        render_frame_direct_lcd_sync(port, objects, icon_handles, fps)
    end
  end

  defp render_frame(
         port,
         h,
         {:native_strips, strip_h},
         flip0,
         objects,
         icon_handles,
         fps
       ) do
    render_native_strips_binary_batch(port, h, strip_h, flip0, objects, icon_handles, fps)
  end

  defp render_frame(
         port,
         h,
         {:strip_buffers, strip_h, buf0, buf1},
         flip0,
         objects,
         icon_handles,
         fps
       ) do
    case @frame_submit_mode do
      :batch ->
        render_strips_batch(port, h, strip_h, buf0, buf1, flip0, objects, icon_handles, fps)

      :sync ->
        render_strips_sync(port, h, strip_h, buf0, buf1, flip0, objects, icon_handles, fps)

      :binary_batch ->
        {:error, :invalid_strip_buffer_binary_batch_target}
    end
  end

  defp render_frame_direct_lcd_sync(port, objects, icon_handles, fps) do
    with :ok <- AtomLGFX.fill_screen(port, @bg),
         :ok <- draw_all_objects_to_target(port, objects, icon_handles, 0, 0),
         :ok <- draw_fps_overlay(port, 0, fps),
         :ok <- AtomLGFX.display(port) do
      {:ok, 0}
    else
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp render_frame_direct_lcd_batch(port, objects, icon_handles, fps) do
    with :ok <- AtomLGFX.fill_screen(port, @bg),
         {:ok, instances} <- build_direct_lcd_frame_batch(objects, icon_handles),
         :ok <- submit_transform_list_ok(port, 0, instances, 0),
         :ok <- draw_fps_overlay(port, 0, fps),
         :ok <- AtomLGFX.display(port) do
      {:ok, 0}
    else
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp render_frame_direct_lcd_binary_batch(port, objects, icon_handles, fps) do
    with {:ok, draw_commands} <-
           build_direct_lcd_binary_batch_draw_commands(objects, icon_handles),
         :ok <-
           binary_batch_ok(port, [
             BinaryBatch.target(0),
             BinaryBatch.clear(@bg),
             draw_commands,
             fps_overlay_commands(fps),
             BinaryBatch.display()
           ]) do
      {:ok, 0}
    else
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp render_strips_sync(port, h, strip_h, buf0, buf1, flip0, objects, icon_handles, fps) do
    case render_strips_sync_i(port, h, strip_h, 0, buf0, buf1, flip0, objects, icon_handles, fps) do
      {:ok, flip1} ->
        case AtomLGFX.display(port) do
          :ok -> {:ok, flip1}
          {:error, reason} -> {:error, reason}
        end

      {:error, _} = err ->
        err
    end
  end

  defp render_strips_sync_i(_port, h, _strip_h, y, _buf0, _buf1, flip, _objects, _icons, _fps)
       when y >= h do
    {:ok, flip}
  end

  # Render the frame in vertical strips into a sprite buffer, then blit each strip to the LCD.
  #
  # This mirrors upstream LovyanGFX MovingIcons:
  #
  # - flip between two strip sprites
  # - clear the current strip
  # - draw every object into the current strip
  # - draw the FPS overlay only in the first strip
  # - push the strip to the LCD
  defp render_strips_sync_i(port, h, strip_h, y0, buf0, buf1, flip0, objects, icon_handles, fps) do
    {flip1, buf} =
      if flip0 == 0 do
        {1, buf0}
      else
        {0, buf1}
      end

    with :ok <- AtomLGFX.clear(port, @bg, buf),
         :ok <- draw_all_objects_to_target(port, objects, icon_handles, buf, y0),
         :ok <- maybe_draw_fps_overlay(port, buf, y0, fps),
         :ok <- AtomLGFX.push_sprite(port, buf, 0, y0) do
      render_strips_sync_i(
        port,
        h,
        strip_h,
        y0 + strip_h,
        buf0,
        buf1,
        flip1,
        objects,
        icon_handles,
        fps
      )
    else
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp render_strips_batch(port, h, strip_h, buf0, buf1, flip0, objects, icon_handles, fps) do
    case render_strips_batch_i(port, h, strip_h, 0, buf0, buf1, flip0, objects, icon_handles, fps) do
      {:ok, flip1} ->
        case AtomLGFX.display(port) do
          :ok -> {:ok, flip1}
          {:error, reason} -> {:error, reason}
        end

      {:error, _} = err ->
        err
    end
  end

  defp render_strips_batch_i(
         _port,
         h,
         _strip_h,
         y0,
         _buf0,
         _buf1,
         flip,
         _objects,
         _icon_handles,
         _fps
       )
       when y0 >= h do
    {:ok, flip}
  end

  defp render_strips_batch_i(port, h, strip_h, y0, buf0, buf1, flip0, objects, icon_handles, fps) do
    {flip1, buf} =
      if flip0 == 0 do
        {1, buf0}
      else
        {0, buf1}
      end

    with :ok <- AtomLGFX.clear(port, @bg, buf),
         {:ok, instances} <- build_strip_frame_batch(objects, icon_handles, y0, strip_h),
         :ok <- submit_transform_list_ok(port, buf, instances, y0),
         :ok <- maybe_draw_fps_overlay(port, buf, y0, fps),
         :ok <- AtomLGFX.push_sprite(port, buf, 0, y0) do
      render_strips_batch_i(
        port,
        h,
        strip_h,
        y0 + strip_h,
        buf0,
        buf1,
        flip1,
        objects,
        icon_handles,
        fps
      )
    else
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp render_native_strips_binary_batch(port, h, strip_h, flip0, objects, icon_handles, fps) do
    build_started_ms = monotonic_ms()

    case @moving_icons_draw_mode do
      :push_rotate_zoom_list ->
        with {:ok, transform_frame_command} <-
               build_native_transform_frame_command(objects, icon_handles, h) do
          commands = [
            transform_frame_command,
            fps_overlay_commands(fps),
            BinaryBatch.display()
          ]

          put_last_build_ms(monotonic_ms() - build_started_ms)

          case binary_batch_ok(port, commands) do
            :ok -> {:ok, flip0}
            {:error, reason} -> {:error, reason}
          end
        end

      _other ->
        case build_native_strips_binary_batch_commands(
               h,
               strip_h,
               0,
               objects,
               icon_handles,
               fps,
               []
             ) do
          {:ok, commands} ->
            put_last_build_ms(monotonic_ms() - build_started_ms)

            case binary_batch_ok(port, [commands, BinaryBatch.display()]) do
              :ok -> {:ok, flip0}
              {:error, reason} -> {:error, reason}
            end

          {:error, _} = err ->
            err
        end
    end
  end

  defp build_native_strips_binary_batch_commands(
         h,
         _strip_h,
         y0,
         _objects,
         _icon_handles,
         _fps,
         acc
       )
       when y0 >= h do
    {:ok, :lists.reverse(acc)}
  end

  defp build_native_strips_binary_batch_commands(
         h,
         strip_h,
         y0,
         objects,
         icon_handles,
         fps,
         acc
       ) do
    with {:ok, draw_commands} <-
           build_strip_binary_batch_draw_commands(objects, icon_handles, y0, strip_h) do
      strip_commands = [
        BinaryBatch.begin_strip(y0),
        BinaryBatch.target(0),
        BinaryBatch.clear(@bg),
        draw_commands,
        maybe_fps_overlay_commands(y0, fps),
        BinaryBatch.present_strip()
      ]

      build_native_strips_binary_batch_commands(
        h,
        strip_h,
        y0 + strip_h,
        objects,
        icon_handles,
        fps,
        [strip_commands | acc]
      )
    end
  end

  defp draw_all_objects_to_target(port, objects, icon_handles, dst_target, y0) do
    draw_all_objects_to_target_i(port, objects, icon_handles, dst_target, y0)
  end

  defp draw_all_objects_to_target_i(_port, [], _icons, _dst_target, _y0), do: :ok

  defp draw_all_objects_to_target_i(
         port,
         [{x, y, _dx, _dy, src, angle_cdeg, zoom_x1024, _dangle, _dzoom} | rest],
         icon_handles,
         dst_target,
         y0
       ) do
    # Target-local coordinates: subtract the current strip's top y-offset.
    # For direct LCD mode, y0 is 0.
    dst_x = x
    dst_y = y - y0
    angle_deg = deg_from_cdeg(angle_cdeg)
    zoom = zoom_from_x1024(zoom_x1024)

    result =
      if @use_transparent_key do
        AtomLGFX.push_rotate_zoom_to(
          port,
          src,
          dst_target,
          dst_x,
          dst_y,
          angle_deg,
          zoom,
          zoom,
          @transparent_key_color565
        )
      else
        AtomLGFX.push_rotate_zoom_to(
          port,
          src,
          dst_target,
          dst_x,
          dst_y,
          angle_deg,
          zoom,
          zoom
        )
      end

    case result do
      :ok -> draw_all_objects_to_target_i(port, rest, icon_handles, dst_target, y0)
      {:error, reason} -> {:error, reason}
    end
  end

  # -----------------------------------------------------------------------------
  # Binary-batch draw-mode frame building
  # -----------------------------------------------------------------------------

  defp build_direct_lcd_binary_batch_draw_commands(objects, icon_handles) do
    case @moving_icons_draw_mode do
      :push_rotate_zoom_list ->
        with {:ok, instances} <- build_direct_lcd_frame_batch(objects, icon_handles) do
          {:ok, transform_list_commands(instances, 0)}
        end

      :push_sprite_list ->
        with {:ok, instances} <- build_direct_lcd_sprite_push_list(objects, icon_handles) do
          {:ok, sprite_push_list_commands(instances)}
        end

      :push_sprite_region_list ->
        with {:ok, instances} <- build_direct_lcd_sprite_region_list(objects, icon_handles) do
          {:ok, sprite_region_list_commands(instances)}
        end
    end
  end

  defp build_strip_binary_batch_draw_commands(objects, icon_handles, y0, strip_h) do
    case @moving_icons_draw_mode do
      :push_rotate_zoom_list ->
        with {:ok, instances} <- build_strip_frame_batch(objects, icon_handles, y0, strip_h) do
          {:ok, transform_list_commands(instances, y0)}
        end

      :push_sprite_list ->
        with {:ok, instances} <- build_strip_sprite_push_list(objects, icon_handles, y0, strip_h) do
          {:ok, sprite_push_list_commands(instances)}
        end

      :push_sprite_region_list ->
        with {:ok, instances} <-
               build_strip_sprite_region_list(objects, icon_handles, y0, strip_h) do
          {:ok, sprite_region_list_commands(instances)}
        end
    end
  end

  defp build_direct_lcd_sprite_push_list(objects, icon_handles) do
    add_objects_to_sprite_push_list(objects, icon_handles, 0, :all, [])
  end

  defp build_strip_sprite_push_list(objects, icon_handles, y0, strip_h) do
    add_objects_to_sprite_push_list(objects, icon_handles, y0, y0 + strip_h, [])
  end

  defp add_objects_to_sprite_push_list([], _icon_handles, _y0, _y1, acc) do
    {:ok, :lists.reverse(acc)}
  end

  defp add_objects_to_sprite_push_list(
         [{x, y, _dx, _dy, src, _angle_cdeg, _zoom_x1024, _dangle, _dzoom} = object | rest],
         icon_handles,
         y0,
         y1,
         acc
       ) do
    next_acc =
      if object_may_touch_strip?(object, y0, y1) do
        dst_x = x - div(Assets.icon_w(), 2)
        dst_y = y - y0 - div(Assets.icon_h(), 2)

        [{src, dst_x, dst_y} | acc]
      else
        acc
      end

    add_objects_to_sprite_push_list(rest, icon_handles, y0, y1, next_acc)
  end

  defp build_direct_lcd_sprite_region_list(objects, icon_handles) do
    add_objects_to_sprite_region_list(objects, icon_handles, 0, :all, [])
  end

  defp build_strip_sprite_region_list(objects, icon_handles, y0, strip_h) do
    add_objects_to_sprite_region_list(objects, icon_handles, y0, y0 + strip_h, [])
  end

  defp add_objects_to_sprite_region_list([], _icon_handles, _y0, _y1, acc) do
    {:ok, :lists.reverse(acc)}
  end

  defp add_objects_to_sprite_region_list(
         [{x, y, _dx, _dy, src, _angle_cdeg, _zoom_x1024, _dangle, _dzoom} = object | rest],
         icon_handles,
         y0,
         y1,
         acc
       ) do
    next_acc =
      if object_may_touch_strip?(object, y0, y1) do
        src_x = 0
        src_y = 0
        src_w = Assets.icon_w()
        src_h = Assets.icon_h()
        dst_x = x - div(src_w, 2)
        dst_y = y - y0 - div(src_h, 2)

        [{src, src_x, src_y, src_w, src_h, dst_x, dst_y} | acc]
      else
        acc
      end

    add_objects_to_sprite_region_list(rest, icon_handles, y0, y1, next_acc)
  end

  # -----------------------------------------------------------------------------
  # v2 transform-list frame building
  # -----------------------------------------------------------------------------

  defp build_direct_lcd_frame_batch(objects, icon_handles) do
    add_objects_to_batch(objects, icon_handles, 0, :all, [])
  end

  # Keep global object coordinates and pass `y_offset` at submit time.
  # Native code applies the target-local offset.
  #
  # For strip-buffer mode, pre-cull objects that conservatively cannot affect the
  # current vertical strip. This reduces binary-batch size and native transform
  # loop work before native approximate culling runs.
  defp build_strip_frame_batch(objects, icon_handles, y0, strip_h) do
    add_objects_to_batch(objects, icon_handles, y0, y0 + strip_h, [])
  end

  defp add_objects_to_batch([], _icon_handles, _y0, _y1, acc) do
    {:ok, :lists.reverse(acc)}
  end

  defp add_objects_to_batch(
         [{x, y, _dx, _dy, src, angle_cdeg, zoom_x1024, _dangle, _dzoom} = object | rest],
         icon_handles,
         y0,
         y1,
         acc
       ) do
    next_acc =
      if object_may_touch_strip?(object, y0, y1) do
        # Keep global coordinates and submit `y_offset` separately.
        # This matches the native fixed-point hot path.
        dst_x = x
        dst_y = y

        [{src, dst_x, dst_y, angle_cdeg, zoom_x1024, zoom_x1024} | acc]
      else
        acc
      end

    add_objects_to_batch(rest, icon_handles, y0, y1, next_acc)
  end

  defp object_may_touch_strip?(_object, _y0, :all), do: true

  defp object_may_touch_strip?(_object, _y0, _y1) when not @use_elixir_strip_cull, do: true

  defp object_may_touch_strip?(
         {_x, y, _dx, _dy, _src, _angle_cdeg, zoom_x1024, _dangle, _dzoom},
         y0,
         y1
       ) do
    radius = approx_icon_transform_radius_px(zoom_x1024)

    y + radius >= y0 and y - radius < y1
  end

  defp approx_icon_transform_radius_px(zoom_x1024) do
    # Conservative center-pivot approximation. This mirrors the native culling
    # shape while staying integer-only for AtomVM.
    base = Assets.icon_w() + Assets.icon_h()

    div(base * zoom_x1024 + 2047, 2048) + 1
  end

  defp transform_list_commands([], _y0), do: []

  defp transform_list_commands(instances, y0) do
    [BinaryBatch.push_rotate_zoom_list(instances, transform_list_opts(y0))]
  end

  defp sprite_push_list_commands([]), do: []

  defp sprite_push_list_commands(instances) do
    [BinaryBatch.push_sprite_list(instances, sprite_list_opts())]
  end

  defp sprite_region_list_commands([]), do: []

  defp sprite_region_list_commands(instances) do
    [BinaryBatch.push_sprite_region_list(instances, sprite_region_list_opts())]
  end

  defp submit_transform_list_ok(_port, _dst_target, [], _y0), do: :ok

  defp submit_transform_list_ok(port, dst_target, instances, y0) do
    case AtomLGFX.push_rotate_zoom_list_to(port, dst_target, instances, transform_list_opts(y0)) do
      :ok ->
        :ok

      {:error, reason} ->
        {:error, reason}
    end
  end

  defp transform_list_opts(y0) do
    if @use_transparent_key do
      [transparent: @transparent_key_color565, y_offset: y0, approx_cull: @use_approx_cull]
    else
      [y_offset: y0, approx_cull: @use_approx_cull]
    end
  end

  # Fast path for the native transform-frame command.
  #
  # The generic `BinaryBatch.push_rotate_zoom_frame_strips/2` helper accepts a
  # list of normalized instance tuples. That is convenient for tests and normal
  # callers, but it allocates an intermediate list before encoding. This demo
  # already stores object state in the exact shape needed for MovingIcons, so the
  # benchmark path encodes PRZF records directly from the object list.
  defp build_native_transform_frame_command(objects, icon_handles, frame_height)
       when is_integer(frame_height) and frame_height > 0 do
    with {:ok, count, records} <- encode_transform_frame_records(objects, icon_handles) do
      command = [
        <<@render_op_extended, @render_ext_op_push_rotate_zoom_frame_strips,
          transform_frame_flags()::little-16, ?P, ?R, ?Z, ?F, @przf_version,
          transform_frame_options(), transform_frame_transparent()::little-16,
          frame_height::little-16, @bg::little-16, count::little-16>>,
        records
      ]

      {:ok, command}
    end
  end

  defp build_native_transform_frame_command(_objects, _icon_handles, frame_height) do
    {:error, {:bad_frame_height, frame_height}}
  end

  # This demo uses an RGB565 transparent key, so no transparent-index protocol
  # flag is needed. If the example later switches to indexed source sprites, use
  # the public BinaryBatch helper or extend this encoder deliberately.
  defp transform_frame_flags, do: 0

  defp transform_frame_options do
    options =
      if @use_transparent_key do
        @przf_option_has_transparent
      else
        0
      end

    if @use_approx_cull do
      options ||| @przf_option_approx_cull
    else
      options
    end
  end

  defp transform_frame_transparent do
    if @use_transparent_key do
      @transparent_key_color565
    else
      0
    end
  end

  defp encode_transform_frame_records(objects, icon_handles) do
    case encode_transform_frame_records_i(objects, icon_handles, 0, []) do
      {:ok, 0, _records} -> {:error, :empty_batch}
      other -> other
    end
  end

  defp encode_transform_frame_records_i([], _icon_handles, count, acc) do
    {:ok, count, :lists.reverse(acc)}
  end

  defp encode_transform_frame_records_i(
         [{x, y, _dx, _dy, src, angle_cdeg, zoom_x1024, _dangle, _dzoom} | rest],
         icon_handles,
         count,
         acc
       )
       when count < @przf_max_count do
    record =
      <<src, 0, x::little-signed-16, y::little-signed-16, angle_cdeg::little-16,
        zoom_x1024::little-16, zoom_x1024::little-16>>

    encode_transform_frame_records_i(rest, icon_handles, count + 1, [record | acc])
  end

  defp encode_transform_frame_records_i([_object | _rest], _icon_handles, count, _acc)
       when count >= @przf_max_count do
    {:error, {:too_many_sprite_transform_instances, count + 1}}
  end

  defp encode_transform_frame_records_i([object | _rest], _icon_handles, _count, _acc) do
    {:error, {:bad_transform_frame_object, object}}
  end

  defp sprite_list_opts do
    if @use_transparent_key do
      [transparent: @transparent_key_color565]
    else
      []
    end
  end

  defp sprite_region_list_opts do
    if @use_transparent_key do
      [transparent: @transparent_key_color565]
    else
      []
    end
  end

  defp binary_batch_ok(port, commands) do
    encode_started_ms = monotonic_ms()
    command_binary = IO.iodata_to_binary(commands)
    encode_ms = monotonic_ms() - encode_started_ms

    put_last_batch_bytes(byte_size(command_binary))
    put_last_encode_ms(encode_ms)

    submit_started_ms = monotonic_ms()

    result =
      case BinaryBatch.render(port, command_binary) do
        :ok -> :ok
        {:error, reason} -> {:error, reason}
      end

    put_last_submit_ms(monotonic_ms() - submit_started_ms)
    result
  end

  defp src_handle_for_index(0, icon_handles), do: elem(icon_handles, 0)
  defp src_handle_for_index(1, icon_handles), do: elem(icon_handles, 1)
  defp src_handle_for_index(2, icon_handles), do: elem(icon_handles, 2)
  defp src_handle_for_index(3, icon_handles), do: elem(icon_handles, 3)

  # -----------------------------------------------------------------------------
  # FPS overlay
  # -----------------------------------------------------------------------------

  defp maybe_draw_fps_overlay(_port, _target, y0, _fps) when y0 != 0, do: :ok

  defp maybe_draw_fps_overlay(port, target, 0, fps) do
    draw_fps_overlay(port, target, fps)
  end

  defp draw_fps_overlay(port, target, fps) do
    with :ok <- AtomLGFX.set_text_font_preset(port, :ascii, target),
         :ok <- AtomLGFX.set_text_size(port, 2, target),
         :ok <- AtomLGFX.set_text_color(port, 0xFFFF, nil, target) do
      AtomLGFX.draw_string(port, 0, 0, fps_overlay_text(fps), target)
    end
  end

  defp maybe_fps_overlay_commands(y0, _fps) when y0 != 0, do: []

  defp maybe_fps_overlay_commands(0, fps) do
    fps_overlay_commands(fps)
  end

  defp fps_overlay_commands(fps) do
    [
      BinaryBatch.set_text_font_preset(:ascii),
      BinaryBatch.set_text_size(2),
      BinaryBatch.set_text_color(0xFFFF),
      BinaryBatch.draw_string(0, 0, fps_overlay_text(fps))
    ]
  end

  defp fps_overlay_text(fps) do
    "obj:#{@obj_count}  fps:#{fps}"
  end

  defp update_fps(fps, frame_count, psec, render_target, h, frame_ms, batch_bytes, render_timings) do
    sec = div(monotonic_ms(), 1000)

    if psec != sec do
      log_frame_stats(frame_count, render_target, h, frame_ms, batch_bytes, render_timings)
      {frame_count, 0, sec}
    else
      {fps, frame_count, psec}
    end
  end

  defp monotonic_ms do
    :erlang.monotonic_time(:millisecond)
  end

  defp put_last_batch_bytes(batch_bytes)
       when is_integer(batch_bytes) and batch_bytes >= 0 do
    :erlang.put(:moving_icons_last_batch_bytes, batch_bytes)
    :ok
  end

  defp last_batch_bytes do
    case :erlang.get(:moving_icons_last_batch_bytes) do
      value when is_integer(value) and value >= 0 -> value
      _ -> 0
    end
  end

  defp put_last_render_timings(build_ms, encode_ms, submit_ms)
       when is_integer(build_ms) and build_ms >= 0 and is_integer(encode_ms) and encode_ms >= 0 and
              is_integer(submit_ms) and submit_ms >= 0 do
    :erlang.put(:moving_icons_last_render_timings, {build_ms, encode_ms, submit_ms})
    :ok
  end

  defp put_last_build_ms(build_ms) when is_integer(build_ms) and build_ms >= 0 do
    {_old_build_ms, encode_ms, submit_ms} = last_render_timings()
    put_last_render_timings(build_ms, encode_ms, submit_ms)
  end

  defp put_last_encode_ms(encode_ms) when is_integer(encode_ms) and encode_ms >= 0 do
    {build_ms, _old_encode_ms, submit_ms} = last_render_timings()
    put_last_render_timings(build_ms, encode_ms, submit_ms)
  end

  defp put_last_submit_ms(submit_ms) when is_integer(submit_ms) and submit_ms >= 0 do
    {build_ms, encode_ms, _old_submit_ms} = last_render_timings()
    put_last_render_timings(build_ms, encode_ms, submit_ms)
  end

  defp last_render_timings do
    case :erlang.get(:moving_icons_last_render_timings) do
      {build_ms, encode_ms, submit_ms}
      when is_integer(build_ms) and build_ms >= 0 and is_integer(encode_ms) and encode_ms >= 0 and
             is_integer(submit_ms) and submit_ms >= 0 ->
        {build_ms, encode_ms, submit_ms}

      _ ->
        {0, 0, 0}
    end
  end

  defp log_frame_stats(fps, render_target, h, frame_ms, batch_bytes, render_timings) do
    {build_ms, encode_ms, submit_ms} = render_timings

    IO.puts(
      "moving_icons stats " <>
        "obj_count=#{@obj_count} " <>
        "render_mode=#{render_mode_name(render_target)} " <>
        "submit_mode=#{@frame_submit_mode} " <>
        "draw_mode=#{@moving_icons_draw_mode} " <>
        "fps=#{fps} " <>
        "frame_ms=#{frame_ms} " <>
        "build_ms=#{build_ms} " <>
        "encode_ms=#{encode_ms} " <>
        "submit_ms=#{submit_ms} " <>
        "batch_bytes=#{batch_bytes} " <>
        "strip_count=#{strip_count(render_target, h)}"
    )
  end

  defp render_mode_name(:direct_lcd), do: :direct_lcd
  defp render_mode_name({:native_strips, _strip_h}), do: :native_strips
  defp render_mode_name({:strip_buffers, _strip_h, _buf0, _buf1}), do: :strip_buffers

  defp strip_count(:direct_lcd, _h), do: 1

  defp strip_count({:native_strips, strip_h}, h) do
    div_ceil(h, strip_h)
  end

  defp strip_count({:strip_buffers, strip_h, _buf0, _buf1}, h) do
    div_ceil(h, strip_h)
  end

  # -----------------------------------------------------------------------------
  # Misc
  # -----------------------------------------------------------------------------

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
      "moving_icons config obj_count=#{@obj_count} icon_count=#{@obj_icon_count} render_mode=#{inspect(@frame_render_mode)} submit_mode=#{inspect(@frame_submit_mode)} draw_mode=#{inspect(@moving_icons_draw_mode)}"
    )
  end

  defp deg_from_cdeg(angle_cdeg) when is_integer(angle_cdeg) do
    angle_cdeg / 100.0
  end

  defp zoom_from_x1024(zoom_x1024) when is_integer(zoom_x1024) do
    zoom_x1024 / 1024.0
  end

  defp div_ceil(a, b) when is_integer(a) and is_integer(b) and b > 0 do
    div(a + b - 1, b)
  end

  defp format_local_error({:frame_sprite_alloc_failed, w, h, split_factor, reason}) do
    "frame sprite alloc failed w=#{w} h=#{h} split_factor=#{split_factor} reason=#{AtomLGFX.format_error(reason)}"
  end

  defp format_local_error(reason), do: inspect(reason)
end
