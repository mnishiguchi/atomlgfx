# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.Face do
  @moduledoc false

  import Bitwise
  import SampleApp.AtomVMCompat, only: [yield: 0]

  alias AtomLGFX.BinaryBatch, as: Batch

  # Keep the persistent graphics buffer small. This 4-bit sprite is the main
  # reason the demo can coexist with Wi-Fi on ESP32-S3 targets without PSRAM.
  #
  @canvas_width 320
  @canvas_height 240
  @sprite_depth 4
  @sprite_target 1

  @eye_r 8
  @eye_r_x 90
  @eye_r_y 93
  @eye_l_x 230
  @eye_l_y 96

  @brow_w 32
  @brow_h 0
  @brow_r_x 96
  @brow_r_y 67
  @brow_l_x 230
  @brow_l_y 72

  @mouth_min_w 50
  @mouth_max_w 90
  @mouth_min_h 4
  @mouth_max_h 60
  @mouth_x 163
  @mouth_y 148

  @palette_bg 0
  @palette_fg 1
  @palette_sweat 2
  @palette_heart 3

  @col_bg {:index, @palette_bg}
  @col_pr {:index, @palette_fg}
  @col_sweat {:index, @palette_sweat}
  @col_heart {:index, @palette_heart}

  @default_expression :neutral
  @expression_interval_ms 10_000

  @external_gaze_hold_ms 200
  @frame_interval_ms 33

  # Batch is the default because ordinary per-primitive handle calls made touch
  # response visibly slow. This batch is synchronous and draws into the sprite;
  # it is not retained native rendering.
  @renderer :batch
  @mouth_touch_open 0.7
  @touch_read_mode :screen
  @stats_interval_frames 60

  defstruct expr: @default_expression,
            expression_index: 0,
            last_expression_change_ms: 0,
            eye_open_l: 1.0,
            eye_open_r: 1.0,
            mouth_open: 0.0,
            gaze_h: 0.0,
            gaze_v: 0.0,
            breath: 0.0,
            breath_count: 0,
            eye_open: true,
            last_blink_ms: 0,
            blink_interval: 2_500,
            last_saccade_ms: 0,
            saccade_interval: 500,
            external_gaze: false,
            external_gaze_ms: 0,
            initialized: false,
            rand_seed: 1,
            display_width: @canvas_width,
            display_height: @canvas_height,
            push_x: 0,
            push_y: 0,
            frame_count: 0

  def new(opts \\ []) do
    now_ms = monotonic_ms()
    {seed1, blink_rand} = rand_mod(1, 20)
    {seed2, saccade_rand} = rand_mod(seed1, 20)

    display_width = Keyword.get(opts, :display_width, @canvas_width)
    display_height = Keyword.get(opts, :display_height, @canvas_height)

    {push_x, push_y} = centered_push_position(display_width, display_height)

    %__MODULE__{
      expr: @default_expression,
      expression_index: expression_index(@default_expression),
      last_expression_change_ms: now_ms,
      eye_open_l: 1.0,
      eye_open_r: 1.0,
      mouth_open: 0.0,
      gaze_h: 0.0,
      gaze_v: 0.0,
      breath: 0.0,
      breath_count: 0,
      eye_open: true,
      last_blink_ms: now_ms,
      blink_interval: 5_000 + 200 * blink_rand,
      last_saccade_ms: now_ms,
      saccade_interval: 1_200 + 100 * saccade_rand,
      external_gaze: false,
      external_gaze_ms: now_ms,
      initialized: false,
      rand_seed: seed2,
      display_width: display_width,
      display_height: display_height,
      push_x: push_x,
      push_y: push_y,
      frame_count: 0
    }
  end

  def put_display_size(%__MODULE__{} = face, display_width, display_height)
      when is_integer(display_width) and display_width > 0 and is_integer(display_height) and
             display_height > 0 do
    {push_x, push_y} = centered_push_position(display_width, display_height)

    %{
      face
      | display_width: display_width,
        display_height: display_height,
        push_x: push_x,
        push_y: push_y
    }
  end

  def run(handle, display_width, display_height)
      when is_integer(display_width) and display_width > 0 and is_integer(display_height) and
             display_height > 0 do
    face = new(display_width: display_width, display_height: display_height)

    with {:ok, initialized_face} <- init(face, handle) do
      touch_enabled = touch_enabled?(handle)

      try do
        loop(handle, initialized_face, touch_enabled)
      after
        cleanup(handle)
      end
    end
  end

  defp cleanup(handle) do
    _ = AtomLGFX.delete_sprite(handle, @sprite_target)
    :ok
  end

  defp loop(handle, face, touch_enabled) do
    frame_started_ms = monotonic_ms()

    # Touch is sampled once per frame. Responsiveness mostly comes from keeping
    # draw latency low, not from repeatedly polling the resistive touch controller.
    #
    {touched_face, touched?, touch_ms} = apply_touch_timed(face, handle, touch_enabled)

    next_face =
      touched_face
      |> maybe_rotate_expression(frame_started_ms)
      |> update(frame_started_ms)

    draw_started_ms = monotonic_ms()

    case draw(next_face, handle) do
      :ok ->
        draw_ms = max(monotonic_ms() - draw_started_ms, 0)

        counted_face = %{next_face | frame_count: next_face.frame_count + 1}
        maybe_log_frame_stats(counted_face, touch_ms, draw_ms, touched?)

        delayed_face = wait_for_next_frame(handle, counted_face, touch_enabled, frame_started_ms)
        loop(handle, delayed_face, touch_enabled)

      {:error, reason} = err ->
        IO.puts("face render failed: #{AtomLGFX.format_error(reason)}")
        err
    end
  end

  defp touch_enabled?(handle) do
    case AtomLGFX.supports_touch?(handle) do
      {:ok, true} -> true
      _other -> false
    end
  end

  defp apply_touch_timed(face, handle, touch_enabled) do
    touch_started_ms = monotonic_ms()
    {updated_face, touched?} = apply_touch(face, handle, touch_enabled)
    {updated_face, touched?, max(monotonic_ms() - touch_started_ms, 0)}
  end

  defp apply_touch(face, _handle, false), do: {set_mouth_open(face, 0.0), false}

  defp apply_touch(face, handle, true) do
    case read_touch(handle) do
      {:ok, {:screen, touch_x, touch_y}} ->
        gaze_h = normalize_touch_axis(touch_x, face.display_width)
        gaze_v = normalize_touch_axis(touch_y, face.display_height)

        updated_face =
          face
          |> set_gaze(gaze_h, gaze_v)
          |> set_mouth_open(@mouth_touch_open)

        {updated_face, true}

      {:ok, :none} ->
        {set_mouth_open(face, 0.0), false}

      {:error, _reason} ->
        {set_mouth_open(face, 0.0), false}
    end
  end

  defp read_touch(handle), do: read_touch_screen(handle)

  defp read_touch_screen(handle) do
    case AtomLGFX.get_touch(handle) do
      {:ok, {touch_x, touch_y, _size}} -> {:ok, {:screen, touch_x, touch_y}}
      {:ok, :none} -> {:ok, :none}
      {:error, reason} -> {:error, reason}
    end
  end

  defp normalize_touch_axis(position, size)
       when is_number(position) and is_integer(size) and size > 0 do
    center = size / 2.0
    clamp((position - center) / center, -1.0, 1.0)
  end

  # Keep a stable max frame rate and yield so networking/AtomVM tasks can run.
  # If drawing already exceeded the frame budget, this returns immediately.
  #
  defp wait_for_next_frame(_handle, face, _touch_enabled, frame_started_ms) do
    yield()
    sleep_until(frame_started_ms + @frame_interval_ms)
    face
  end

  defp sleep_until(deadline_ms) do
    remaining_ms = deadline_ms - monotonic_ms()

    if remaining_ms > 0 do
      sleep_ms(remaining_ms)
    end
  end

  defp sleep_ms(ms) when is_integer(ms) and ms > 0 do
    receive do
    after
      ms -> :ok
    end
  end

  defp maybe_log_frame_stats(face, touch_ms, draw_ms, touched?) do
    if rem(face.frame_count, @stats_interval_frames) == 0 do
      IO.puts(
        "face stats frame=#{face.frame_count} touch_ms=#{touch_ms} draw_ms=#{draw_ms} touched=#{inspect(touched?)} touch_mode=#{inspect(@touch_read_mode)}"
      )
    end

    :ok
  end

  def init(%__MODULE__{initialized: true} = face, _handle), do: {:ok, face}

  def init(%__MODULE__{} = face, handle) do
    with :ok <-
           AtomLGFX.create_sprite(
             handle,
             @canvas_width,
             @canvas_height,
             @sprite_depth,
             @sprite_target
           ),
         :ok <- AtomLGFX.create_palette(handle, @sprite_target),
         :ok <- AtomLGFX.set_palette_color(handle, @sprite_target, @palette_bg, 0x00FFF4B8),
         :ok <- AtomLGFX.set_palette_color(handle, @sprite_target, @palette_fg, 0x00000000),
         :ok <- AtomLGFX.set_palette_color(handle, @sprite_target, @palette_sweat, 0x000040FF),
         :ok <- AtomLGFX.set_palette_color(handle, @sprite_target, @palette_heart, 0x00FF4080),
         :ok <- AtomLGFX.fill_screen(handle, @col_bg, @sprite_target) do
      IO.puts("face config sprite=#{@canvas_width}x#{@canvas_height} depth=#{@sprite_depth}")
      {:ok, %{face | initialized: true}}
    end
  end

  def set_expression(%__MODULE__{} = face, expr)
      when expr in [:neutral, :happy, :angry, :sad, :doubt, :sleepy] do
    %{face | expr: expr, expression_index: expression_index(expr)}
  end

  def set_mouth_open(%__MODULE__{} = face, ratio) when is_number(ratio) do
    %{face | mouth_open: clamp(ratio * 1.0, 0.0, 1.0)}
  end

  def set_gaze(%__MODULE__{} = face, horizontal, vertical)
      when is_number(horizontal) and is_number(vertical) do
    now_ms = monotonic_ms()

    %{
      face
      | gaze_h: clamp(horizontal * 1.0, -1.0, 1.0),
        gaze_v: clamp(vertical * 1.0, -1.0, 1.0),
        external_gaze: true,
        external_gaze_ms: now_ms
    }
  end

  def update(%__MODULE__{} = face, now_ms) when is_integer(now_ms) do
    face
    |> update_breath()
    |> update_blink(now_ms)
    |> update_saccade(now_ms)
  end

  def draw(%__MODULE__{initialized: false} = face, handle) do
    with {:ok, initialized_face} <- init(face, handle) do
      draw(initialized_face, handle)
    end
  end

  def draw(%__MODULE__{} = face, handle) do
    case @renderer do
      :batch -> draw_batch(face, handle)
      :sync -> draw_sync(face, handle)
    end
  end

  # Fast path: build one bounded command stream for the face primitives, then
  # push the completed offscreen sprite once. This avoids visible flicker without
  # retained native renderer state.
  #
  defp draw_batch(%__MODULE__{} = face, handle) do
    breath_offset = min(1.0, face.breath)
    by = trunc(breath_offset * 3)

    commands = [
      Batch.target(@sprite_target),
      Batch.color_mode(:palette_index),
      Batch.fill_screen(@palette_bg),
      draw_mouth_commands(face, @mouth_x, @mouth_y + by),
      draw_eye_commands(face, @eye_r_x, @eye_r_y + by, face.eye_open_r, false),
      draw_eye_commands(face, @eye_l_x, @eye_l_y + by, face.eye_open_l, true),
      maybe_draw_eyebrows_commands(face, by),
      draw_effect_commands(face, breath_offset)
    ]

    with :ok <- Batch.render(handle, commands),
         :ok <- AtomLGFX.push_sprite(handle, @sprite_target, face.push_x, face.push_y) do
      :ok
    end
  end

  defp draw_eye_commands(face, x, y, open_ratio, is_left) do
    offset_x = trunc(face.gaze_h * 3)
    offset_y = trunc(face.gaze_v * 3)
    eye_x = x + offset_x
    eye_y = y + offset_y

    if open_ratio > 0.0 do
      [
        Batch.fill_circle(eye_x, eye_y, @eye_r, @palette_fg),
        maybe_apply_angry_sad_mask_commands(face, eye_x, eye_y, is_left),
        maybe_apply_happy_sleepy_mask_commands(face, eye_x, eye_y)
      ]
    else
      Batch.fill_rect(
        x - @eye_r + offset_x,
        y - 2 + offset_y,
        @eye_r * 2,
        4,
        @palette_fg
      )
    end
  end

  defp maybe_apply_angry_sad_mask_commands(face, eye_x, eye_y, is_left) do
    if face.expr in [:angry, :sad] do
      x0 = eye_x - @eye_r
      y0 = eye_y - @eye_r
      x1 = x0 + @eye_r * 2
      y1 = y0
      x2 = if(not is_left != not (face.expr == :sad), do: x0, else: x1)
      y2 = y0 + @eye_r

      Batch.fill_triangle(x0, y0, x1, y1, x2, y2, @palette_bg)
    else
      []
    end
  end

  defp maybe_apply_happy_sleepy_mask_commands(face, eye_x, eye_y) do
    if face.expr in [:happy, :sleepy] do
      rx = eye_x - @eye_r
      ry = eye_y - @eye_r
      rw = @eye_r * 2 + 4
      rh = @eye_r + 2

      [
        maybe_happy_eye_inner_circle_commands(face.expr, eye_x, eye_y),
        Batch.fill_rect(
          rx,
          if(face.expr == :happy, do: ry + @eye_r, else: ry),
          rw,
          rh,
          @palette_bg
        )
      ]
    else
      []
    end
  end

  defp maybe_happy_eye_inner_circle_commands(expr, _eye_x, _eye_y) when expr != :happy, do: []

  defp maybe_happy_eye_inner_circle_commands(:happy, eye_x, eye_y) do
    Batch.fill_circle(eye_x, eye_y, trunc(@eye_r / 1.5), @palette_bg)
  end

  defp draw_mouth_commands(face, cx, cy) do
    open_ratio = face.mouth_open
    h = @mouth_min_h + trunc((@mouth_max_h - @mouth_min_h) * open_ratio)
    w = @mouth_min_w + trunc((@mouth_max_w - @mouth_min_w) * (1.0 - open_ratio))
    x = cx - div(w, 2)
    y = cy - div(h, 2) + trunc(face.breath * 2)

    Batch.fill_rect(x, y, w, h, @palette_fg)
  end

  defp maybe_draw_eyebrows_commands(face, by) do
    if @brow_h <= 0 do
      []
    else
      [
        draw_eyebrow_commands(face, @brow_r_x, @brow_r_y + by, false),
        draw_eyebrow_commands(face, @brow_l_x, @brow_l_y + by, true)
      ]
    end
  end

  defp draw_eyebrow_commands(face, x, y, is_left) do
    if @brow_w == 0 or @brow_h == 0 do
      []
    else
      if face.expr in [:angry, :sad] do
        a = if(is_left != (face.expr == :sad), do: -1, else: 1)
        dx = a * 3
        dy = a * 5
        x1 = x - div(@brow_w, 2)
        x2 = x1 - dx
        x4 = x + div(@brow_w, 2)
        x3 = x4 + dx
        y1 = y - div(@brow_h, 2) - dy
        y2 = y + div(@brow_h, 2) - dy
        y3 = y - div(@brow_h, 2) + dy
        y4 = y + div(@brow_h, 2) + dy

        [
          Batch.fill_triangle(x1, y1, x2, y2, x3, y3, @palette_fg),
          Batch.fill_triangle(x2, y2, x3, y3, x4, y4, @palette_fg)
        ]
      else
        bx = x - div(@brow_w, 2)
        by = y - div(@brow_h, 2)

        Batch.fill_rect(
          bx,
          if(face.expr == :happy, do: by - 5, else: by),
          @brow_w,
          @brow_h,
          @palette_fg
        )
      end
    end
  end

  defp draw_effect_commands(face, offset) do
    case face.expr do
      :doubt ->
        draw_sweat_mark_commands(290, 110, 7, -offset)

      :angry ->
        draw_anger_mark_commands(280, 50, 12, offset)

      :happy ->
        draw_heart_mark_commands(280, 50, 12, offset)

      :sad ->
        draw_chill_mark_commands(270, 0, 30, offset)

      :sleepy ->
        [
          draw_bubble_mark_commands(290, 40, 10, offset),
          draw_bubble_mark_commands(270, 52, 6, -offset)
        ]

      :neutral ->
        []
    end
  end

  defp draw_sweat_mark_commands(x, y, r, offset) do
    y1 = y + trunc(5 * offset)
    r1 = r + trunc(r * 0.2 * offset)

    if r1 < 1 do
      []
    else
      a = trunc(:math.sqrt(3.0) * r1 / 2.0)

      [
        Batch.fill_circle(x, y1, r1, @palette_sweat),
        Batch.fill_triangle(
          x,
          y1 - r1 * 2,
          x - a,
          y1 - div(r1, 2),
          x + a,
          y1 - div(r1, 2),
          @palette_sweat
        )
      ]
    end
  end

  defp draw_anger_mark_commands(x, y, r, offset) do
    r1 = r + abs(trunc(r * 0.4 * offset))

    [
      Batch.fill_rect(x - div(r1, 3), y - r1, div(r1 * 2, 3), r1 * 2, @palette_fg),
      Batch.fill_rect(x - r1, y - div(r1, 3), r1 * 2, div(r1 * 2, 3), @palette_fg),
      maybe_fill_rect_commands(
        x - div(r1, 3) + 2,
        y - r1,
        max(div(r1 * 2, 3) - 4, 0),
        r1 * 2,
        @palette_bg
      ),
      maybe_fill_rect_commands(
        x - r1,
        y - div(r1, 3) + 2,
        r1 * 2,
        max(div(r1 * 2, 3) - 4, 0),
        @palette_bg
      )
    ]
  end

  defp draw_heart_mark_commands(x, y, r, offset) do
    r1 = r + trunc(r * 0.4 * offset)

    if r1 < 2 do
      []
    else
      a = :math.sqrt(2.0) * r1 / 4.0
      a_i = trunc(a)

      [
        Batch.fill_circle(x - div(r1, 2), y, div(r1, 2), @palette_heart),
        Batch.fill_circle(x + div(r1, 2), y, div(r1, 2), @palette_heart),
        Batch.fill_triangle(
          x,
          y,
          x - div(r1, 2) - a_i,
          y + a_i,
          x + div(r1, 2) + a_i,
          y + a_i,
          @palette_heart
        ),
        Batch.fill_triangle(
          x,
          y + div(r1, 2) + trunc(2 * a),
          x - div(r1, 2) - a_i,
          y + a_i,
          x + div(r1, 2) + a_i,
          y + a_i,
          @palette_heart
        )
      ]
    end
  end

  defp draw_chill_mark_commands(x, y, r, offset) do
    h = r + abs(trunc(r * 0.2 * offset))

    [
      Batch.fill_rect(x - div(r, 2), y, 3, div(h, 2), @palette_fg),
      Batch.fill_rect(x, y, 3, div(h * 3, 4), @palette_fg),
      Batch.fill_rect(x + div(r, 2), y, 3, h, @palette_fg)
    ]
  end

  defp draw_bubble_mark_commands(x, y, r, offset) do
    r1 = r + trunc(r * 0.2 * offset)
    inner_r = div(r1, 4)

    if r1 < 1 do
      []
    else
      [
        Batch.draw_circle(x, y, r1, @palette_fg),
        maybe_draw_circle_commands(x - div(r1, 4), y - div(r1, 4), inner_r, @palette_fg)
      ]
    end
  end

  defp maybe_fill_rect_commands(_x, _y, width, _height, _color) when width < 1, do: []
  defp maybe_fill_rect_commands(_x, _y, _width, height, _color) when height < 1, do: []

  defp maybe_fill_rect_commands(x, y, width, height, color),
    do: Batch.fill_rect(x, y, width, height, color)

  defp maybe_draw_circle_commands(_x, _y, radius, _color) when radius < 1, do: []
  defp maybe_draw_circle_commands(x, y, radius, color), do: Batch.draw_circle(x, y, radius, color)

  # Safe reference path: easiest to reason about, but slower because each
  # primitive crosses the AtomVM/native boundary separately.
  #
  defp draw_sync(%__MODULE__{} = face, handle) do
    breath_offset = min(1.0, face.breath)
    by = trunc(breath_offset * 3)

    with :ok <- AtomLGFX.fill_screen(handle, @col_bg, @sprite_target),
         :ok <- draw_mouth(handle, face, @mouth_x, @mouth_y + by),
         :ok <- draw_eye(handle, face, @eye_r_x, @eye_r_y + by, face.eye_open_r, false),
         :ok <- draw_eye(handle, face, @eye_l_x, @eye_l_y + by, face.eye_open_l, true),
         :ok <- maybe_draw_eyebrows(handle, face, by),
         :ok <- draw_effect(handle, face, breath_offset),
         :ok <- AtomLGFX.push_sprite(handle, @sprite_target, face.push_x, face.push_y) do
      :ok
    end
  end

  defp centered_push_position(display_width, display_height) do
    {
      center_offset(display_width, @canvas_width),
      center_offset(display_height, @canvas_height)
    }
  end

  defp center_offset(outer_size, inner_size)
       when is_integer(outer_size) and is_integer(inner_size) and outer_size > inner_size do
    div(outer_size - inner_size, 2)
  end

  defp center_offset(_outer_size, _inner_size), do: 0

  defp draw_eye(handle, face, x, y, open_ratio, is_left) do
    offset_x = trunc(face.gaze_h * 3)
    offset_y = trunc(face.gaze_v * 3)
    eye_x = x + offset_x
    eye_y = y + offset_y

    if open_ratio > 0.0 do
      with :ok <- AtomLGFX.fill_circle(handle, eye_x, eye_y, @eye_r, @col_pr, @sprite_target),
           :ok <- maybe_apply_angry_sad_mask(handle, face, eye_x, eye_y, is_left),
           :ok <- maybe_apply_happy_sleepy_mask(handle, face, eye_x, eye_y) do
        :ok
      end
    else
      AtomLGFX.fill_rect(
        handle,
        x - @eye_r + offset_x,
        y - 2 + offset_y,
        @eye_r * 2,
        4,
        @col_pr,
        @sprite_target
      )
    end
  end

  defp maybe_apply_angry_sad_mask(handle, face, eye_x, eye_y, is_left) do
    if face.expr in [:angry, :sad] do
      x0 = eye_x - @eye_r
      y0 = eye_y - @eye_r
      x1 = x0 + @eye_r * 2
      y1 = y0
      x2 = if(not is_left != not (face.expr == :sad), do: x0, else: x1)
      y2 = y0 + @eye_r

      AtomLGFX.fill_triangle(handle, x0, y0, x1, y1, x2, y2, @col_bg, @sprite_target)
    else
      :ok
    end
  end

  defp maybe_apply_happy_sleepy_mask(handle, face, eye_x, eye_y) do
    if face.expr in [:happy, :sleepy] do
      rx = eye_x - @eye_r
      ry = eye_y - @eye_r
      rw = @eye_r * 2 + 4
      rh = @eye_r + 2

      with :ok <- maybe_happy_eye_inner_circle(handle, face.expr, eye_x, eye_y),
           :ok <-
             AtomLGFX.fill_rect(
               handle,
               rx,
               if(face.expr == :happy, do: ry + @eye_r, else: ry),
               rw,
               rh,
               @col_bg,
               @sprite_target
             ) do
        :ok
      end
    else
      :ok
    end
  end

  defp maybe_happy_eye_inner_circle(_handle, expr, _eye_x, _eye_y) when expr != :happy,
    do: :ok

  defp maybe_happy_eye_inner_circle(handle, :happy, eye_x, eye_y) do
    AtomLGFX.fill_circle(handle, eye_x, eye_y, trunc(@eye_r / 1.5), @col_bg, @sprite_target)
  end

  defp draw_mouth(handle, face, cx, cy) do
    open_ratio = face.mouth_open
    h = @mouth_min_h + trunc((@mouth_max_h - @mouth_min_h) * open_ratio)
    w = @mouth_min_w + trunc((@mouth_max_w - @mouth_min_w) * (1.0 - open_ratio))
    x = cx - div(w, 2)
    y = cy - div(h, 2) + trunc(face.breath * 2)

    AtomLGFX.fill_rect(handle, x, y, w, h, @col_pr, @sprite_target)
  end

  defp maybe_draw_eyebrows(handle, face, by) do
    if @brow_h <= 0 do
      :ok
    else
      with :ok <- draw_eyebrow(handle, face, @brow_r_x, @brow_r_y + by, false),
           :ok <- draw_eyebrow(handle, face, @brow_l_x, @brow_l_y + by, true) do
        :ok
      end
    end
  end

  defp draw_eyebrow(handle, face, x, y, is_left) do
    if @brow_w == 0 or @brow_h == 0 do
      :ok
    else
      if face.expr in [:angry, :sad] do
        a = if(is_left != (face.expr == :sad), do: -1, else: 1)
        dx = a * 3
        dy = a * 5
        x1 = x - div(@brow_w, 2)
        x2 = x1 - dx
        x4 = x + div(@brow_w, 2)
        x3 = x4 + dx
        y1 = y - div(@brow_h, 2) - dy
        y2 = y + div(@brow_h, 2) - dy
        y3 = y - div(@brow_h, 2) + dy
        y4 = y + div(@brow_h, 2) + dy

        with :ok <-
               AtomLGFX.fill_triangle(handle, x1, y1, x2, y2, x3, y3, @col_pr, @sprite_target),
             :ok <-
               AtomLGFX.fill_triangle(handle, x2, y2, x3, y3, x4, y4, @col_pr, @sprite_target) do
          :ok
        end
      else
        bx = x - div(@brow_w, 2)
        by = y - div(@brow_h, 2)

        AtomLGFX.fill_rect(
          handle,
          bx,
          if(face.expr == :happy, do: by - 5, else: by),
          @brow_w,
          @brow_h,
          @col_pr,
          @sprite_target
        )
      end
    end
  end

  defp draw_effect(handle, face, offset) do
    case face.expr do
      :doubt ->
        draw_sweat_mark(handle, 290, 110, 7, -offset)

      :angry ->
        draw_anger_mark(handle, 280, 50, 12, offset)

      :happy ->
        draw_heart_mark(handle, 280, 50, 12, offset)

      :sad ->
        draw_chill_mark(handle, 270, 0, 30, offset)

      :sleepy ->
        with :ok <- draw_bubble_mark(handle, 290, 40, 10, offset),
             :ok <- draw_bubble_mark(handle, 270, 52, 6, -offset) do
          :ok
        end

      :neutral ->
        :ok
    end
  end

  defp draw_sweat_mark(handle, x, y, r, offset) do
    y1 = y + trunc(5 * offset)
    r1 = r + trunc(r * 0.2 * offset)

    if r1 < 1 do
      :ok
    else
      a = trunc(:math.sqrt(3.0) * r1 / 2.0)

      with :ok <- AtomLGFX.fill_circle(handle, x, y1, r1, @col_sweat, @sprite_target),
           :ok <-
             AtomLGFX.fill_triangle(
               handle,
               x,
               y1 - r1 * 2,
               x - a,
               y1 - div(r1, 2),
               x + a,
               y1 - div(r1, 2),
               @col_sweat,
               @sprite_target
             ) do
        :ok
      end
    end
  end

  defp draw_anger_mark(handle, x, y, r, offset) do
    r1 = r + abs(trunc(r * 0.4 * offset))

    with :ok <-
           AtomLGFX.fill_rect(
             handle,
             x - div(r1, 3),
             y - r1,
             div(r1 * 2, 3),
             r1 * 2,
             @col_pr,
             @sprite_target
           ),
         :ok <-
           AtomLGFX.fill_rect(
             handle,
             x - r1,
             y - div(r1, 3),
             r1 * 2,
             div(r1 * 2, 3),
             @col_pr,
             @sprite_target
           ),
         :ok <-
           maybe_fill_rect(
             handle,
             x - div(r1, 3) + 2,
             y - r1,
             max(div(r1 * 2, 3) - 4, 0),
             r1 * 2,
             @col_bg
           ),
         :ok <-
           maybe_fill_rect(
             handle,
             x - r1,
             y - div(r1, 3) + 2,
             r1 * 2,
             max(div(r1 * 2, 3) - 4, 0),
             @col_bg
           ) do
      :ok
    end
  end

  defp draw_heart_mark(handle, x, y, r, offset) do
    r1 = r + trunc(r * 0.4 * offset)

    if r1 < 2 do
      :ok
    else
      a = :math.sqrt(2.0) * r1 / 4.0
      a_i = trunc(a)

      with :ok <-
             AtomLGFX.fill_circle(
               handle,
               x - div(r1, 2),
               y,
               div(r1, 2),
               @col_heart,
               @sprite_target
             ),
           :ok <-
             AtomLGFX.fill_circle(
               handle,
               x + div(r1, 2),
               y,
               div(r1, 2),
               @col_heart,
               @sprite_target
             ),
           :ok <-
             AtomLGFX.fill_triangle(
               handle,
               x,
               y,
               x - div(r1, 2) - a_i,
               y + a_i,
               x + div(r1, 2) + a_i,
               y + a_i,
               @col_heart,
               @sprite_target
             ),
           :ok <-
             AtomLGFX.fill_triangle(
               handle,
               x,
               y + div(r1, 2) + trunc(2 * a),
               x - div(r1, 2) - a_i,
               y + a_i,
               x + div(r1, 2) + a_i,
               y + a_i,
               @col_heart,
               @sprite_target
             ) do
        :ok
      end
    end
  end

  defp draw_chill_mark(handle, x, y, r, offset) do
    h = r + abs(trunc(r * 0.2 * offset))

    with :ok <-
           AtomLGFX.fill_rect(handle, x - div(r, 2), y, 3, div(h, 2), @col_pr, @sprite_target),
         :ok <- AtomLGFX.fill_rect(handle, x, y, 3, div(h * 3, 4), @col_pr, @sprite_target),
         :ok <- AtomLGFX.fill_rect(handle, x + div(r, 2), y, 3, h, @col_pr, @sprite_target) do
      :ok
    end
  end

  defp draw_bubble_mark(handle, x, y, r, offset) do
    r1 = r + trunc(r * 0.2 * offset)

    if r1 < 1 do
      :ok
    else
      with :ok <- AtomLGFX.draw_circle(handle, x, y, r1, @col_pr, @sprite_target),
           :ok <-
             AtomLGFX.draw_circle(
               handle,
               x - div(r1, 4),
               y - div(r1, 4),
               div(r1, 4),
               @col_pr,
               @sprite_target
             ) do
        :ok
      end
    end
  end

  defp maybe_fill_rect(_handle, _x, _y, width, _height, _color) when width < 1, do: :ok
  defp maybe_fill_rect(_handle, _x, _y, _width, height, _color) when height < 1, do: :ok

  defp maybe_fill_rect(handle, x, y, width, height, color) do
    AtomLGFX.fill_rect(handle, x, y, width, height, color, @sprite_target)
  end

  defp maybe_rotate_expression(%__MODULE__{} = face, now_ms) do
    if now_ms - face.last_expression_change_ms > @expression_interval_ms do
      next_index = next_expression_index(face.expression_index)
      next_expression = expression_by_index(next_index)

      IO.puts("Expression: #{expression_name(next_expression)}")

      %{
        face
        | expr: next_expression,
          expression_index: next_index,
          last_expression_change_ms: now_ms
      }
    else
      face
    end
  end

  defp next_expression_index(0), do: 1
  defp next_expression_index(1), do: 2
  defp next_expression_index(2), do: 3
  defp next_expression_index(3), do: 4
  defp next_expression_index(4), do: 5
  defp next_expression_index(_index), do: 0

  defp expression_by_index(0), do: :neutral
  defp expression_by_index(1), do: :happy
  defp expression_by_index(2), do: :angry
  defp expression_by_index(3), do: :sad
  defp expression_by_index(4), do: :doubt
  defp expression_by_index(_index), do: :sleepy

  defp expression_index(:neutral), do: 0
  defp expression_index(:happy), do: 1
  defp expression_index(:angry), do: 2
  defp expression_index(:sad), do: 3
  defp expression_index(:doubt), do: 4
  defp expression_index(:sleepy), do: 5
  defp expression_index(_expression), do: 0

  defp expression_name(:neutral), do: "Neutral"
  defp expression_name(:happy), do: "Happy"
  defp expression_name(:angry), do: "Angry"
  defp expression_name(:sad), do: "Sad"
  defp expression_name(:doubt), do: "Doubt"
  defp expression_name(:sleepy), do: "Sleepy"

  defp update_breath(face) do
    breath_count = rem(face.breath_count + 1, 100)
    breath = :math.sin(breath_count * 2.0 * :math.pi() / 100.0)
    %{face | breath_count: breath_count, breath: breath}
  end

  defp update_blink(face, now_ms) do
    if now_ms - face.last_blink_ms > face.blink_interval do
      {next_seed, interval_rand} = rand_mod(face.rand_seed, 20)

      if face.eye_open do
        %{
          face
          | eye_open_l: 0.0,
            eye_open_r: 0.0,
            blink_interval: 120 + 10 * interval_rand,
            eye_open: false,
            last_blink_ms: now_ms,
            rand_seed: next_seed
        }
      else
        %{
          face
          | eye_open_l: 1.0,
            eye_open_r: 1.0,
            blink_interval: 5_000 + 200 * interval_rand,
            eye_open: true,
            last_blink_ms: now_ms,
            rand_seed: next_seed
        }
      end
    else
      face
    end
  end

  defp update_saccade(face, now_ms) do
    face =
      if face.external_gaze do
        if now_ms - face.external_gaze_ms < @external_gaze_hold_ms do
          face
        else
          %{face | external_gaze: false}
        end
      else
        face
      end

    if face.external_gaze do
      face
    else
      if now_ms - face.last_saccade_ms > face.saccade_interval do
        {seed1, gaze_v_rand} = rand_mod(face.rand_seed, 200)
        {seed2, gaze_h_rand} = rand_mod(seed1, 200)
        {seed3, interval_rand} = rand_mod(seed2, 20)

        %{
          face
          | gaze_v: gaze_v_rand / 100.0 - 1.0,
            gaze_h: gaze_h_rand / 100.0 - 1.0,
            saccade_interval: 1_200 + 100 * interval_rand,
            last_saccade_ms: now_ms,
            rand_seed: seed3
        }
      else
        face
      end
    end
  end

  defp rand_mod(seed, modulus) when is_integer(seed) and is_integer(modulus) and modulus > 0 do
    next_seed = lcg_next(seed)
    {next_seed, rem(next_seed, modulus)}
  end

  defp lcg_next(seed) do
    seed * 1_103_515_245 + 12_345 &&& 0x7FFFFFFF
  end

  defp monotonic_ms do
    :erlang.monotonic_time(:millisecond)
  end

  defp clamp(value, min_value, _max_value) when value < min_value, do: min_value
  defp clamp(value, _min_value, max_value) when value > max_value, do: max_value
  defp clamp(value, _min_value, _max_value), do: value
end
