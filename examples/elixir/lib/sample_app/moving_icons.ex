defmodule SampleApp.MovingIcons do
  @moduledoc false

  import Bitwise

  alias LGFXPort, as: Port
  alias SampleApp.Assets
  import SampleApp.AtomVMCompat, only: [yield: 0]

  # -----------------------------------------------------------------------------
  # Demo config (LovyanGFX-style)
  # -----------------------------------------------------------------------------

  @obj_count 5

  # Strip-buffer mode gives nicer composition.
  # Direct-LCD mode is simpler and useful for bring-up when sprite buffering is unstable.
  #
  # - :auto
  #     Try strip buffers first, then fall back to direct LCD.
  #
  # - :strip_buffers
  #     Require strip-buffer rendering.
  #
  # - :direct_lcd
  #     Render directly to LCD every frame.
  @frame_render_mode :auto

  @initial_split_factor 2

  # Icon sprites are authored with a solid background color. Use a transparent-color key so
  # that background pixels do not overwrite what is already in the destination.
  #
  # - Protocol expects the transparent key as RGB565 u16.
  # - `0x0000` matches the upstream LovyanGFX demo (`transparent=0`).
  @use_transparent_key true
  @transparent_key_rgb888 0x000000
  @transparent_key_rgb565 0x0000

  # Background fill color for the playfield and frame buffers (RGB888).
  @bg 0x000000

  # Capability bit: sprite operations available.
  @cap_sprite 1 <<< 0

  # Source sprite handles (icons).
  @sprite_info 1
  @sprite_alert 2
  @sprite_close 3
  @sprite_piyopiyo 4

  # Destination sprite handles (double-buffered strip renderer).
  @sprite_buf0 10
  @sprite_buf1 11

  # Zoom bounds in protocol units (x1024 fixed-point).
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

    with {:ok, caps} <- Port.get_caps(port),
         :ok <- ensure_sprite_support(caps, 6),
         :ok <- Port.fill_screen(port, @bg),
         {:ok, icon_handles} <- prepare_icon_sprites(port, icons, icon_w, icon_h),
         {:ok, render_target} <- prepare_render_target(port, w, h) do
      try do
        {_seed, objects} = init_objects(1, @obj_count, w, h)

        # State tuple:
        # {w, h, render_target, flip, objects, icon_handles}
        state = {w, h, render_target, 0, objects, icon_handles}

        loop(port, state)
      after
        cleanup_frame_sprites(port)
        cleanup_icon_sprites(port)
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

    with :ok <- Port.create_sprite(port, icon_w, icon_h, sprite_target),
         :ok <- Port.clear(port, @transparent_key_rgb888, sprite_target),
         :ok <- Port.push_image_rgb565(port, 0, 0, icon_w, icon_h, pixels, 0, sprite_target),
         :ok <- Port.set_pivot(port, sprite_target, pivot_x, pivot_y) do
      :ok
    else
      {:error, reason} ->
        _ = Port.delete_sprite(port, sprite_target)
        {:error, {:sprite_setup_failed, sprite_target, reason}}
    end
  end

  defp prepare_render_target(port, w, h) do
    case @frame_render_mode do
      :direct_lcd ->
        IO.puts("moving_icons render mode=direct_lcd")
        {:ok, :direct_lcd}

      :strip_buffers ->
        with {:ok, strip_h} <- prepare_frame_sprites(port, w, h) do
          IO.puts("moving_icons render mode=strip_buffers strip_h=#{strip_h}")
          {:ok, {:strip_buffers, strip_h, @sprite_buf0, @sprite_buf1}}
        end

      :auto ->
        case prepare_frame_sprites(port, w, h) do
          {:ok, strip_h} ->
            IO.puts("moving_icons render mode=strip_buffers strip_h=#{strip_h}")
            {:ok, {:strip_buffers, strip_h, @sprite_buf0, @sprite_buf1}}

          {:error, reason} ->
            IO.puts(
              "moving_icons strip buffers unavailable: #{format_local_error(reason)}; falling back to direct_lcd"
            )

            {:ok, :direct_lcd}
        end
    end
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
    # Use a fixed depth for now; align with LCD depth later if you expose it via the protocol.
    color_depth = 16
    Port.create_sprite(port, w, h, color_depth, target)
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
  # Object init + move
  # -----------------------------------------------------------------------------

  # Object tuple:
  # {x, y, dx, dy, img_index, r_cdeg, z_x1024, dr_cdeg, dz_x1024}
  defp init_objects(seed, count, w, h) do
    init_objects_i(seed, 0, count, w, h, [])
  end

  defp init_objects_i(seed, _i, count, _w, _h, acc) when count <= 0 do
    {seed, :lists.reverse(acc)}
  end

  defp init_objects_i(seed, i, count, w, h, acc) do
    {seed, r1} = rand_u32(seed)
    {seed, r2} = rand_u32(seed)
    {seed, r3} = rand_u32(seed)
    {seed, r4} = rand_u32(seed)
    {seed, r5} = rand_u32(seed)

    img = rem(i, 4)

    x = rem(r1, w)
    y = rem(r2, h)

    dx0 = (band3(r3) + 1) * sign(i &&& 1)
    dy0 = (band3(r4) + 1) * sign(i &&& 2)

    dr_deg = (band3(r5) + 1) * sign(i &&& 2)
    dr_cdeg = dr_deg * 100

    # Zoom and delta-zoom in protocol units:
    # - z_x1024: 1.0..1.9 (step 0.1)
    # - dz_x1024: 0.01..0.10 (step 0.01)
    z10 = rem(r3, 10) + 10
    z_x1024 = div(z10 * 1024, 10)

    dz100 = rem(r4, 10) + 1
    dz_x1024 = div(dz100 * 1024, 100)

    obj = {x, y, dx0, dy0, img, 0, z_x1024, dr_cdeg, dz_x1024}
    init_objects_i(seed, i + 1, count - 1, w, h, [obj | acc])
  end

  defp band3(u32), do: u32 &&& 3

  defp sign(0), do: -1
  defp sign(_), do: 1

  defp move_objects(objects, w, h) do
    move_objects_i(objects, w, h, [])
  end

  defp move_objects_i([], _w, _h, acc), do: :lists.reverse(acc)

  defp move_objects_i([{x, y, dx, dy, img, r_cdeg, z_x1024, dr_cdeg, dz_x1024} | rest], w, h, acc) do
    r2 = wrap_angle_cdeg(r_cdeg + dr_cdeg)

    {x2, dx2} = bounce_i16(x + dx, dx, 0, w - 1)
    {y2, dy2} = bounce_i16(y + dy, dy, 0, h - 1)

    z2 = z_x1024 + dz_x1024
    {z3, dz2} = bounce_i32(z2, dz_x1024, @zoom_min_x1024, @zoom_max_x1024)

    move_objects_i(rest, w, h, [{x2, y2, dx2, dy2, img, r2, z3, dr_cdeg, dz2} | acc])
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
         {w, h, render_target, flip0, objects0, icon_handles}
       ) do
    objects = move_objects(objects0, w, h)

    case render_frame(port, h, render_target, flip0, objects, icon_handles) do
      {:ok, flip1} ->
        yield()

        loop(port, {w, h, render_target, flip1, objects, icon_handles})

      {:error, reason} ->
        IO.puts("moving_icons render failed: #{Port.format_error(reason)}")
        {:error, reason}
    end
  end

  defp render_frame(port, _h, :direct_lcd, _flip0, objects, icon_handles) do
    with :ok <- Port.fill_screen(port, @bg),
         :ok <- draw_all_objects_to_target(port, objects, icon_handles, 0, 0),
         :ok <- Port.display(port) do
      {:ok, 0}
    else
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp render_frame(
         port,
         h,
         {:strip_buffers, strip_h, buf0, buf1},
         flip0,
         objects,
         icon_handles
       ) do
    render_strips(port, h, strip_h, buf0, buf1, flip0, objects, icon_handles)
  end

  defp render_strips(port, h, strip_h, buf0, buf1, flip0, objects, icon_handles) do
    case render_strips_i(port, h, strip_h, 0, buf0, buf1, flip0, objects, icon_handles) do
      {:ok, flip1} ->
        case Port.display(port) do
          :ok -> {:ok, flip1}
          {:error, reason} -> {:error, reason}
        end

      {:error, _} = err ->
        err
    end
  end

  defp render_strips_i(_port, h, _strip_h, y, _buf0, _buf1, flip, _objects, _icons)
       when y >= h do
    {:ok, flip}
  end

  # Render the frame in vertical strips into a sprite buffer, then blit each strip to the LCD.
  # This avoids per-object "erase then redraw" artifacts when objects overlap.
  defp render_strips_i(port, h, strip_h, y0, buf0, buf1, flip0, objects, icon_handles) do
    {flip1, buf} =
      if flip0 == 0 do
        {1, buf0}
      else
        {0, buf1}
      end

    with :ok <- Port.clear(port, @bg, buf),
         :ok <- draw_all_objects_to_target(port, objects, icon_handles, buf, y0),
         :ok <- Port.push_sprite(port, buf, 0, y0) do
      render_strips_i(
        port,
        h,
        strip_h,
        y0 + strip_h,
        buf0,
        buf1,
        flip1,
        objects,
        icon_handles
      )
    else
      {:error, reason} ->
        {:error, reason}
    end
  end

  defp draw_all_objects_to_target(port, objects, icon_handles, dst_target, y0) do
    draw_all_objects_to_target_i(port, objects, icon_handles, dst_target, y0)
  end

  defp draw_all_objects_to_target_i(_port, [], _icons, _dst_target, _y0), do: :ok

  defp draw_all_objects_to_target_i(
         port,
         [{x, y, _dx, _dy, img, r_cdeg, z_x1024, _dr, _dz} | rest],
         icon_handles,
         dst_target,
         y0
       ) do
    src =
      case img do
        0 -> elem(icon_handles, 0)
        1 -> elem(icon_handles, 1)
        2 -> elem(icon_handles, 2)
        3 -> elem(icon_handles, 3)
      end

    # Target-local coordinates: subtract the current strip's top y-offset.
    # For direct LCD mode, y0 is 0.
    dst_x = x
    dst_y = y - y0

    result =
      if @use_transparent_key do
        Port.push_rotate_zoom_to(
          port,
          src,
          dst_target,
          dst_x,
          dst_y,
          r_cdeg,
          z_x1024,
          z_x1024,
          @transparent_key_rgb565
        )
      else
        Port.push_rotate_zoom_to(
          port,
          src,
          dst_target,
          dst_x,
          dst_y,
          r_cdeg,
          z_x1024,
          z_x1024
        )
      end

    case result do
      :ok -> draw_all_objects_to_target_i(port, rest, icon_handles, dst_target, y0)
      {:error, reason} -> {:error, reason}
    end
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

  defp div_ceil(a, b) when is_integer(a) and is_integer(b) and b > 0 do
    div(a + b - 1, b)
  end

  defp format_local_error({:frame_sprite_alloc_failed, w, h, split_factor, reason}) do
    "frame sprite alloc failed w=#{w} h=#{h} split_factor=#{split_factor} reason=#{Port.format_error(reason)}"
  end

  defp format_local_error(reason), do: inspect(reason)
end
