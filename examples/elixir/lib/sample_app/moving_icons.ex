defmodule SampleApp.MovingIcons do
  @moduledoc false

  alias SampleApp.Port
  alias SampleApp.Assets
  import SampleApp.AtomVMCompat, only: [yield: 0]

  # Bench knob
  @obj_count 5

  # HUD
  @hud_h 32
  @hud_bg 0x101010
  @hud_fg 0xFFFFFF

  # Playfield background (below HUD)
  @bg 0x000000

  # -----------------------------------------------------------------------------
  # Public entry
  # -----------------------------------------------------------------------------

  # Expects display already initialized and rotated, with normalized logical dims.
  def run(port, w, h)
      when is_integer(w) and w > 0 and is_integer(h) and h > 0 do
    icon_w = Assets.icon_w()
    icon_h = Assets.icon_h()

    icons = {Assets.icon(:info), Assets.icon(:alert), Assets.icon(:close)}
    log_icon_sizes(icons, icon_w, icon_h)

    max_x = max(0, w - icon_w)
    min_y = @hud_h
    max_y = max(min_y, h - icon_h)

    case Port.fill_screen(port, @bg) do
      :ok ->
        draw_hud!(port, w, @obj_count, 0, 0, 0, 0)
        {_seed, objects} = init_objects(1, @obj_count, max_x, min_y, max_y)

        # State tuple:
        # {w, h, max_x, min_y, max_y, icon_w, icon_h, icons, objects, fps, frame_count, prev_sec, hud_sec}
        state = {w, h, max_x, min_y, max_y, icon_w, icon_h, icons, objects, 0, 0, nil, nil}

        loop(port, state)

      {:error, reason} ->
        IO.puts("fill_screen failed: #{Port.format_error(reason)}")
        {:error, reason}
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

  # objects are tuples: {x, y, dx, dy, img_index}
  defp init_objects(seed, count, max_x, min_y, max_y) do
    init_objects_i(seed, count, max_x, min_y, max_y, [])
  end

  defp init_objects_i(seed, 0, _max_x, _min_y, _max_y, acc),
    do: {seed, :lists.reverse(acc)}

  defp init_objects_i(seed, n, max_x, min_y, max_y, acc) do
    {seed, r1} = rand_u32(seed)
    {seed, r2} = rand_u32(seed)
    {seed, r3} = rand_u32(seed)
    {seed, r4} = rand_u32(seed)

    x = rem(r1, max_x + 1)
    y = min_y + rem(r2, max(1, max_y - min_y + 1))

    base_dx = rem(r3, 4) + 1
    base_dy = rem(r4, 4) + 1

    dx = if rem(r1, 2) == 0, do: base_dx, else: -base_dx
    dy = if rem(r2, 2) == 0, do: base_dy, else: -base_dy

    img_index = rem(n, 3)

    init_objects_i(seed, n - 1, max_x, min_y, max_y, [{x, y, dx, dy, img_index} | acc])
  end

  # -----------------------------------------------------------------------------
  # Main loop
  # -----------------------------------------------------------------------------

  defp loop(port, state) do
    now_ms = :erlang.monotonic_time(:millisecond)
    sec = div(now_ms, 1000)

    {w, _h, _max_x, _min_y, _max_y, _iw, _ih, _icons, _objects, fps0, frame_count0, prev_sec0,
     hud_sec0} = state

    {fps, frame_count, prev_sec} = tick_fps(sec, fps0, frame_count0, prev_sec0)

    {objects_next, clr_ms, spr_ms, draw_ms} = render_frame(port, state)

    hud_sec =
      if hud_sec0 != sec do
        draw_hud!(port, w, @obj_count, fps, draw_ms, clr_ms, spr_ms)
        sec
      else
        hud_sec0
      end

    yield()

    {w1, h1, max_x1, min_y1, max_y1, iw1, ih1, icons1, _objects1, _fps1, _fc1, _ps1, _hs1} =
      state

    loop(
      port,
      {w1, h1, max_x1, min_y1, max_y1, iw1, ih1, icons1, objects_next, fps, frame_count, prev_sec,
       hud_sec}
    )
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

  # -----------------------------------------------------------------------------
  # Render (single-pass: clear old, move, draw new)
  # -----------------------------------------------------------------------------

  defp render_frame(port, state) do
    {_w, _h, max_x, min_y, max_y, icon_w, icon_h, icons, objects, _fps, _fc, _ps, _hs} = state

    t0 = :erlang.monotonic_time(:millisecond)

    {objects_rev, clr_ms, spr_ms} =
      render_objects_i(port, objects, max_x, min_y, max_y, icon_w, icon_h, icons, 0, 0, [])

    t1 = :erlang.monotonic_time(:millisecond)

    objects_next = :lists.reverse(objects_rev)
    draw_ms = t1 - t0

    {objects_next, clr_ms, spr_ms, draw_ms}
  end

  defp render_objects_i(
         _port,
         [],
         _max_x,
         _min_y,
         _max_y,
         _iw,
         _ih,
         _icons,
         clr_ms,
         spr_ms,
         acc
       ),
       do: {acc, clr_ms, spr_ms}

  defp render_objects_i(
         port,
         [{x, y, dx, dy, img_index} | rest],
         max_x,
         min_y,
         max_y,
         iw,
         ih,
         icons,
         clr_ms,
         spr_ms,
         acc
       ) do
    # Clear old (dirty rect)
    t0 = :erlang.monotonic_time(:millisecond)
    _ = Port.fill_rect(port, x, y, iw, ih, @bg)
    t1 = :erlang.monotonic_time(:millisecond)

    # Move (bounce within playfield)
    {x2, dx2} =
      cond do
        x + dx < 0 -> {0, abs(dx)}
        x + dx > max_x -> {max_x, -abs(dx)}
        true -> {x + dx, dx}
      end

    {y2, dy2} =
      cond do
        y + dy < min_y -> {min_y, abs(dy)}
        y + dy > max_y -> {max_y, -abs(dy)}
        true -> {y + dy, dy}
      end

    # Draw new (push RGB565 icon directly)
    icon = elem(icons, img_index)
    _ = Port.push_image_rgb565(port, x2, y2, iw, ih, icon)
    t2 = :erlang.monotonic_time(:millisecond)

    render_objects_i(
      port,
      rest,
      max_x,
      min_y,
      max_y,
      iw,
      ih,
      icons,
      clr_ms + (t1 - t0),
      spr_ms + (t2 - t1),
      [{x2, y2, dx2, dy2, img_index} | acc]
    )
  end

  # -----------------------------------------------------------------------------
  # HUD
  # -----------------------------------------------------------------------------

  defp draw_hud!(port, w, obj_count, fps, draw_ms, clr_ms, spr_ms) do
    _ = Port.fill_rect(port, 0, 0, w, @hud_h, @hud_bg)

    line1 =
      <<"obj:", :erlang.integer_to_binary(obj_count)::binary, " fps:",
        :erlang.integer_to_binary(fps)::binary, " draw:",
        :erlang.integer_to_binary(draw_ms)::binary>>

    line2 =
      <<"clr:", :erlang.integer_to_binary(clr_ms)::binary, " spr:",
        :erlang.integer_to_binary(spr_ms)::binary>>

    _ = Port.draw_string_bg(port, 4, 0, @hud_fg, @hud_bg, 2, line1)
    _ = Port.draw_string_bg(port, 4, 16, @hud_fg, @hud_bg, 2, line2)

    :ok
  end
end
