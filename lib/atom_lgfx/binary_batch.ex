# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatch do
  @moduledoc """
  Packed binary-batch command builders.

  The binary-batch path uses scalar command encodings plus render-only control
  commands such as `target/1`, `color_mode/1`, `display/0`, `set_text_datum/1`, `set_text_wrap/1`,
  `set_text_wrap_xy/2`, `set_cursor/2`, `set_text_color/2`, `draw_string/3`, `print/1`, `println/1`,
  `set_palette_color/2`, `set_palette_color/4`, `set_pivot/2`,
  `draw_pixel_list/1`, `draw_rect_list/1`, `fill_rect_list/1`, `draw_circle_list/1`,
  `fill_circle_list/1`, `draw_ellipse_list/1`, `fill_ellipse_list/1`, `draw_line_list/1`,
  `draw_triangle_list/1`, `fill_triangle_list/1`, `push_sprite/3`,
  `push_sprite/4`, `push_rotate_zoom/5`,
  `push_rotate_zoom/6`, `push_rotate_zoom/7`, `push_sprite_list/2`,
  `push_sprite_region_list/2`, `draw_jpg/3`, `draw_jpg/9`,
  `push_image_rgb565/5`, `push_image_rgb565/6`, `draw_arc/7`, `fill_arc/7`,
  `draw_bezier/7`, `draw_bezier/9`, and `push_rotate_zoom_list/2`. Submit
  binary batches with `render/2`. Use `summary/1`, `diagnose/1`,
  `compare/2`, and `check_budget/2` for Elixir-side diagnostics.
  """

  import Bitwise
  import AtomLGFX.Guards

  alias AtomLGFX.Errors
  alias AtomLGFX.OpSchema
  alias AtomLGFX.Protocol
  alias AtomLGFX.Sprites

  @op_fill_screen OpSchema.opcode!(:fill_screen)
  @op_clear OpSchema.opcode!(:clear)
  @op_draw_pixel OpSchema.opcode!(:draw_pixel)
  @op_draw_fast_vline OpSchema.opcode!(:draw_fast_vline)
  @op_draw_fast_hline OpSchema.opcode!(:draw_fast_hline)
  @op_draw_line OpSchema.opcode!(:draw_line)
  @op_draw_rect OpSchema.opcode!(:draw_rect)
  @op_fill_rect OpSchema.opcode!(:fill_rect)
  @op_draw_round_rect OpSchema.opcode!(:draw_round_rect)
  @op_fill_round_rect OpSchema.opcode!(:fill_round_rect)
  @op_draw_circle OpSchema.opcode!(:draw_circle)
  @op_fill_circle OpSchema.opcode!(:fill_circle)
  @op_draw_ellipse OpSchema.opcode!(:draw_ellipse)
  @op_fill_ellipse OpSchema.opcode!(:fill_ellipse)
  @op_draw_arc OpSchema.opcode!(:draw_arc)
  @op_fill_arc OpSchema.opcode!(:fill_arc)
  @op_draw_bezier OpSchema.opcode!(:draw_bezier)
  @op_draw_triangle OpSchema.opcode!(:draw_triangle)
  @op_fill_triangle OpSchema.opcode!(:fill_triangle)
  @op_set_text_size OpSchema.opcode!(:set_text_size)
  @op_set_text_datum OpSchema.opcode!(:set_text_datum)
  @op_set_text_wrap OpSchema.opcode!(:set_text_wrap)
  @op_set_text_font_preset OpSchema.opcode!(:set_text_font_preset)
  @op_set_text_color OpSchema.opcode!(:set_text_color)
  @op_set_cursor OpSchema.opcode!(:set_cursor)
  @op_draw_string OpSchema.opcode!(:draw_string)
  @op_print OpSchema.opcode!(:print)
  @op_println OpSchema.opcode!(:println)
  @op_draw_jpg OpSchema.opcode!(:draw_jpg)
  @op_set_clip_rect OpSchema.opcode!(:set_clip_rect)
  @op_clear_clip_rect OpSchema.opcode!(:clear_clip_rect)
  @op_display OpSchema.opcode!(:display)
  @op_push_image OpSchema.opcode!(:push_image)
  @op_set_palette_color OpSchema.opcode!(:set_palette_color)
  @op_set_pivot OpSchema.opcode!(:set_pivot)
  @op_push_sprite OpSchema.opcode!(:push_sprite)
  @op_push_rotate_zoom OpSchema.opcode!(:push_rotate_zoom)
  @op_push_rotate_zoom_list OpSchema.opcode!(:push_rotate_zoom_list)

  @render_op_target 0xF0
  @render_op_color_mode 0xF1
  @render_op_push_sprite_transparent 0xF2
  @render_op_push_sprite_list 0xF3
  @render_op_push_sprite_region_list 0xF4
  @render_op_begin_strip 0xF5
  @render_op_present_strip 0xF6
  @render_op_fill_rect_list 0xF7
  @render_op_draw_line_list 0xF8
  @render_op_draw_pixel_list 0xF9
  @render_op_draw_rect_list 0xFA
  @render_op_fill_circle_list 0xFB
  @render_op_draw_circle_list 0xFC
  @render_op_fill_triangle_list 0xFD
  @render_op_draw_triangle_list 0xFE
  @render_op_ellipse_list 0xFF

  @render_private_opcodes [
    target: @render_op_target,
    color_mode: @render_op_color_mode,
    push_sprite_transparent: @render_op_push_sprite_transparent,
    push_sprite_list: @render_op_push_sprite_list,
    push_sprite_region_list: @render_op_push_sprite_region_list,
    begin_strip: @render_op_begin_strip,
    present_strip: @render_op_present_strip,
    fill_rect_list: @render_op_fill_rect_list,
    draw_line_list: @render_op_draw_line_list,
    draw_pixel_list: @render_op_draw_pixel_list,
    draw_rect_list: @render_op_draw_rect_list,
    fill_circle_list: @render_op_fill_circle_list,
    draw_circle_list: @render_op_draw_circle_list,
    fill_triangle_list: @render_op_fill_triangle_list,
    draw_triangle_list: @render_op_draw_triangle_list,
    ellipse_list: @render_op_ellipse_list
  ]

  @render_private_opcode_values Keyword.values(@render_private_opcodes)

  @batch_scalar_opcodes [
    @op_display,
    @op_clear_clip_rect,
    @op_fill_screen,
    @op_clear,
    @op_draw_pixel,
    @op_draw_fast_vline,
    @op_draw_fast_hline,
    @op_draw_line,
    @op_draw_rect,
    @op_fill_rect,
    @op_draw_round_rect,
    @op_fill_round_rect,
    @op_draw_circle,
    @op_fill_circle,
    @op_draw_ellipse,
    @op_fill_ellipse,
    @op_draw_arc,
    @op_fill_arc,
    @op_draw_bezier,
    @op_draw_triangle,
    @op_fill_triangle,
    @op_set_clip_rect,
    @op_set_text_font_preset,
    @op_set_text_size,
    @op_set_text_datum,
    @op_set_text_wrap,
    @op_set_text_color,
    @op_set_cursor,
    @op_draw_string,
    @op_print,
    @op_println,
    @op_draw_jpg,
    @op_push_image,
    @op_set_palette_color,
    @op_set_pivot,
    @op_push_sprite,
    @op_push_rotate_zoom,
    @op_push_rotate_zoom_list
  ]

  @known_batch_opcodes @render_private_opcode_values ++ @batch_scalar_opcodes

  @render_color_mode_rgb565 0
  @render_color_mode_palette_index 1

  @font_preset_ascii 0
  @font_preset_jp 1

  @text_scale_factor 1024
  @max_text_scale_x1024 0xFFFF
  @max_f32 3.4028234663852886e38

  @sprite_list_flag_has_transparent 0x0001
  @sprite_region_list_flag_has_transparent 0x0001
  @sprite_list_record_size 6
  @sprite_region_list_record_size 14
  @fill_rect_list_record_size 10
  @draw_line_list_record_size 10
  @draw_pixel_list_record_size 6
  @draw_rect_list_record_size 10
  @fill_circle_list_record_size 8
  @draw_circle_list_record_size 8
  @ellipse_list_kind_draw 0
  @ellipse_list_kind_fill 1
  @ellipse_list_record_size 10
  @fill_triangle_list_record_size 14
  @draw_triangle_list_record_size 14
  @push_rotate_zoom_list_record_size 12

  @doc false
  @spec __render_private_opcodes__() :: [{atom(), byte()}]
  def __render_private_opcodes__ do
    @render_private_opcodes
  end

  @doc false
  @spec __known_batch_opcodes__() :: [byte()]
  def __known_batch_opcodes__ do
    @known_batch_opcodes
  end

  @doc """
  Combines packed binary command fragments into one command stream.

  Prebuilt binaries are returned directly so callers that already built the
  command stream do not pay another conversion cost on the render hot path.
  """
  @spec batch(iodata()) :: binary()
  def batch(commands) when is_binary(commands) do
    commands
  end

  def batch(commands) do
    :erlang.iolist_to_binary(commands)
  end

  @doc """
  Submits a binary batch command stream.

  Returns `:ok` on success or `{:error, reason}` on failure.
  """
  @spec render(port(), iodata()) :: :ok | {:error, term()}
  def render(port, commands) do
    command_binary = batch(commands)

    case AtomLGFX.submit_binary_batch(port, command_binary) do
      {:ok, :ok} -> :ok
      {:ok, other} -> {:error, {:unexpected_reply, other}}
      {:error, reason} -> {:error, reason}
    end
  end

  @doc """
  Validates and then submits a binary batch command stream.

  This is an opt-in safety path for generated or experimental frame scripts. It
  decodes the whole batch on the Elixir side before crossing the port boundary,
  so malformed batches are rejected before native rendering can partially mutate
  the display when native prevalidation is disabled.
  """
  @spec render_checked(port(), iodata()) :: :ok | {:error, term()}
  def render_checked(port, commands) do
    command_binary = batch(commands)

    with :ok <- validate(command_binary) do
      render(port, command_binary)
    end
  end

  @doc """
  Validates a binary-batch command stream without submitting it.

  This performs the same Elixir-side structural decode used by `decode/1`, but
  returns only `:ok` or `{:error, reason}`. Use this for tests, generated-frame
  guardrails, and optional preflight checks around risky frame construction.
  """
  @spec validate(iodata()) :: :ok | {:error, term()}
  def validate(commands) do
    case decode(commands) do
      {:ok, _decoded_commands} -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

  @doc """
  Validates a binary-batch command stream or raises `ArgumentError`.
  """
  @spec validate!(iodata()) :: :ok
  def validate!(commands) do
    case validate(commands) do
      :ok -> :ok
      {:error, reason} -> raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Selects the current render target for following binary-batch commands.
  """
  @spec target(integer()) :: binary()
  def target(target) when target_any(target) do
    <<@render_op_target, target>>
  end

  @doc """
  Selects how following scalar color fields are interpreted.
  """
  @spec color_mode(:rgb565 | :palette_index) :: binary()
  def color_mode(:rgb565) do
    <<@render_op_color_mode, @render_color_mode_rgb565>>
  end

  def color_mode(:palette_index) do
    <<@render_op_color_mode, @render_color_mode_palette_index>>
  end

  @doc """
  Begins drawing into the native presentation strip at LCD y-coordinate `y0`.

  While a strip is active, render target `0` resolves to the native strip buffer
  instead of the live LCD. Finish the strip with `present_strip/0`.
  """
  @spec begin_strip(non_neg_integer()) :: binary()
  def begin_strip(y0) when u16(y0) do
    <<@render_op_begin_strip, y0::little-16>>
  end

  @doc """
  Presents the active native presentation strip to the live LCD.
  """
  @spec present_strip() :: binary()
  def present_strip do
    <<@render_op_present_strip>>
  end

  @doc """
  Presents the current frame.
  """
  @spec display() :: binary()
  def display do
    <<@op_display>>
  end

  @spec fill_screen(integer()) :: binary()
  def fill_screen(color) when u16(color) do
    <<@op_fill_screen, color::little-16>>
  end

  @spec clear(integer()) :: binary()
  def clear(color) when u16(color) do
    <<@op_clear, color::little-16>>
  end

  @spec draw_pixel(integer(), integer(), integer()) :: binary()
  def draw_pixel(x, y, color) when i16(x) and i16(y) and u16(color) do
    <<@op_draw_pixel, x::signed-little-16, y::signed-little-16, color::little-16>>
  end

  @spec draw_fast_vline(integer(), integer(), integer(), integer()) :: binary()
  def draw_fast_vline(x, y, height, color)
      when i16(x) and i16(y) and u16(height) and height >= 1 and u16(color) do
    <<@op_draw_fast_vline, x::signed-little-16, y::signed-little-16, height::little-16,
      color::little-16>>
  end

  @spec draw_fast_hline(integer(), integer(), integer(), integer()) :: binary()
  def draw_fast_hline(x, y, width, color)
      when i16(x) and i16(y) and u16(width) and width >= 1 and u16(color) do
    <<@op_draw_fast_hline, x::signed-little-16, y::signed-little-16, width::little-16,
      color::little-16>>
  end

  @spec draw_line(integer(), integer(), integer(), integer(), integer()) :: binary()
  def draw_line(x0, y0, x1, y1, color)
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and u16(color) do
    <<@op_draw_line, x0::signed-little-16, y0::signed-little-16, x1::signed-little-16,
      y1::signed-little-16, color::little-16>>
  end

  @spec draw_rect(integer(), integer(), integer(), integer(), integer()) :: binary()
  def draw_rect(x, y, width, height, color)
      when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 and
             u16(color) do
    <<@op_draw_rect, x::signed-little-16, y::signed-little-16, width::little-16,
      height::little-16, color::little-16>>
  end

  @spec fill_rect(integer(), integer(), integer(), integer(), integer()) :: binary()
  def fill_rect(x, y, width, height, color)
      when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 and
             u16(color) do
    <<@op_fill_rect, x::signed-little-16, y::signed-little-16, width::little-16,
      height::little-16, color::little-16>>
  end

  @doc """
  Draws many pixels on the current render target with compact records.

  `pixels` is a list of fixed-width tuples:

      {x, y, color}

  The current `color_mode/1` controls whether `color` is interpreted as RGB565
  or as a palette index, matching `draw_pixel/3`.
  """
  @spec draw_pixel_list(list()) :: binary()
  def draw_pixel_list(pixels) when is_list(pixels) do
    case encode_draw_pixel_list_payload(pixels) do
      {:ok, count, payload} ->
        <<@render_op_draw_pixel_list, 0::little-16, count::little-16, payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Draws many rectangle outlines on the current render target with compact records.

  `rectangles` is a list of fixed-width tuples:

      {x, y, width, height, color}

  The current `color_mode/1` controls whether `color` is interpreted as RGB565
  or as a palette index, matching `draw_rect/5`.
  """
  @spec draw_rect_list(list()) :: binary()
  def draw_rect_list(rectangles) when is_list(rectangles) do
    case encode_draw_rect_list_payload(rectangles) do
      {:ok, count, payload} ->
        <<@render_op_draw_rect_list, 0::little-16, count::little-16, payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Fills many rectangles on the current render target with compact records.

  `rectangles` is a list of fixed-width tuples:

      {x, y, width, height, color}

  The current `color_mode/1` controls whether `color` is interpreted as RGB565
  or as a palette index, matching `fill_rect/5`.
  """
  @spec fill_rect_list(list()) :: binary()
  def fill_rect_list(rectangles) when is_list(rectangles) do
    case encode_fill_rect_list_payload(rectangles) do
      {:ok, count, payload} ->
        <<@render_op_fill_rect_list, 0::little-16, count::little-16, payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Draws many circle outlines on the current render target with compact records.

  `circles` is a list of fixed-width tuples:

      {x, y, radius, color}

  The current `color_mode/1` controls whether `color` is interpreted as RGB565
  or as a palette index, matching `draw_circle/4`.
  """
  @spec draw_circle_list(list()) :: binary()
  def draw_circle_list(circles) when is_list(circles) do
    case encode_draw_circle_list_payload(circles) do
      {:ok, count, payload} ->
        <<@render_op_draw_circle_list, 0::little-16, count::little-16, payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Fills many circles on the current render target with compact records.

  `circles` is a list of fixed-width tuples:

      {x, y, radius, color}

  The current `color_mode/1` controls whether `color` is interpreted as RGB565
  or as a palette index, matching `fill_circle/4`.
  """
  @spec fill_circle_list(list()) :: binary()
  def fill_circle_list(circles) when is_list(circles) do
    case encode_fill_circle_list_payload(circles) do
      {:ok, count, payload} ->
        <<@render_op_fill_circle_list, 0::little-16, count::little-16, payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Draws many ellipse outlines on the current render target with compact records.

  `ellipses` is a list of fixed-width tuples:

      {x, y, radius_x, radius_y, color}

  The current `color_mode/1` controls whether `color` is interpreted as RGB565
  or as a palette index, matching `draw_ellipse/5`.
  """
  @spec draw_ellipse_list(list()) :: binary()
  def draw_ellipse_list(ellipses) when is_list(ellipses) do
    case encode_ellipse_list_payload(:draw_ellipse_list, ellipses) do
      {:ok, count, payload} ->
        <<@render_op_ellipse_list, @ellipse_list_kind_draw, 0, 0::little-16, count::little-16,
          payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Fills many ellipses on the current render target with compact records.

  `ellipses` is a list of fixed-width tuples:

      {x, y, radius_x, radius_y, color}

  The current `color_mode/1` controls whether `color` is interpreted as RGB565
  or as a palette index, matching `fill_ellipse/5`.
  """
  @spec fill_ellipse_list(list()) :: binary()
  def fill_ellipse_list(ellipses) when is_list(ellipses) do
    case encode_ellipse_list_payload(:fill_ellipse_list, ellipses) do
      {:ok, count, payload} ->
        <<@render_op_ellipse_list, @ellipse_list_kind_fill, 0, 0::little-16, count::little-16,
          payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Draws many lines on the current render target with compact records.

  `lines` is a list of fixed-width tuples:

      {x0, y0, x1, y1, color}

  The current `color_mode/1` controls whether `color` is interpreted as RGB565
  or as a palette index, matching `draw_line/5`.
  """
  @spec draw_line_list(list()) :: binary()
  def draw_line_list(lines) when is_list(lines) do
    case encode_draw_line_list_payload(lines) do
      {:ok, count, payload} ->
        <<@render_op_draw_line_list, 0::little-16, count::little-16, payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Draws many triangle outlines on the current render target with compact records.

  `triangles` is a list of fixed-width tuples:

      {x0, y0, x1, y1, x2, y2, color}

  The current `color_mode/1` controls whether `color` is interpreted as RGB565
  or as a palette index, matching `draw_triangle/7`.
  """
  @spec draw_triangle_list(list()) :: binary()
  def draw_triangle_list(triangles) when is_list(triangles) do
    case encode_draw_triangle_list_payload(triangles) do
      {:ok, count, payload} ->
        <<@render_op_draw_triangle_list, 0::little-16, count::little-16, payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Fills many triangles on the current render target with compact records.

  `triangles` is a list of fixed-width tuples:

      {x0, y0, x1, y1, x2, y2, color}

  The current `color_mode/1` controls whether `color` is interpreted as RGB565
  or as a palette index, matching `fill_triangle/7`.
  """
  @spec fill_triangle_list(list()) :: binary()
  def fill_triangle_list(triangles) when is_list(triangles) do
    case encode_fill_triangle_list_payload(triangles) do
      {:ok, count, payload} ->
        <<@render_op_fill_triangle_list, 0::little-16, count::little-16, payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @spec draw_round_rect(integer(), integer(), integer(), integer(), integer(), integer()) ::
          binary()
  def draw_round_rect(x, y, width, height, radius, color)
      when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 and
             u16(radius) and radius >= 1 and u16(color) do
    <<@op_draw_round_rect, x::signed-little-16, y::signed-little-16, width::little-16,
      height::little-16, radius::little-16, color::little-16>>
  end

  @spec fill_round_rect(integer(), integer(), integer(), integer(), integer(), integer()) ::
          binary()
  def fill_round_rect(x, y, width, height, radius, color)
      when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 and
             u16(radius) and radius >= 1 and u16(color) do
    <<@op_fill_round_rect, x::signed-little-16, y::signed-little-16, width::little-16,
      height::little-16, radius::little-16, color::little-16>>
  end

  @spec draw_circle(integer(), integer(), integer(), integer()) :: binary()
  def draw_circle(x, y, radius, color)
      when i16(x) and i16(y) and u16(radius) and radius >= 1 and u16(color) do
    <<@op_draw_circle, x::signed-little-16, y::signed-little-16, radius::little-16,
      color::little-16>>
  end

  @spec fill_circle(integer(), integer(), integer(), integer()) :: binary()
  def fill_circle(x, y, radius, color)
      when i16(x) and i16(y) and u16(radius) and radius >= 1 and u16(color) do
    <<@op_fill_circle, x::signed-little-16, y::signed-little-16, radius::little-16,
      color::little-16>>
  end

  @spec draw_ellipse(integer(), integer(), integer(), integer(), integer()) :: binary()
  def draw_ellipse(x, y, radius_x, radius_y, color)
      when i16(x) and i16(y) and u16(radius_x) and radius_x >= 1 and u16(radius_y) and
             radius_y >= 1 and u16(color) do
    <<@op_draw_ellipse, x::signed-little-16, y::signed-little-16, radius_x::little-16,
      radius_y::little-16, color::little-16>>
  end

  @spec fill_ellipse(integer(), integer(), integer(), integer(), integer()) :: binary()
  def fill_ellipse(x, y, radius_x, radius_y, color)
      when i16(x) and i16(y) and u16(radius_x) and radius_x >= 1 and u16(radius_y) and
             radius_y >= 1 and u16(color) do
    <<@op_fill_ellipse, x::signed-little-16, y::signed-little-16, radius_x::little-16,
      radius_y::little-16, color::little-16>>
  end

  @spec draw_arc(integer(), integer(), integer(), integer(), number(), number(), integer()) ::
          binary()
  def draw_arc(x, y, radius0, radius1, angle0, angle1, color)
      when i16(x) and i16(y) and u16(radius0) and radius0 >= 1 and u16(radius1) and
             radius1 >= 1 and u16(color) do
    encode_arc_command(@op_draw_arc, x, y, radius0, radius1, angle0, angle1, color)
  end

  @spec fill_arc(integer(), integer(), integer(), integer(), number(), number(), integer()) ::
          binary()
  def fill_arc(x, y, radius0, radius1, angle0, angle1, color)
      when i16(x) and i16(y) and u16(radius0) and radius0 >= 1 and u16(radius1) and
             radius1 >= 1 and u16(color) do
    encode_arc_command(@op_fill_arc, x, y, radius0, radius1, angle0, angle1, color)
  end

  @spec draw_bezier(
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer()
        ) :: binary()
  def draw_bezier(x0, y0, x1, y1, x2, y2, color)
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and i16(x2) and i16(y2) and
             u16(color) do
    <<@op_draw_bezier, 3, 0, x0::signed-little-16, y0::signed-little-16, x1::signed-little-16,
      y1::signed-little-16, x2::signed-little-16, y2::signed-little-16, color::little-16>>
  end

  @spec draw_bezier(
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer()
        ) :: binary()
  def draw_bezier(x0, y0, x1, y1, x2, y2, x3, y3, color)
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and i16(x2) and i16(y2) and
             i16(x3) and i16(y3) and u16(color) do
    <<@op_draw_bezier, 4, 0, x0::signed-little-16, y0::signed-little-16, x1::signed-little-16,
      y1::signed-little-16, x2::signed-little-16, y2::signed-little-16, x3::signed-little-16,
      y3::signed-little-16, color::little-16>>
  end

  @spec draw_triangle(
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer()
        ) :: binary()
  def draw_triangle(x0, y0, x1, y1, x2, y2, color)
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and i16(x2) and i16(y2) and
             u16(color) do
    <<@op_draw_triangle, x0::signed-little-16, y0::signed-little-16, x1::signed-little-16,
      y1::signed-little-16, x2::signed-little-16, y2::signed-little-16, color::little-16>>
  end

  @spec fill_triangle(
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer()
        ) :: binary()
  def fill_triangle(x0, y0, x1, y1, x2, y2, color)
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and i16(x2) and i16(y2) and
             u16(color) do
    <<@op_fill_triangle, x0::signed-little-16, y0::signed-little-16, x1::signed-little-16,
      y1::signed-little-16, x2::signed-little-16, y2::signed-little-16, color::little-16>>
  end

  @doc """
  Sets a clip rectangle on the current render target.

  The clip state belongs to the current target, following LovyanGFX semantics.
  Use `clear_clip_rect/0` before leaving a batch section when later commands
  should not inherit the clip rectangle.
  """
  @spec set_clip_rect(integer(), integer(), integer(), integer()) :: binary()
  def set_clip_rect(x, y, width, height)
      when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 do
    <<@op_set_clip_rect, x::signed-little-16, y::signed-little-16, width::little-16,
      height::little-16>>
  end

  @doc """
  Clears the clip rectangle on the current render target.
  """
  @spec clear_clip_rect() :: binary()
  def clear_clip_rect do
    <<@op_clear_clip_rect>>
  end

  @doc """
  Selects a text font preset for the current render target.

  Supported presets are `:ascii` and `:jp`.
  """
  @spec set_text_font_preset(:ascii | :jp) :: binary()
  def set_text_font_preset(:ascii) do
    <<@op_set_text_font_preset, @font_preset_ascii>>
  end

  def set_text_font_preset(:jp) do
    <<@op_set_text_font_preset, @font_preset_jp>>
  end

  @doc """
  Sets text size for the current render target.

  Scale values use x1024 fixed-point encoding inside the render batch.
  """
  @spec set_text_size(number()) :: binary()
  def set_text_size(scale) when is_number(scale) and scale > 0 do
    set_text_size_xy(scale, scale)
  end

  @doc """
  Sets independent X/Y text size for the current render target.

  Scale values use x1024 fixed-point encoding inside the render batch.
  """
  @spec set_text_size_xy(number(), number()) :: binary()
  def set_text_size_xy(scale_x, scale_y)
      when is_number(scale_x) and scale_x > 0 and is_number(scale_y) and scale_y > 0 do
    with {:ok, scale_x1024} <- normalize_text_scale_x1024(scale_x),
         {:ok, scale_y1024} <- normalize_text_scale_x1024(scale_y) do
      <<@op_set_text_size, scale_x1024::little-16, scale_y1024::little-16>>
    else
      {:error, reason} -> raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Sets text datum for the current render target.
  """
  @spec set_text_datum(non_neg_integer()) :: binary()
  def set_text_datum(datum) when u8(datum) do
    <<@op_set_text_datum, datum>>
  end

  @doc """
  Sets text wrapping for the current render target.

  This follows the scalar `set_text_wrap/3` semantics: the single boolean controls
  X wrapping and leaves Y wrapping disabled.
  """
  @spec set_text_wrap(boolean()) :: binary()
  def set_text_wrap(wrap) when is_boolean(wrap) do
    set_text_wrap_xy(wrap, false)
  end

  @doc """
  Sets independent X/Y text wrapping for the current render target.
  """
  @spec set_text_wrap_xy(boolean(), boolean()) :: binary()
  def set_text_wrap_xy(wrap_x, wrap_y) when is_boolean(wrap_x) and is_boolean(wrap_y) do
    <<@op_set_text_wrap, encode_bool(wrap_x), encode_bool(wrap_y)>>
  end

  @doc """
  Sets text cursor position for the current render target.
  """
  @spec set_cursor(integer(), integer()) :: binary()
  def set_cursor(x, y) when i16(x) and i16(y) do
    <<@op_set_cursor, x::signed-little-16, y::signed-little-16>>
  end

  @doc """
  Sets text color for the current render target.

  Colors accept RGB565 integers, `{:rgb565, value}`, or `{:index, value}`.
  """
  @spec set_text_color(
          integer() | {:rgb565, integer()} | {:index, integer()},
          nil | integer() | {:rgb565, integer()} | {:index, integer()}
        ) :: binary()
  def set_text_color(fg_color, bg_color \\ nil) do
    case normalize_text_color_args(fg_color, bg_color) do
      {:ok, flags, [fg_arg]} ->
        <<@op_set_text_color, flags::little-16, fg_arg::little-16>>

      {:ok, flags, [fg_arg, bg_arg]} ->
        <<@op_set_text_color, flags::little-16, fg_arg::little-16, bg_arg::little-16>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Draws a UTF-8 string at `{x, y}` on the current render target.
  """
  @spec draw_string(integer(), integer(), binary()) :: binary()
  def draw_string(x, y, text) when i16(x) and i16(y) and is_binary(text) do
    case validate_render_text(text) do
      :ok ->
        text_len = byte_size(text)

        <<@op_draw_string, x::signed-little-16, y::signed-little-16, text_len::little-16,
          text::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Prints UTF-8 text at the current cursor position on the current render target.
  """
  @spec print(binary()) :: binary()
  def print(text) when is_binary(text) do
    encode_print_command(@op_print, :print, text)
  end

  @doc """
  Prints UTF-8 text plus a trailing newline at the current cursor position.
  """
  @spec println(binary()) :: binary()
  def println(text \\ "") when is_binary(text) do
    encode_print_command(@op_println, :println, text)
  end

  @doc """
  Draws JPEG bytes at `{x, y}` on the current render target.

  The JPEG payload is length-prefixed in the binary batch so following commands
  can be decoded without copying or scanning the image bytes.
  """
  @spec draw_jpg(integer(), integer(), binary()) :: binary()
  def draw_jpg(x, y, jpeg) when i16(x) and i16(y) and is_binary(jpeg) do
    case validate_jpeg_payload(jpeg) do
      {:ok, jpeg_len} ->
        <<@op_draw_jpg, 0, 0, x::signed-little-16, y::signed-little-16, jpeg_len::little-32,
          jpeg::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Draws JPEG bytes with clipping offset and independent X/Y scale.

  `max_width` or `max_height` may be zero to use LovyanGFX's default behavior,
  matching the scalar `AtomLGFX.draw_jpg/11` contract.
  """
  @spec draw_jpg(
          integer(),
          integer(),
          non_neg_integer(),
          non_neg_integer(),
          integer(),
          integer(),
          number(),
          number(),
          binary()
        ) :: binary()
  def draw_jpg(x, y, max_width, max_height, off_x, off_y, scale_x, scale_y, jpeg)
      when i16(x) and i16(y) and u16(max_width) and u16(max_height) and i16(off_x) and
             i16(off_y) and is_number(scale_x) and is_number(scale_y) and is_binary(jpeg) do
    with {:ok, scale_x} <- normalize_image_scale(scale_x),
         {:ok, scale_y} <- normalize_image_scale(scale_y),
         {:ok, jpeg_len} <- validate_jpeg_payload(jpeg) do
      <<@op_draw_jpg, 1, 0, x::signed-little-16, y::signed-little-16, max_width::little-16,
        max_height::little-16, off_x::signed-little-16, off_y::signed-little-16,
        scale_x::little-float-32, scale_y::little-float-32, jpeg_len::little-32, jpeg::binary>>
    else
      {:error, reason} -> raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Pushes RGB565 image pixels to the current render target.

  Pixel data must be little-endian RGB565 words. `stride_pixels` defaults to
  `width`; use a larger value when each source row has padding.
  """
  @spec push_image_rgb565(integer(), integer(), integer(), integer(), binary(), non_neg_integer()) ::
          binary()
  def push_image_rgb565(x, y, width, height, pixels, stride_pixels \\ 0)
      when i16(x) and i16(y) and u16(width) and u16(height) and is_binary(pixels) and
             u16(stride_pixels) do
    case validate_push_image_rgb565_payload(width, height, pixels, stride_pixels) do
      {:ok, pixels_len} ->
        <<@op_push_image, x::signed-little-16, y::signed-little-16, width::little-16,
          height::little-16, stride_pixels::little-16, pixels_len::little-32, pixels::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Sets one palette entry on the current sprite target.

  The current batch target must be a palette-backed sprite when this command is
  executed natively. `rgb888` uses the same `0x00RRGGBB` format as the scalar
  API.
  """
  @spec set_palette_color(non_neg_integer(), non_neg_integer()) :: binary()
  def set_palette_color(palette_index, rgb888) when u8(palette_index) and color888(rgb888) do
    <<@op_set_palette_color, palette_index, 0, rgb888::little-32>>
  end

  @doc """
  Sets one palette entry from RGB components on the current sprite target.
  """
  @spec set_palette_color(
          non_neg_integer(),
          non_neg_integer(),
          non_neg_integer(),
          non_neg_integer()
        ) ::
          binary()
  def set_palette_color(palette_index, red, green, blue)
      when u8(palette_index) and u8(red) and u8(green) and u8(blue) do
    rgb888 = red <<< 16 ||| green <<< 8 ||| blue
    set_palette_color(palette_index, rgb888)
  end

  @doc """
  Sets the pivot point on the current render target.

  This is useful before one-off or list-based transformed sprite pushes when the
  source sprite pivot is part of frame construction.
  """
  @spec set_pivot(integer(), integer()) :: binary()
  def set_pivot(x, y) when i16(x) and i16(y) do
    <<@op_set_pivot, x::signed-little-16, y::signed-little-16>>
  end

  @doc """
  Pushes a sprite to the current render target.
  """
  @spec push_sprite(integer(), integer(), integer()) :: binary()
  def push_sprite(source_target, x, y) when sprite_handle(source_target) and i16(x) and i16(y) do
    <<@op_push_sprite, source_target, x::signed-little-16, y::signed-little-16>>
  end

  @doc """
  Pushes a sprite to the current render target using a transparent key.

  The transparent key accepts an RGB565 integer, `{:rgb565, value}`, or
  `{:index, value}` for palette-backed source sprites.
  """
  @spec push_sprite(
          integer(),
          integer(),
          integer(),
          integer() | {:rgb565, integer()} | {:index, integer()}
        ) :: binary()
  def push_sprite(source_target, x, y, transparent)
      when sprite_handle(source_target) and i16(x) and i16(y) do
    case normalize_transparent_arg(transparent) do
      {:ok, flags, transparent_value} ->
        <<@render_op_push_sprite_transparent, flags::little-16, source_target,
          x::signed-little-16, y::signed-little-16, transparent_value::little-16>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Pushes a transformed source sprite to the current render target.

  This is the one-off BinaryBatch form of `pushRotateZoom`. Use
  `push_rotate_zoom_list/2` for many transformed sprites in the same frame.
  """
  @spec push_rotate_zoom(integer(), integer(), integer(), number(), number()) :: binary()
  def push_rotate_zoom(source_target, x, y, angle_deg, zoom)
      when sprite_handle(source_target) and i16(x) and i16(y) and is_number(angle_deg) and
             is_number(zoom) and zoom > 0 do
    push_rotate_zoom(source_target, x, y, angle_deg, zoom, zoom)
  end

  @doc """
  Pushes a transformed source sprite with independent X/Y zoom.
  """
  @spec push_rotate_zoom(integer(), integer(), integer(), number(), number(), number()) ::
          binary()
  def push_rotate_zoom(source_target, x, y, angle_deg, zoom_x, zoom_y)
      when sprite_handle(source_target) and i16(x) and i16(y) and is_number(angle_deg) and
             is_number(zoom_x) and zoom_x > 0 and is_number(zoom_y) and zoom_y > 0 do
    encode_push_rotate_zoom_command(source_target, x, y, angle_deg, zoom_x, zoom_y, nil)
  end

  @doc """
  Pushes a transformed source sprite using a transparent key.

  The transparent key accepts an RGB565 integer, `{:rgb565, value}`, or
  `{:index, value}` for palette-backed source sprites.
  """
  @spec push_rotate_zoom(
          integer(),
          integer(),
          integer(),
          number(),
          number(),
          number(),
          integer() | {:rgb565, integer()} | {:index, integer()}
        ) :: binary()
  def push_rotate_zoom(source_target, x, y, angle_deg, zoom_x, zoom_y, transparent)
      when sprite_handle(source_target) and i16(x) and i16(y) and is_number(angle_deg) and
             is_number(zoom_x) and zoom_x > 0 and is_number(zoom_y) and zoom_y > 0 do
    encode_push_rotate_zoom_command(source_target, x, y, angle_deg, zoom_x, zoom_y, transparent)
  end

  @doc """
  Pushes many whole source sprites to the current render target.

  `instances` is a list of fixed-width tuples:

      {source_target, x, y}

  Options:

  - `:transparent` accepts an RGB565 integer, `{:rgb565, n}`, or `{:index, n}`
  """
  @spec push_sprite_list(list(), keyword()) :: binary()
  def push_sprite_list(instances, opts \\ []) when is_list(instances) and is_list(opts) do
    case encode_push_sprite_list_payload(instances, opts) do
      {:ok, flags, transparent_value, count, payload} ->
        <<@render_op_push_sprite_list, flags::little-16, transparent_value::little-16,
          count::little-16, payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Pushes many RGB565 source-sprite regions to the current render target.

  `instances` is a list of fixed-width tuples:

      {source_target, src_x, src_y, src_w, src_h, dst_x, dst_y}

  This is intended for atlas-style rendering where setup code prepares sprite
  variants once and the hot frame path only blits selected regions.

  Options:

  - `:transparent` accepts an RGB565 integer or `{:rgb565, n}`
  """
  @spec push_sprite_region_list(list(), keyword()) :: binary()
  def push_sprite_region_list(instances, opts \\ []) when is_list(instances) and is_list(opts) do
    case encode_push_sprite_region_list_payload(instances, opts) do
      {:ok, flags, transparent_value, count, payload} ->
        <<@render_op_push_sprite_region_list, flags::little-16, transparent_value::little-16,
          count::little-16, payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Pushes many transformed source sprites to the current render target.

  `instances` uses the same fixed-point tuple shape as
  `AtomLGFX.push_rotate_zoom_list_to/4`:

      {source_target, x, y, angle_cdeg, zoom_x1024, zoom_y1024}

  Options:

  - `:transparent` accepts an RGB565 integer or `{:index, n}`
  - `:y_offset` is subtracted from each y coordinate natively
  - `:approx_cull` asks native code to skip off-target transforms when safe
  """
  @spec push_rotate_zoom_list(list(), keyword()) :: binary()
  def push_rotate_zoom_list(instances, opts \\ []) when is_list(instances) and is_list(opts) do
    case Sprites.encode_push_rotate_zoom_list_payload(instances, opts) do
      {:ok, flags, payload} ->
        <<@op_push_rotate_zoom_list, flags::little-16, payload::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Decodes a binary-batch command stream into readable command maps.

  This helper is intended for tests, logs, and generated-frame debugging. It does
  not call the native driver. The returned command maps include the command
  index and wire opcode so native `{:batch_failed, index, opcode, reason}`
  errors can be inspected against the original frame script.
  """
  @spec decode(iodata()) :: {:ok, [map()]} | {:error, term()}
  def decode(commands) do
    command_binary = batch(commands)

    if command_binary == <<>> do
      {:error, :empty_batch}
    else
      with {:ok, decoded_commands} <- decode_commands(command_binary, 0, []),
           :ok <- validate_strip_lifecycle(decoded_commands) do
        {:ok, decoded_commands}
      end
    end
  end

  @doc """
  Decodes a binary-batch command stream or raises `ArgumentError`.
  """
  @spec decode!(iodata()) :: [map()]
  def decode!(commands) do
    case decode(commands) do
      {:ok, decoded_commands} -> decoded_commands
      {:error, reason} -> raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Returns a compact diagnostic summary for a binary-batch command stream.

  This helper is intended for logs, benchmarks, and generated-frame debugging. It
  decodes the stream without calling the native driver and reports command counts
  plus useful aggregate values such as batch size, dynamic payload bytes, fixed
  overhead bytes, packed list record bytes, x1000 wire-efficiency ratios, packed
  list command count, packed list instance count, and transform instance count.
  """
  @spec summary(iodata()) :: {:ok, map()} | {:error, term()}
  def summary(commands) do
    command_binary = batch(commands)

    with {:ok, decoded_commands} <- decode(command_binary) do
      {:ok, summarize_decoded_commands(command_binary, decoded_commands)}
    end
  end

  @doc """
  Returns a binary-batch diagnostic summary or raises `ArgumentError`.
  """
  @spec summary!(iodata()) :: map()
  def summary!(commands) do
    case summary(commands) do
      {:ok, summary} -> summary
      {:error, reason} -> raise ArgumentError, Errors.format_error(reason)
    end
  end

  @doc """
  Returns a structured diagnostic report for a binary-batch command stream.

  Unlike `decode/1` and `summary/1`, this helper returns partial context for
  malformed streams. It is intended for generated-frame debugging and test
  failure messages where knowing the last successfully decoded command is more
  useful than only seeing the failing opcode.
  """
  @spec diagnose(iodata()) :: {:ok, map()} | {:error, map()}
  def diagnose(commands) do
    command_binary = batch(commands)

    if command_binary == <<>> do
      {:error, build_diagnosis(command_binary, [], :empty_batch)}
    else
      case decode_commands_with_partial(command_binary, 0, []) do
        {:ok, decoded_commands} ->
          case validate_strip_lifecycle(decoded_commands) do
            :ok ->
              {:ok, summarize_valid_diagnosis(command_binary, decoded_commands)}

            {:error, reason} ->
              {:error, build_diagnosis(command_binary, decoded_commands, reason)}
          end

        {:error, reason, decoded_commands} ->
          {:error, build_diagnosis(command_binary, decoded_commands, reason)}
      end
    end
  end

  @doc """
  Returns a successful binary-batch diagnostic report or raises `ArgumentError`.
  """
  @spec diagnose!(iodata()) :: map()
  def diagnose!(commands) do
    case diagnose(commands) do
      {:ok, diagnosis} -> diagnosis
      {:error, diagnosis} -> raise ArgumentError, Map.fetch!(diagnosis, :message)
    end
  end

  @doc """
  Compares two binary-batch command streams using `summary/1` metrics.

  The first argument is treated as the baseline and the second argument as the
  candidate. Delta values are `candidate - baseline`, while
  `:batch_bytes_savings` is `baseline - candidate` for easier compactness checks.

  This helper is intended for tests, logs, and ADR-driven profiling. It does not
  call the native driver and does not change the render hot path.
  """
  @spec compare(iodata(), iodata()) ::
          {:ok, map()} | {:error, {:baseline, term()}} | {:error, {:candidate, term()}}
  def compare(baseline_commands, candidate_commands) do
    case summary(baseline_commands) do
      {:ok, baseline_summary} ->
        case summary(candidate_commands) do
          {:ok, candidate_summary} ->
            {:ok,
             %{
               baseline: baseline_summary,
               candidate: candidate_summary,
               delta: compare_summary_delta(baseline_summary, candidate_summary)
             }}

          {:error, reason} ->
            {:error, {:candidate, reason}}
        end

      {:error, reason} ->
        {:error, {:baseline, reason}}
    end
  end

  @doc """
  Compares two binary-batch command streams or raises `ArgumentError`.
  """
  @spec compare!(iodata(), iodata()) :: map()
  def compare!(baseline_commands, candidate_commands) do
    case compare(baseline_commands, candidate_commands) do
      {:ok, comparison} ->
        comparison

      {:error, {side, reason}} ->
        raise ArgumentError, "#{side} batch invalid: #{Errors.format_error(reason)}"
    end
  end

  @doc """
  Checks a binary-batch command stream against caller-provided diagnostic limits.

  This helper is intended for tests, generated-frame guardrails, and benchmark
  logs. It uses `summary/1`, so it does not call the native driver and does not
  change the render hot path.

  Supported limit keys are:

    * `:max_batch_bytes`
    * `:max_command_count`
    * `:max_scalar_count`
    * `:max_render_private_count`
    * `:max_dynamic_payload_bytes`
    * `:max_fixed_overhead_bytes`
    * `:max_bytes_per_command_x1000`
    * `:max_bytes_per_logical_scalar_x1000`
    * `:max_dynamic_payload_ratio_x1000`
    * `:max_packed_list_record_ratio_x1000`
    * `:max_packed_list_instances_per_command_x1000`
    * `:min_packed_list_count`
    * `:min_packed_list_instance_count`
    * `:min_packed_list_instances_per_command_x1000`

  Returns `{:ok, report}` when all limits pass, or
  `{:error, {:budget_exceeded, report}}` when any limit fails.
  """
  @spec check_budget(iodata(), map() | keyword()) ::
          {:ok, map()} | {:error, term()} | {:error, {:budget_exceeded, map()}}
  def check_budget(commands, limits) when is_map(limits) or is_list(limits) do
    with {:ok, normalized_limits} <- normalize_budget_limits(limits),
         {:ok, summary} <- summary(commands) do
      violations = budget_violations(summary, normalized_limits)

      report = %{
        ok?: violations == [],
        summary: summary,
        limits: normalized_budget_limits_map(normalized_limits),
        violations: violations
      }

      if violations == [] do
        {:ok, report}
      else
        {:error, {:budget_exceeded, report}}
      end
    end
  end

  def check_budget(_commands, limits) do
    {:error, {:invalid_budget_limits, limits}}
  end

  @doc """
  Checks a binary-batch command stream against diagnostic limits or raises
  `ArgumentError`.
  """
  @spec check_budget!(iodata(), map() | keyword()) :: map()
  def check_budget!(commands, limits) do
    case check_budget(commands, limits) do
      {:ok, report} ->
        report

      {:error, {:budget_exceeded, report}} ->
        raise ArgumentError, format_budget_exceeded(report)

      {:error, {:invalid_budget_limit, key, value}} ->
        raise ArgumentError,
              "invalid binary batch budget limit #{inspect(key)}: #{inspect(value)}"

      {:error, {:invalid_budget_limits, value}} ->
        raise ArgumentError, "invalid binary batch budget limits: #{inspect(value)}"

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  @budget_limit_specs %{
    max_batch_bytes: {:max, :batch_bytes},
    max_command_count: {:max, :command_count},
    max_scalar_count: {:max, :scalar_count},
    max_render_private_count: {:max, :render_private_count},
    max_dynamic_payload_bytes: {:max, :dynamic_payload_bytes},
    max_fixed_overhead_bytes: {:max, :fixed_overhead_bytes},
    max_bytes_per_command_x1000: {:max, :bytes_per_command_x1000},
    max_bytes_per_logical_scalar_x1000: {:max, :bytes_per_logical_scalar_x1000},
    max_dynamic_payload_ratio_x1000: {:max, :dynamic_payload_ratio_x1000},
    max_packed_list_record_ratio_x1000: {:max, :packed_list_record_ratio_x1000},
    max_packed_list_instances_per_command_x1000: {:max, :packed_list_instances_per_command_x1000},
    min_packed_list_count: {:min, :packed_list_count},
    min_packed_list_instance_count: {:min, :packed_list_instance_count},
    min_packed_list_instances_per_command_x1000: {:min, :packed_list_instances_per_command_x1000}
  }

  defp normalize_budget_limits(limits) when is_map(limits) do
    limits
    |> Map.to_list()
    |> normalize_budget_limit_entries([])
  end

  defp normalize_budget_limits(limits) when is_list(limits) do
    normalize_budget_limit_entries(limits, [])
  end

  defp normalize_budget_limit_entries([], acc), do: {:ok, :lists.reverse(acc)}

  defp normalize_budget_limit_entries([{limit_key, value} | rest], acc)
       when is_atom(limit_key) and is_integer(value) and value >= 0 do
    case Map.fetch(@budget_limit_specs, limit_key) do
      {:ok, {direction, metric}} ->
        normalize_budget_limit_entries(rest, [{limit_key, direction, metric, value} | acc])

      :error ->
        {:error, {:invalid_budget_limit, limit_key, value}}
    end
  end

  defp normalize_budget_limit_entries([{limit_key, value} | _rest], _acc) do
    {:error, {:invalid_budget_limit, limit_key, value}}
  end

  defp normalize_budget_limit_entries([entry | _rest], _acc) do
    {:error, {:invalid_budget_limit, entry, nil}}
  end

  defp normalized_budget_limits_map(normalized_limits) do
    normalized_limits
    |> Enum.map(fn {limit_key, _direction, _metric, value} -> {limit_key, value} end)
    |> Map.new()
  end

  defp budget_violations(summary, normalized_limits) do
    normalized_limits
    |> Enum.reduce([], fn limit, acc ->
      case budget_violation(summary, limit) do
        nil -> acc
        violation -> [violation | acc]
      end
    end)
    |> :lists.reverse()
  end

  defp budget_violation(summary, {limit_key, direction, metric, limit_value}) do
    actual_value = Map.fetch!(summary, metric)

    cond do
      is_nil(actual_value) ->
        %{
          limit: limit_key,
          metric: metric,
          direction: direction,
          actual: nil,
          limit_value: limit_value,
          reason: :metric_unavailable
        }

      direction == :max and actual_value > limit_value ->
        %{
          limit: limit_key,
          metric: metric,
          direction: :max,
          actual: actual_value,
          limit_value: limit_value,
          over_by: actual_value - limit_value
        }

      direction == :min and actual_value < limit_value ->
        %{
          limit: limit_key,
          metric: metric,
          direction: :min,
          actual: actual_value,
          limit_value: limit_value,
          under_by: limit_value - actual_value
        }

      true ->
        nil
    end
  end

  defp format_budget_exceeded(%{violations: violations}) do
    formatted_violations =
      violations
      |> Enum.map(&format_budget_violation/1)
      |> Enum.join(", ")

    "binary batch exceeds budget: #{formatted_violations}"
  end

  defp format_budget_violation(%{
         reason: :metric_unavailable,
         metric: metric,
         limit: limit_key,
         limit_value: limit_value
       }) do
    "#{metric} unavailable for #{limit_key} #{limit_value}"
  end

  defp format_budget_violation(%{
         direction: :max,
         metric: metric,
         actual: actual_value,
         limit: limit_key,
         limit_value: limit_value
       }) do
    "#{metric} #{actual_value} > #{limit_key} #{limit_value}"
  end

  defp format_budget_violation(%{
         direction: :min,
         metric: metric,
         actual: actual_value,
         limit: limit_key,
         limit_value: limit_value
       }) do
    "#{metric} #{actual_value} < #{limit_key} #{limit_value}"
  end

  @comparison_delta_keys [
    :batch_bytes,
    :command_count,
    :scalar_count,
    :render_private_count,
    :dynamic_payload_bytes,
    :packed_list_record_bytes,
    :packed_list_count,
    :packed_list_instance_count,
    :fixed_overhead_bytes,
    :bytes_per_command_x1000,
    :bytes_per_logical_scalar_x1000,
    :dynamic_payload_ratio_x1000,
    :packed_list_record_ratio_x1000,
    :packed_list_instances_per_command_x1000
  ]

  defp compare_summary_delta(baseline_summary, candidate_summary) do
    @comparison_delta_keys
    |> Enum.reduce(%{}, fn key, acc ->
      baseline_value = Map.fetch!(baseline_summary, key)
      candidate_value = Map.fetch!(candidate_summary, key)

      Map.put(acc, key, optional_delta(baseline_value, candidate_value))
    end)
    |> Map.put(
      :batch_bytes_savings,
      Map.fetch!(baseline_summary, :batch_bytes) - Map.fetch!(candidate_summary, :batch_bytes)
    )
    |> Map.put(
      :batch_bytes_savings_ratio_x1000,
      ratio_x1000(
        Map.fetch!(baseline_summary, :batch_bytes) - Map.fetch!(candidate_summary, :batch_bytes),
        Map.fetch!(baseline_summary, :batch_bytes)
      )
    )
    |> Map.put(
      :candidate_batch_bytes_ratio_x1000,
      ratio_x1000(
        Map.fetch!(candidate_summary, :batch_bytes),
        Map.fetch!(baseline_summary, :batch_bytes)
      )
    )
  end

  defp optional_delta(nil, _candidate_value), do: nil
  defp optional_delta(_baseline_value, nil), do: nil
  defp optional_delta(baseline_value, candidate_value), do: candidate_value - baseline_value

  defp summarize_decoded_commands(command_binary, decoded_commands) do
    decoded_commands
    |> Enum.reduce(initial_summary(command_binary), &accumulate_summary_command/2)
    |> finalize_summary()
  end

  defp initial_summary(command_binary) do
    %{
      batch_bytes: byte_size(command_binary),
      command_count: 0,
      ops: %{},
      scalar_count: 0,
      render_private_count: 0,
      dynamic_payload_bytes: 0,
      packed_list_record_bytes: 0,
      packed_list_count: 0,
      packed_list_instance_count: 0,
      draw_pixel_list_count: 0,
      draw_pixel_list_instance_count: 0,
      draw_rect_list_count: 0,
      draw_rect_list_instance_count: 0,
      fill_rect_list_count: 0,
      fill_rect_list_instance_count: 0,
      draw_circle_list_count: 0,
      draw_circle_list_instance_count: 0,
      fill_circle_list_count: 0,
      fill_circle_list_instance_count: 0,
      draw_ellipse_list_count: 0,
      draw_ellipse_list_instance_count: 0,
      fill_ellipse_list_count: 0,
      fill_ellipse_list_instance_count: 0,
      draw_line_list_count: 0,
      draw_line_list_instance_count: 0,
      draw_triangle_list_count: 0,
      draw_triangle_list_instance_count: 0,
      fill_triangle_list_count: 0,
      fill_triangle_list_instance_count: 0,
      clip_count: 0,
      text_count: 0,
      image_count: 0,
      sprite_state_count: 0,
      sprite_push_count: 0,
      push_rotate_zoom_count: 0,
      sprite_push_list_count: 0,
      sprite_push_list_instance_count: 0,
      sprite_region_list_count: 0,
      sprite_region_list_instance_count: 0,
      push_rotate_zoom_list_count: 0,
      push_rotate_zoom_instance_count: 0,
      strip_begin_count: 0,
      strip_present_count: 0,
      display_count: 0,
      target_count: 0,
      fixed_overhead_bytes: 0,
      bytes_per_command_x1000: nil,
      bytes_per_logical_scalar_x1000: nil,
      dynamic_payload_ratio_x1000: 0,
      packed_list_record_ratio_x1000: 0,
      packed_list_instances_per_command_x1000: nil
    }
  end

  defp finalize_summary(summary) do
    batch_bytes = Map.fetch!(summary, :batch_bytes)
    command_count = Map.fetch!(summary, :command_count)
    scalar_count = Map.fetch!(summary, :scalar_count)
    dynamic_payload_bytes = Map.fetch!(summary, :dynamic_payload_bytes)
    packed_list_record_bytes = Map.fetch!(summary, :packed_list_record_bytes)
    packed_list_count = Map.fetch!(summary, :packed_list_count)
    packed_list_instance_count = Map.fetch!(summary, :packed_list_instance_count)

    summary
    |> Map.put(:fixed_overhead_bytes, max(batch_bytes - dynamic_payload_bytes, 0))
    |> Map.put(:bytes_per_command_x1000, ratio_x1000(batch_bytes, command_count))
    |> Map.put(:bytes_per_logical_scalar_x1000, ratio_x1000(batch_bytes, scalar_count))
    |> Map.put(:dynamic_payload_ratio_x1000, ratio_x1000(dynamic_payload_bytes, batch_bytes))
    |> Map.put(
      :packed_list_record_ratio_x1000,
      ratio_x1000(packed_list_record_bytes, batch_bytes)
    )
    |> Map.put(
      :packed_list_instances_per_command_x1000,
      ratio_x1000(packed_list_instance_count, packed_list_count)
    )
  end

  defp ratio_x1000(_numerator, denominator) when denominator in [0, nil], do: nil

  defp ratio_x1000(numerator, denominator)
       when is_integer(numerator) and is_integer(denominator) and denominator > 0 do
    div(numerator * 1000, denominator)
  end

  defp accumulate_summary_command(command, summary) do
    op = Map.fetch!(command, :op)

    summary
    |> Map.update!(:command_count, &(&1 + 1))
    |> Map.update!(:ops, &increment_count(&1, op))
    |> accumulate_render_private_count(command)
    |> accumulate_summary_category(command)
  end

  defp accumulate_render_private_count(summary, %{opcode: opcode})
       when opcode in @render_private_opcode_values do
    Map.update!(summary, :render_private_count, &(&1 + 1))
  end

  defp accumulate_render_private_count(summary, _command), do: summary

  defp accumulate_dynamic_payload_bytes(summary, bytes) when is_integer(bytes) and bytes >= 0 do
    Map.update!(summary, :dynamic_payload_bytes, &(&1 + bytes))
  end

  defp accumulate_packed_list(summary, instance_count, record_size)
       when is_integer(instance_count) and instance_count >= 0 and is_integer(record_size) and
              record_size >= 0 do
    record_bytes = instance_count * record_size

    summary
    |> Map.update!(:packed_list_count, &(&1 + 1))
    |> Map.update!(:packed_list_instance_count, &(&1 + instance_count))
    |> Map.update!(:packed_list_record_bytes, &(&1 + record_bytes))
    |> accumulate_dynamic_payload_bytes(record_bytes)
  end

  defp accumulate_summary_category(summary, %{op: op})
       when op in [
              :fill_screen,
              :clear,
              :draw_pixel,
              :draw_fast_vline,
              :draw_fast_hline,
              :draw_line,
              :draw_rect,
              :fill_rect,
              :draw_round_rect,
              :fill_round_rect,
              :draw_circle,
              :fill_circle,
              :draw_ellipse,
              :fill_ellipse,
              :draw_arc,
              :fill_arc,
              :draw_bezier,
              :draw_triangle,
              :fill_triangle
            ] do
    Map.update!(summary, :scalar_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :draw_pixel_list, pixels: pixels}) do
    pixel_count = length(pixels)

    summary
    |> Map.update!(:scalar_count, &(&1 + pixel_count))
    |> Map.update!(:draw_pixel_list_count, &(&1 + 1))
    |> Map.update!(:draw_pixel_list_instance_count, &(&1 + pixel_count))
    |> accumulate_packed_list(pixel_count, @draw_pixel_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :draw_rect_list, rectangles: rectangles}) do
    rectangle_count = length(rectangles)

    summary
    |> Map.update!(:scalar_count, &(&1 + rectangle_count))
    |> Map.update!(:draw_rect_list_count, &(&1 + 1))
    |> Map.update!(:draw_rect_list_instance_count, &(&1 + rectangle_count))
    |> accumulate_packed_list(rectangle_count, @draw_rect_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :fill_rect_list, rectangles: rectangles}) do
    rectangle_count = length(rectangles)

    summary
    |> Map.update!(:scalar_count, &(&1 + rectangle_count))
    |> Map.update!(:fill_rect_list_count, &(&1 + 1))
    |> Map.update!(:fill_rect_list_instance_count, &(&1 + rectangle_count))
    |> accumulate_packed_list(rectangle_count, @fill_rect_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :draw_circle_list, circles: circles}) do
    circle_count = length(circles)

    summary
    |> Map.update!(:scalar_count, &(&1 + circle_count))
    |> Map.update!(:draw_circle_list_count, &(&1 + 1))
    |> Map.update!(:draw_circle_list_instance_count, &(&1 + circle_count))
    |> accumulate_packed_list(circle_count, @draw_circle_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :fill_circle_list, circles: circles}) do
    circle_count = length(circles)

    summary
    |> Map.update!(:scalar_count, &(&1 + circle_count))
    |> Map.update!(:fill_circle_list_count, &(&1 + 1))
    |> Map.update!(:fill_circle_list_instance_count, &(&1 + circle_count))
    |> accumulate_packed_list(circle_count, @fill_circle_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :draw_ellipse_list, ellipses: ellipses}) do
    ellipse_count = length(ellipses)

    summary
    |> Map.update!(:scalar_count, &(&1 + ellipse_count))
    |> Map.update!(:draw_ellipse_list_count, &(&1 + 1))
    |> Map.update!(:draw_ellipse_list_instance_count, &(&1 + ellipse_count))
    |> accumulate_packed_list(ellipse_count, @ellipse_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :fill_ellipse_list, ellipses: ellipses}) do
    ellipse_count = length(ellipses)

    summary
    |> Map.update!(:scalar_count, &(&1 + ellipse_count))
    |> Map.update!(:fill_ellipse_list_count, &(&1 + 1))
    |> Map.update!(:fill_ellipse_list_instance_count, &(&1 + ellipse_count))
    |> accumulate_packed_list(ellipse_count, @ellipse_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :draw_line_list, lines: lines}) do
    line_count = length(lines)

    summary
    |> Map.update!(:scalar_count, &(&1 + line_count))
    |> Map.update!(:draw_line_list_count, &(&1 + 1))
    |> Map.update!(:draw_line_list_instance_count, &(&1 + line_count))
    |> accumulate_packed_list(line_count, @draw_line_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :draw_triangle_list, triangles: triangles}) do
    triangle_count = length(triangles)

    summary
    |> Map.update!(:scalar_count, &(&1 + triangle_count))
    |> Map.update!(:draw_triangle_list_count, &(&1 + 1))
    |> Map.update!(:draw_triangle_list_instance_count, &(&1 + triangle_count))
    |> accumulate_packed_list(triangle_count, @draw_triangle_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :fill_triangle_list, triangles: triangles}) do
    triangle_count = length(triangles)

    summary
    |> Map.update!(:scalar_count, &(&1 + triangle_count))
    |> Map.update!(:fill_triangle_list_count, &(&1 + 1))
    |> Map.update!(:fill_triangle_list_instance_count, &(&1 + triangle_count))
    |> accumulate_packed_list(triangle_count, @fill_triangle_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: op})
       when op in [:set_clip_rect, :clear_clip_rect] do
    Map.update!(summary, :clip_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: op, text_len: text_len})
       when op in [:draw_string, :print, :println] do
    summary
    |> Map.update!(:text_count, &(&1 + 1))
    |> accumulate_dynamic_payload_bytes(text_len)
  end

  defp accumulate_summary_category(summary, %{op: op})
       when op in [
              :set_text_font_preset,
              :set_text_size,
              :set_text_datum,
              :set_text_wrap,
              :set_cursor,
              :set_text_color
            ] do
    Map.update!(summary, :text_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :draw_jpg, jpeg_len: jpeg_len}) do
    summary
    |> Map.update!(:image_count, &(&1 + 1))
    |> accumulate_dynamic_payload_bytes(jpeg_len)
  end

  defp accumulate_summary_category(summary, %{op: :push_image, pixels_len: pixels_len}) do
    summary
    |> Map.update!(:image_count, &(&1 + 1))
    |> accumulate_dynamic_payload_bytes(pixels_len)
  end

  defp accumulate_summary_category(summary, %{op: op})
       when op in [:set_palette_color, :set_pivot] do
    Map.update!(summary, :sprite_state_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :push_sprite}) do
    Map.update!(summary, :sprite_push_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :push_rotate_zoom}) do
    summary
    |> Map.update!(:push_rotate_zoom_count, &(&1 + 1))
    |> Map.update!(:sprite_push_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :push_sprite_list, instances: instances}) do
    instance_count = length(instances)

    summary
    |> Map.update!(:sprite_push_list_count, &(&1 + 1))
    |> Map.update!(:sprite_push_list_instance_count, &(&1 + instance_count))
    |> Map.update!(:sprite_push_count, &(&1 + instance_count))
    |> accumulate_packed_list(instance_count, @sprite_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :push_sprite_region_list, instances: instances}) do
    instance_count = length(instances)

    summary
    |> Map.update!(:sprite_region_list_count, &(&1 + 1))
    |> Map.update!(:sprite_region_list_instance_count, &(&1 + instance_count))
    |> Map.update!(:sprite_push_count, &(&1 + instance_count))
    |> accumulate_packed_list(instance_count, @sprite_region_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :push_rotate_zoom_list, instances: instances}) do
    instance_count = length(instances)

    summary
    |> Map.update!(:push_rotate_zoom_list_count, &(&1 + 1))
    |> Map.update!(:push_rotate_zoom_instance_count, &(&1 + instance_count))
    |> accumulate_packed_list(instance_count, @push_rotate_zoom_list_record_size)
  end

  defp accumulate_summary_category(summary, %{op: :begin_strip}) do
    Map.update!(summary, :strip_begin_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :present_strip}) do
    Map.update!(summary, :strip_present_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :display}) do
    Map.update!(summary, :display_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, %{op: :target}) do
    Map.update!(summary, :target_count, &(&1 + 1))
  end

  defp accumulate_summary_category(summary, _command), do: summary

  defp summarize_valid_diagnosis(command_binary, decoded_commands) do
    summarize_decoded_commands(command_binary, decoded_commands)
    |> Map.put(:valid?, true)
    |> Map.put(:message, "binary batch is valid")
    |> Map.put(:error, nil)
    |> Map.put(:failed_index, nil)
    |> Map.put(:failed_opcode, nil)
    |> Map.put(:failed_op, nil)
    |> Map.put(:decoded_command_count, length(decoded_commands))
    |> Map.put(:last_decoded_command, List.last(decoded_commands))
  end

  defp build_diagnosis(command_binary, decoded_commands, reason) do
    {failed_index, failed_opcode} = failed_command_location(reason)

    summarize_decoded_commands(command_binary, decoded_commands)
    |> Map.put(:valid?, false)
    |> Map.put(:message, Errors.format_error(reason))
    |> Map.put(:error, reason)
    |> Map.put(:failed_index, failed_index)
    |> Map.put(:failed_opcode, failed_opcode)
    |> Map.put(:failed_op, opcode_name(failed_opcode))
    |> Map.put(:decoded_command_count, length(decoded_commands))
    |> Map.put(:last_decoded_command, List.last(decoded_commands))
  end

  defp failed_command_location({:batch_failed, index, opcode, _reason}), do: {index, opcode}
  defp failed_command_location({:batch_failed, {index, opcode, _reason}}), do: {index, opcode}
  defp failed_command_location(_reason), do: {nil, nil}

  defp opcode_name(nil), do: nil

  defp opcode_name(opcode) do
    case render_private_opcode_name(opcode) do
      nil ->
        case OpSchema.name(opcode) do
          {:ok, name} -> name
          :error -> nil
        end

      name ->
        name
    end
  end

  defp render_private_opcode_name(opcode) do
    find_render_private_opcode_name(@render_private_opcodes, opcode)
  end

  defp find_render_private_opcode_name([], _opcode), do: nil

  defp find_render_private_opcode_name([{name, value} | _rest], opcode) when value == opcode,
    do: name

  defp find_render_private_opcode_name([_entry | rest], opcode) do
    find_render_private_opcode_name(rest, opcode)
  end

  defp increment_count(counts, key) do
    Map.update(counts, key, 1, &(&1 + 1))
  end

  defp validate_strip_lifecycle(decoded_commands) do
    validate_strip_lifecycle(decoded_commands, false)
  end

  defp validate_strip_lifecycle([], false), do: :ok

  defp validate_strip_lifecycle([], true) do
    {:error, {:batch_failed, :end_of_batch, 0, :strip_not_presented}}
  end

  defp validate_strip_lifecycle([%{op: :begin_strip, index: index, opcode: opcode} | _rest], true) do
    {:error, {:batch_failed, index, opcode, :strip_already_active}}
  end

  defp validate_strip_lifecycle([%{op: :begin_strip} | rest], false) do
    validate_strip_lifecycle(rest, true)
  end

  defp validate_strip_lifecycle(
         [%{op: :present_strip, index: index, opcode: opcode} | _rest],
         false
       ) do
    {:error, {:batch_failed, index, opcode, :strip_not_active}}
  end

  defp validate_strip_lifecycle([%{op: :present_strip} | rest], true) do
    validate_strip_lifecycle(rest, false)
  end

  defp validate_strip_lifecycle([%{op: :display, index: index, opcode: opcode} | _rest], true) do
    {:error, {:batch_failed, index, opcode, :strip_not_presented}}
  end

  defp validate_strip_lifecycle([%{op: :display} | rest], false) do
    validate_strip_lifecycle(rest, false)
  end

  defp validate_strip_lifecycle([_command | rest], strip_active) do
    validate_strip_lifecycle(rest, strip_active)
  end

  defp decode_commands(binary, index, acc) do
    case decode_commands_with_partial(binary, index, acc) do
      {:ok, decoded_commands} -> {:ok, decoded_commands}
      {:error, reason, _decoded_commands} -> {:error, reason}
    end
  end

  defp decode_commands_with_partial(<<>>, _index, acc), do: {:ok, :lists.reverse(acc)}

  defp decode_commands_with_partial(<<opcode, rest::binary>>, index, acc) do
    case decode_command(opcode, rest) do
      {:ok, command, remaining} ->
        command = Map.merge(command, %{index: index, opcode: opcode})
        decode_commands_with_partial(remaining, index + 1, [command | acc])

      {:error, reason} ->
        decoded_commands = :lists.reverse(acc)
        {:error, {:batch_failed, index, opcode, reason}, decoded_commands}
    end
  end

  defp decode_command(@render_op_target, <<target, rest::binary>>) do
    if target_any(target) do
      {:ok, %{op: :target, target: target}, rest}
    else
      {:error, {:bad_target, target}}
    end
  end

  defp decode_command(@render_op_color_mode, <<@render_color_mode_rgb565, rest::binary>>) do
    {:ok, %{op: :color_mode, color_mode: :rgb565}, rest}
  end

  defp decode_command(@render_op_color_mode, <<@render_color_mode_palette_index, rest::binary>>) do
    {:ok, %{op: :color_mode, color_mode: :palette_index}, rest}
  end

  defp decode_command(@render_op_color_mode, <<_unknown, _rest::binary>>) do
    {:error, :bad_color_mode}
  end

  defp decode_command(@render_op_begin_strip, <<y0::little-16, rest::binary>>) do
    {:ok, %{op: :begin_strip, y0: y0}, rest}
  end

  defp decode_command(@render_op_present_strip, rest) do
    {:ok, %{op: :present_strip}, rest}
  end

  defp decode_command(@op_display, rest) do
    {:ok, %{op: :display}, rest}
  end

  defp decode_command(@op_clear_clip_rect, rest) do
    {:ok, %{op: :clear_clip_rect}, rest}
  end

  defp decode_command(@op_fill_screen, <<color::little-16, rest::binary>>) do
    {:ok, %{op: :fill_screen, color: color}, rest}
  end

  defp decode_command(@op_clear, <<color::little-16, rest::binary>>) do
    {:ok, %{op: :clear, color: color}, rest}
  end

  defp decode_command(
         @op_draw_pixel,
         <<x::little-signed-16, y::little-signed-16, color::little-16, rest::binary>>
       ) do
    {:ok, %{op: :draw_pixel, x: x, y: y, color: color}, rest}
  end

  defp decode_command(
         @op_draw_fast_vline,
         <<x::little-signed-16, y::little-signed-16, height::little-16, color::little-16,
           rest::binary>>
       ) do
    with :ok <- validate_non_zero(height, :height) do
      {:ok, %{op: :draw_fast_vline, x: x, y: y, height: height, color: color}, rest}
    end
  end

  defp decode_command(
         @op_draw_fast_hline,
         <<x::little-signed-16, y::little-signed-16, width::little-16, color::little-16,
           rest::binary>>
       ) do
    with :ok <- validate_non_zero(width, :width) do
      {:ok, %{op: :draw_fast_hline, x: x, y: y, width: width, color: color}, rest}
    end
  end

  defp decode_command(
         @op_draw_line,
         <<x0::little-signed-16, y0::little-signed-16, x1::little-signed-16, y1::little-signed-16,
           color::little-16, rest::binary>>
       ) do
    {:ok, %{op: :draw_line, x0: x0, y0: y0, x1: x1, y1: y1, color: color}, rest}
  end

  defp decode_command(
         @op_draw_rect,
         <<x::little-signed-16, y::little-signed-16, width::little-16, height::little-16,
           color::little-16, rest::binary>>
       ) do
    with :ok <- validate_non_zero(width, :width),
         :ok <- validate_non_zero(height, :height) do
      {:ok, %{op: :draw_rect, x: x, y: y, width: width, height: height, color: color}, rest}
    end
  end

  defp decode_command(
         @op_fill_rect,
         <<x::little-signed-16, y::little-signed-16, width::little-16, height::little-16,
           color::little-16, rest::binary>>
       ) do
    with :ok <- validate_non_zero(width, :width),
         :ok <- validate_non_zero(height, :height) do
      {:ok, %{op: :fill_rect, x: x, y: y, width: width, height: height, color: color}, rest}
    end
  end

  defp decode_command(
         @render_op_draw_pixel_list,
         <<flags::little-16, count::little-16, rest::binary>>
       ) do
    records_len = count * @draw_pixel_list_record_size

    with :ok <- validate_draw_pixel_list_header(flags, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          {:ok,
           %{
             op: :draw_pixel_list,
             flags: flags,
             pixels: decode_draw_pixel_records(records, [])
           }, remaining}

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(
         @render_op_draw_rect_list,
         <<flags::little-16, count::little-16, rest::binary>>
       ) do
    records_len = count * @draw_rect_list_record_size

    with :ok <- validate_draw_rect_list_header(flags, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          case decode_draw_rect_records(records, []) do
            {:ok, rectangles} ->
              {:ok,
               %{
                 op: :draw_rect_list,
                 flags: flags,
                 rectangles: rectangles
               }, remaining}

            {:error, reason} ->
              {:error, reason}
          end

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(
         @render_op_fill_rect_list,
         <<flags::little-16, count::little-16, rest::binary>>
       ) do
    records_len = count * @fill_rect_list_record_size

    with :ok <- validate_fill_rect_list_header(flags, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          case decode_fill_rect_records(records, []) do
            {:ok, rectangles} ->
              {:ok,
               %{
                 op: :fill_rect_list,
                 flags: flags,
                 rectangles: rectangles
               }, remaining}

            {:error, reason} ->
              {:error, reason}
          end

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(
         @render_op_draw_circle_list,
         <<flags::little-16, count::little-16, rest::binary>>
       ) do
    records_len = count * @draw_circle_list_record_size

    with :ok <- validate_draw_circle_list_header(flags, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          case decode_draw_circle_records(records, []) do
            {:ok, circles} ->
              {:ok,
               %{
                 op: :draw_circle_list,
                 flags: flags,
                 circles: circles
               }, remaining}

            {:error, reason} ->
              {:error, reason}
          end

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(
         @render_op_fill_circle_list,
         <<flags::little-16, count::little-16, rest::binary>>
       ) do
    records_len = count * @fill_circle_list_record_size

    with :ok <- validate_fill_circle_list_header(flags, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          case decode_fill_circle_records(records, []) do
            {:ok, circles} ->
              {:ok,
               %{
                 op: :fill_circle_list,
                 flags: flags,
                 circles: circles
               }, remaining}

            {:error, reason} ->
              {:error, reason}
          end

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(
         @render_op_ellipse_list,
         <<kind, reserved, flags::little-16, count::little-16, rest::binary>>
       ) do
    records_len = count * @ellipse_list_record_size

    with :ok <- validate_ellipse_list_header(kind, reserved, flags, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          case decode_ellipse_records(records, []) do
            {:ok, ellipses} ->
              op =
                if kind == @ellipse_list_kind_draw,
                  do: :draw_ellipse_list,
                  else: :fill_ellipse_list

              {:ok,
               %{
                 op: op,
                 kind: kind,
                 flags: flags,
                 ellipses: ellipses
               }, remaining}

            {:error, reason} ->
              {:error, reason}
          end

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(
         @render_op_draw_line_list,
         <<flags::little-16, count::little-16, rest::binary>>
       ) do
    records_len = count * @draw_line_list_record_size

    with :ok <- validate_draw_line_list_header(flags, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          {:ok,
           %{
             op: :draw_line_list,
             flags: flags,
             lines: decode_draw_line_records(records, [])
           }, remaining}

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(
         @render_op_draw_triangle_list,
         <<flags::little-16, count::little-16, rest::binary>>
       ) do
    records_len = count * @draw_triangle_list_record_size

    with :ok <- validate_draw_triangle_list_header(flags, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          {:ok,
           %{
             op: :draw_triangle_list,
             flags: flags,
             triangles: decode_draw_triangle_records(records, [])
           }, remaining}

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(
         @render_op_fill_triangle_list,
         <<flags::little-16, count::little-16, rest::binary>>
       ) do
    records_len = count * @fill_triangle_list_record_size

    with :ok <- validate_fill_triangle_list_header(flags, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          {:ok,
           %{
             op: :fill_triangle_list,
             flags: flags,
             triangles: decode_fill_triangle_records(records, [])
           }, remaining}

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(
         @op_draw_round_rect,
         <<x::little-signed-16, y::little-signed-16, width::little-16, height::little-16,
           radius::little-16, color::little-16, rest::binary>>
       ) do
    with :ok <- validate_non_zero(width, :width),
         :ok <- validate_non_zero(height, :height),
         :ok <- validate_non_zero(radius, :radius) do
      {:ok,
       %{
         op: :draw_round_rect,
         x: x,
         y: y,
         width: width,
         height: height,
         radius: radius,
         color: color
       }, rest}
    end
  end

  defp decode_command(
         @op_fill_round_rect,
         <<x::little-signed-16, y::little-signed-16, width::little-16, height::little-16,
           radius::little-16, color::little-16, rest::binary>>
       ) do
    with :ok <- validate_non_zero(width, :width),
         :ok <- validate_non_zero(height, :height),
         :ok <- validate_non_zero(radius, :radius) do
      {:ok,
       %{
         op: :fill_round_rect,
         x: x,
         y: y,
         width: width,
         height: height,
         radius: radius,
         color: color
       }, rest}
    end
  end

  defp decode_command(
         @op_draw_circle,
         <<x::little-signed-16, y::little-signed-16, radius::little-16, color::little-16,
           rest::binary>>
       ) do
    with :ok <- validate_non_zero(radius, :radius) do
      {:ok, %{op: :draw_circle, x: x, y: y, radius: radius, color: color}, rest}
    end
  end

  defp decode_command(
         @op_fill_circle,
         <<x::little-signed-16, y::little-signed-16, radius::little-16, color::little-16,
           rest::binary>>
       ) do
    with :ok <- validate_non_zero(radius, :radius) do
      {:ok, %{op: :fill_circle, x: x, y: y, radius: radius, color: color}, rest}
    end
  end

  defp decode_command(
         @op_draw_ellipse,
         <<x::little-signed-16, y::little-signed-16, radius_x::little-16, radius_y::little-16,
           color::little-16, rest::binary>>
       ) do
    with :ok <- validate_non_zero(radius_x, :radius_x),
         :ok <- validate_non_zero(radius_y, :radius_y) do
      {:ok,
       %{op: :draw_ellipse, x: x, y: y, radius_x: radius_x, radius_y: radius_y, color: color},
       rest}
    end
  end

  defp decode_command(
         @op_fill_ellipse,
         <<x::little-signed-16, y::little-signed-16, radius_x::little-16, radius_y::little-16,
           color::little-16, rest::binary>>
       ) do
    with :ok <- validate_non_zero(radius_x, :radius_x),
         :ok <- validate_non_zero(radius_y, :radius_y) do
      {:ok,
       %{op: :fill_ellipse, x: x, y: y, radius_x: radius_x, radius_y: radius_y, color: color},
       rest}
    end
  end

  defp decode_command(
         @op_draw_arc,
         <<x::little-signed-16, y::little-signed-16, radius0::little-16, radius1::little-16,
           angle0::little-float-32, angle1::little-float-32, color::little-16, rest::binary>>
       ) do
    with :ok <- validate_non_zero(radius0, :radius0),
         :ok <- validate_non_zero(radius1, :radius1),
         :ok <- validate_angle(angle0),
         :ok <- validate_angle(angle1) do
      {:ok,
       %{
         op: :draw_arc,
         x: x,
         y: y,
         radius0: radius0,
         radius1: radius1,
         angle0: angle0,
         angle1: angle1,
         color: color
       }, rest}
    end
  end

  defp decode_command(
         @op_fill_arc,
         <<x::little-signed-16, y::little-signed-16, radius0::little-16, radius1::little-16,
           angle0::little-float-32, angle1::little-float-32, color::little-16, rest::binary>>
       ) do
    with :ok <- validate_non_zero(radius0, :radius0),
         :ok <- validate_non_zero(radius1, :radius1),
         :ok <- validate_angle(angle0),
         :ok <- validate_angle(angle1) do
      {:ok,
       %{
         op: :fill_arc,
         x: x,
         y: y,
         radius0: radius0,
         radius1: radius1,
         angle0: angle0,
         angle1: angle1,
         color: color
       }, rest}
    end
  end

  defp decode_command(
         @op_draw_bezier,
         <<3, 0, x0::little-signed-16, y0::little-signed-16, x1::little-signed-16,
           y1::little-signed-16, x2::little-signed-16, y2::little-signed-16, color::little-16,
           rest::binary>>
       ) do
    {:ok,
     %{
       op: :draw_bezier,
       point_count: 3,
       x0: x0,
       y0: y0,
       x1: x1,
       y1: y1,
       x2: x2,
       y2: y2,
       color: color
     }, rest}
  end

  defp decode_command(
         @op_draw_bezier,
         <<4, 0, x0::little-signed-16, y0::little-signed-16, x1::little-signed-16,
           y1::little-signed-16, x2::little-signed-16, y2::little-signed-16, x3::little-signed-16,
           y3::little-signed-16, color::little-16, rest::binary>>
       ) do
    {:ok,
     %{
       op: :draw_bezier,
       point_count: 4,
       x0: x0,
       y0: y0,
       x1: x1,
       y1: y1,
       x2: x2,
       y2: y2,
       x3: x3,
       y3: y3,
       color: color
     }, rest}
  end

  defp decode_command(@op_draw_bezier, <<point_count, _reserved, _rest::binary>>)
       when point_count not in [3, 4] do
    {:error, {:bad_bezier_point_count, point_count}}
  end

  defp decode_command(@op_draw_bezier, <<_point_count, reserved, _rest::binary>>)
       when reserved != 0 do
    {:error, {:bad_reserved, reserved}}
  end

  defp decode_command(
         @op_draw_triangle,
         <<x0::little-signed-16, y0::little-signed-16, x1::little-signed-16, y1::little-signed-16,
           x2::little-signed-16, y2::little-signed-16, color::little-16, rest::binary>>
       ) do
    {:ok, %{op: :draw_triangle, x0: x0, y0: y0, x1: x1, y1: y1, x2: x2, y2: y2, color: color},
     rest}
  end

  defp decode_command(
         @op_fill_triangle,
         <<x0::little-signed-16, y0::little-signed-16, x1::little-signed-16, y1::little-signed-16,
           x2::little-signed-16, y2::little-signed-16, color::little-16, rest::binary>>
       ) do
    {:ok, %{op: :fill_triangle, x0: x0, y0: y0, x1: x1, y1: y1, x2: x2, y2: y2, color: color},
     rest}
  end

  defp decode_command(
         @op_set_clip_rect,
         <<x::little-signed-16, y::little-signed-16, width::little-16, height::little-16,
           rest::binary>>
       ) do
    with :ok <- validate_non_zero(width, :width),
         :ok <- validate_non_zero(height, :height) do
      {:ok, %{op: :set_clip_rect, x: x, y: y, width: width, height: height}, rest}
    end
  end

  defp decode_command(@op_set_text_font_preset, <<@font_preset_ascii, rest::binary>>) do
    {:ok, %{op: :set_text_font_preset, preset: :ascii}, rest}
  end

  defp decode_command(@op_set_text_font_preset, <<@font_preset_jp, rest::binary>>) do
    {:ok, %{op: :set_text_font_preset, preset: :jp}, rest}
  end

  defp decode_command(@op_set_text_font_preset, <<preset, _rest::binary>>) do
    {:error, {:bad_text_font_preset, preset}}
  end

  defp decode_command(
         @op_set_text_size,
         <<scale_x1024::little-16, scale_y1024::little-16, rest::binary>>
       ) do
    with :ok <- validate_non_zero(scale_x1024, :scale_x1024),
         :ok <- validate_non_zero(scale_y1024, :scale_y1024) do
      {:ok, %{op: :set_text_size, scale_x1024: scale_x1024, scale_y1024: scale_y1024}, rest}
    end
  end

  defp decode_command(@op_set_text_datum, <<datum, rest::binary>>) do
    {:ok, %{op: :set_text_datum, datum: datum}, rest}
  end

  defp decode_command(@op_set_text_wrap, <<wrap_x, wrap_y, rest::binary>>)
       when wrap_x in [0, 1] and wrap_y in [0, 1] do
    {:ok, %{op: :set_text_wrap, wrap_x: decode_bool(wrap_x), wrap_y: decode_bool(wrap_y)}, rest}
  end

  defp decode_command(@op_set_text_wrap, <<_wrap_x, _wrap_y, _rest::binary>>) do
    {:error, :bad_text_wrap}
  end

  defp decode_command(
         @op_set_cursor,
         <<x::little-signed-16, y::little-signed-16, rest::binary>>
       ) do
    {:ok, %{op: :set_cursor, x: x, y: y}, rest}
  end

  defp decode_command(@op_set_text_color, <<flags::little-16, fg::little-16, rest::binary>>) do
    allowed_flags =
      Protocol.text_has_bg_flag() ||| Protocol.text_fg_index_flag() |||
        Protocol.text_bg_index_flag()

    has_bg = flag_set?(flags, Protocol.text_has_bg_flag())
    fg_is_index = flag_set?(flags, Protocol.text_fg_index_flag())
    bg_is_index = flag_set?(flags, Protocol.text_bg_index_flag())

    cond do
      (flags &&& bnot(allowed_flags)) != 0 ->
        {:error, {:bad_flags, flags}}

      bg_is_index and not has_bg ->
        {:error, {:bad_flags, flags}}

      fg_is_index and fg > 0xFF ->
        {:error, {:bad_text_color, :fg, {:index, fg}}}

      true ->
        decode_text_color_rest(flags, fg, rest, has_bg, bg_is_index)
    end
  end

  defp decode_command(
         @render_op_push_sprite_transparent,
         <<flags::little-16, source_target, x::little-signed-16, y::little-signed-16,
           transparent::little-16, rest::binary>>
       ) do
    cond do
      (flags &&& bnot(Protocol.transparent_index_flag())) != 0 ->
        {:error, {:bad_flags, flags}}

      not sprite_handle(source_target) ->
        {:error, {:bad_sprite_target, source_target}}

      true ->
        transparent_color = decode_transparent_by_flags(flags, transparent)

        case transparent_color do
          {:index, index} when index > 0xFF ->
            {:error, {:bad_transparent_color, transparent_color}}

          _ ->
            {:ok,
             %{
               op: :push_sprite,
               source_target: source_target,
               x: x,
               y: y,
               transparent: transparent_color,
               flags: flags
             }, rest}
        end
    end
  end

  defp decode_command(
         @render_op_push_sprite_list,
         <<flags::little-16, transparent::little-16, count::little-16, rest::binary>>
       ) do
    records_len = count * @sprite_list_record_size

    with :ok <- validate_push_sprite_list_header(flags, transparent, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          case decode_push_sprite_records(records, []) do
            {:ok, instances} ->
              {:ok,
               %{
                 op: :push_sprite_list,
                 flags: flags,
                 transparent: decode_push_sprite_list_transparent(flags, transparent),
                 instances: instances
               }, remaining}

            {:error, reason} ->
              {:error, reason}
          end

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(
         @render_op_push_sprite_region_list,
         <<flags::little-16, transparent::little-16, count::little-16, rest::binary>>
       ) do
    records_len = count * @sprite_region_list_record_size

    with :ok <- validate_push_sprite_region_list_header(flags, transparent, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          case decode_push_sprite_region_records(records, []) do
            {:ok, instances} ->
              {:ok,
               %{
                 op: :push_sprite_region_list,
                 flags: flags,
                 transparent: decode_push_sprite_region_list_transparent(flags, transparent),
                 instances: instances
               }, remaining}

            {:error, reason} ->
              {:error, reason}
          end

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(
         @op_draw_jpg,
         <<0, 0, x::little-signed-16, y::little-signed-16, jpeg_len::little-32, rest::binary>>
       ) do
    decode_jpeg_payload(
      %{op: :draw_jpg, x: x, y: y, variant: :basic, jpeg_len: jpeg_len},
      jpeg_len,
      rest
    )
  end

  defp decode_command(
         @op_draw_jpg,
         <<1, 0, x::little-signed-16, y::little-signed-16, max_width::little-16,
           max_height::little-16, off_x::little-signed-16, off_y::little-signed-16,
           scale_x::little-float-32, scale_y::little-float-32, jpeg_len::little-32, rest::binary>>
       ) do
    with :ok <- validate_image_scale(scale_x),
         :ok <- validate_image_scale(scale_y) do
      decode_jpeg_payload(
        %{
          op: :draw_jpg,
          x: x,
          y: y,
          variant: :scaled,
          max_width: max_width,
          max_height: max_height,
          off_x: off_x,
          off_y: off_y,
          scale_x: scale_x,
          scale_y: scale_y,
          jpeg_len: jpeg_len
        },
        jpeg_len,
        rest
      )
    end
  end

  defp decode_command(@op_draw_jpg, <<_variant, reserved, _rest::binary>>) when reserved != 0 do
    {:error, {:bad_reserved, reserved}}
  end

  defp decode_command(@op_draw_jpg, <<variant, _reserved, _rest::binary>>) do
    {:error, {:bad_jpeg_variant, variant}}
  end

  defp decode_command(
         @op_push_image,
         <<x::little-signed-16, y::little-signed-16, width::little-16, height::little-16,
           stride_pixels::little-16, pixels_len::little-32, rest::binary>>
       ) do
    case rest do
      <<pixels::binary-size(pixels_len), remaining::binary>> ->
        case validate_push_image_rgb565_payload(width, height, pixels, stride_pixels) do
          {:ok, _pixels_len} ->
            {:ok,
             %{
               op: :push_image,
               x: x,
               y: y,
               width: width,
               height: height,
               stride_pixels: stride_pixels,
               pixels_len: pixels_len
             }, remaining}

          {:error, reason} ->
            {:error, reason}
        end

      _ ->
        {:error, :truncated}
    end
  end

  defp decode_command(
         @op_set_palette_color,
         <<palette_index, 0, rgb888::little-32, rest::binary>>
       ) do
    if color888(rgb888) do
      {:ok, %{op: :set_palette_color, palette_index: palette_index, rgb888: rgb888}, rest}
    else
      {:error, {:bad_palette_color, rgb888}}
    end
  end

  defp decode_command(@op_set_palette_color, <<_palette_index, reserved, _rest::binary>>)
       when reserved != 0 do
    {:error, {:bad_reserved, reserved}}
  end

  defp decode_command(
         @op_set_pivot,
         <<x::little-signed-16, y::little-signed-16, rest::binary>>
       ) do
    {:ok, %{op: :set_pivot, x: x, y: y}, rest}
  end

  defp decode_command(
         @op_push_rotate_zoom,
         <<options, 0, flags::little-16, source_target, 0, x::little-signed-16,
           y::little-signed-16, angle_deg::little-float-32, zoom_x::little-float-32,
           zoom_y::little-float-32, transparent::little-16, rest::binary>>
       ) do
    with :ok <- validate_push_rotate_zoom_header(flags, options, transparent),
         :ok <- validate_angle(angle_deg),
         :ok <- validate_image_scale(zoom_x),
         :ok <- validate_image_scale(zoom_y) do
      if sprite_handle(source_target) do
        {:ok,
         %{
           op: :push_rotate_zoom,
           flags: flags,
           source_target: source_target,
           x: x,
           y: y,
           angle_deg: angle_deg,
           zoom_x: zoom_x,
           zoom_y: zoom_y,
           transparent: decode_push_rotate_zoom_transparent(flags, options, transparent)
         }, rest}
      else
        {:error, {:bad_sprite_target, source_target}}
      end
    end
  end

  defp decode_command(@op_push_rotate_zoom, <<_options, reserved, _rest::binary>>)
       when reserved != 0 do
    {:error, {:bad_reserved, reserved}}
  end

  defp decode_command(
         @op_push_rotate_zoom,
         <<_options, 0, _flags::little-16, _source_target, reserved, _rest::binary>>
       )
       when reserved != 0 do
    {:error, {:bad_reserved, reserved}}
  end

  defp decode_command(
         @op_push_rotate_zoom_list,
         <<flags::little-16, ?P, ?R, ?Z, ?L, 1, options, transparent::little-16,
           y_offset::little-signed-16, count::little-16, rest::binary>>
       ) do
    records_len = count * 12

    with :ok <- validate_push_rotate_zoom_list_header(flags, options, transparent, count) do
      case rest do
        <<records::binary-size(records_len), remaining::binary>> ->
          case decode_push_rotate_zoom_records(records, []) do
            {:ok, instances} ->
              {:ok,
               %{
                 op: :push_rotate_zoom_list,
                 flags: flags,
                 options: options,
                 transparent:
                   decode_push_rotate_zoom_list_transparent(flags, options, transparent),
                 y_offset: y_offset,
                 approx_cull: flag_set?(options, 0x02),
                 instances: instances
               }, remaining}

            {:error, reason} ->
              {:error, reason}
          end

        _ ->
          {:error, :truncated}
      end
    end
  end

  defp decode_command(@op_push_rotate_zoom_list, <<_flags::little-16, _rest::binary>>) do
    {:error, :bad_push_rotate_zoom_list_payload}
  end

  defp decode_command(@op_set_text_color, _rest) do
    {:error, :truncated}
  end

  defp decode_command(
         @op_draw_string,
         <<x::little-signed-16, y::little-signed-16, text_len::little-16, rest::binary>>
       ) do
    case rest do
      <<text::binary-size(text_len), remaining::binary>> ->
        cond do
          text_len == 0 ->
            {:error, :empty_text}

          contains_nul?(text) ->
            {:error, :text_contains_nul}

          true ->
            {:ok, %{op: :draw_string, x: x, y: y, text: text, text_len: text_len}, remaining}
        end

      _ ->
        {:error, :truncated}
    end
  end

  defp decode_command(@op_print, <<text_len::little-16, rest::binary>>) do
    decode_print_text(:print, text_len, rest)
  end

  defp decode_command(@op_println, <<text_len::little-16, rest::binary>>) do
    decode_print_text(:println, text_len, rest)
  end

  defp decode_command(
         @op_push_sprite,
         <<source_target, x::little-signed-16, y::little-signed-16, rest::binary>>
       ) do
    if sprite_handle(source_target) do
      {:ok, %{op: :push_sprite, source_target: source_target, x: x, y: y}, rest}
    else
      {:error, {:bad_sprite_target, source_target}}
    end
  end

  defp decode_command(opcode, _rest) when opcode in @known_batch_opcodes do
    {:error, :truncated}
  end

  defp decode_command(_opcode, _rest) do
    {:error, :unsupported_command}
  end

  defp encode_draw_pixel_list_payload([]), do: {:error, :empty_batch}

  defp encode_draw_pixel_list_payload(pixels) when length(pixels) <= 0xFFFF do
    encode_draw_pixel_records(pixels, 0, [])
  end

  defp encode_draw_pixel_list_payload(pixels),
    do: {:error, {:too_many_records, :draw_pixel_list, length(pixels), 0xFFFF}}

  defp encode_draw_pixel_records([], count, acc) do
    {:ok, count, :erlang.iolist_to_binary(:lists.reverse(acc))}
  end

  defp encode_draw_pixel_records([{x, y, color} | rest], count, acc)
       when i16(x) and i16(y) and u16(color) do
    record = <<x::signed-little-16, y::signed-little-16, color::little-16>>

    encode_draw_pixel_records(rest, count + 1, [record | acc])
  end

  defp encode_draw_pixel_records([bad_pixel | _rest], _count, _acc) do
    {:error, {:bad_draw_pixel_record, bad_pixel}}
  end

  defp validate_draw_pixel_list_header(flags, count) do
    cond do
      flags != 0 ->
        {:error, {:bad_flags, flags}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp decode_draw_pixel_records(<<>>, acc), do: :lists.reverse(acc)

  defp decode_draw_pixel_records(
         <<x::little-signed-16, y::little-signed-16, color::little-16, rest::binary>>,
         acc
       ) do
    pixel = %{x: x, y: y, color: color}
    decode_draw_pixel_records(rest, [pixel | acc])
  end

  defp encode_draw_rect_list_payload([]), do: {:error, :empty_batch}

  defp encode_draw_rect_list_payload(rectangles) when length(rectangles) <= 0xFFFF do
    encode_draw_rect_records(rectangles, 0, [])
  end

  defp encode_draw_rect_list_payload(rectangles),
    do: {:error, {:too_many_records, :draw_rect_list, length(rectangles), 0xFFFF}}

  defp encode_draw_rect_records([], count, acc) do
    {:ok, count, :erlang.iolist_to_binary(:lists.reverse(acc))}
  end

  defp encode_draw_rect_records([{x, y, width, height, color} | rest], count, acc)
       when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 and
              u16(color) do
    record =
      <<x::signed-little-16, y::signed-little-16, width::little-16, height::little-16,
        color::little-16>>

    encode_draw_rect_records(rest, count + 1, [record | acc])
  end

  defp encode_draw_rect_records([bad_rectangle | _rest], _count, _acc) do
    {:error, {:bad_draw_rect_record, bad_rectangle}}
  end

  defp validate_draw_rect_list_header(flags, count) do
    cond do
      flags != 0 ->
        {:error, {:bad_flags, flags}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp decode_draw_rect_records(<<>>, acc), do: {:ok, :lists.reverse(acc)}

  defp decode_draw_rect_records(
         <<x::little-signed-16, y::little-signed-16, width::little-16, height::little-16,
           color::little-16, rest::binary>>,
         acc
       ) do
    cond do
      width == 0 ->
        {:error, {:bad_zero_value, :width}}

      height == 0 ->
        {:error, {:bad_zero_value, :height}}

      true ->
        rectangle = %{x: x, y: y, width: width, height: height, color: color}
        decode_draw_rect_records(rest, [rectangle | acc])
    end
  end

  defp encode_fill_rect_list_payload([]), do: {:error, :empty_batch}

  defp encode_fill_rect_list_payload(rectangles) when length(rectangles) <= 0xFFFF do
    encode_fill_rect_records(rectangles, 0, [])
  end

  defp encode_fill_rect_list_payload(rectangles),
    do: {:error, {:too_many_records, :fill_rect_list, length(rectangles), 0xFFFF}}

  defp encode_fill_rect_records([], count, acc) do
    {:ok, count, :erlang.iolist_to_binary(:lists.reverse(acc))}
  end

  defp encode_fill_rect_records([{x, y, width, height, color} | rest], count, acc)
       when i16(x) and i16(y) and u16(width) and width >= 1 and u16(height) and height >= 1 and
              u16(color) do
    record =
      <<x::signed-little-16, y::signed-little-16, width::little-16, height::little-16,
        color::little-16>>

    encode_fill_rect_records(rest, count + 1, [record | acc])
  end

  defp encode_fill_rect_records([bad_rectangle | _rest], _count, _acc) do
    {:error, {:bad_fill_rect_record, bad_rectangle}}
  end

  defp validate_fill_rect_list_header(flags, count) do
    cond do
      flags != 0 ->
        {:error, {:bad_flags, flags}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp decode_fill_rect_records(<<>>, acc), do: {:ok, :lists.reverse(acc)}

  defp decode_fill_rect_records(
         <<x::little-signed-16, y::little-signed-16, width::little-16, height::little-16,
           color::little-16, rest::binary>>,
         acc
       ) do
    cond do
      width == 0 ->
        {:error, {:bad_zero_value, :width}}

      height == 0 ->
        {:error, {:bad_zero_value, :height}}

      true ->
        rectangle = %{x: x, y: y, width: width, height: height, color: color}
        decode_fill_rect_records(rest, [rectangle | acc])
    end
  end

  defp encode_draw_circle_list_payload([]), do: {:error, :empty_batch}

  defp encode_draw_circle_list_payload(circles) when length(circles) <= 0xFFFF do
    encode_draw_circle_records(circles, 0, [])
  end

  defp encode_draw_circle_list_payload(circles),
    do: {:error, {:too_many_records, :draw_circle_list, length(circles), 0xFFFF}}

  defp encode_draw_circle_records([], count, acc) do
    {:ok, count, :erlang.iolist_to_binary(:lists.reverse(acc))}
  end

  defp encode_draw_circle_records([{x, y, radius, color} | rest], count, acc)
       when i16(x) and i16(y) and u16(radius) and radius >= 1 and u16(color) do
    record =
      <<x::signed-little-16, y::signed-little-16, radius::little-16, color::little-16>>

    encode_draw_circle_records(rest, count + 1, [record | acc])
  end

  defp encode_draw_circle_records([bad_circle | _rest], _count, _acc) do
    {:error, {:bad_draw_circle_record, bad_circle}}
  end

  defp validate_draw_circle_list_header(flags, count) do
    cond do
      flags != 0 ->
        {:error, {:bad_flags, flags}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp decode_draw_circle_records(<<>>, acc), do: {:ok, :lists.reverse(acc)}

  defp decode_draw_circle_records(
         <<x::little-signed-16, y::little-signed-16, radius::little-16, color::little-16,
           rest::binary>>,
         acc
       ) do
    cond do
      radius == 0 ->
        {:error, {:bad_zero_value, :radius}}

      true ->
        circle = %{x: x, y: y, radius: radius, color: color}
        decode_draw_circle_records(rest, [circle | acc])
    end
  end

  defp encode_fill_circle_list_payload([]), do: {:error, :empty_batch}

  defp encode_fill_circle_list_payload(circles) when length(circles) <= 0xFFFF do
    encode_fill_circle_records(circles, 0, [])
  end

  defp encode_fill_circle_list_payload(circles),
    do: {:error, {:too_many_records, :fill_circle_list, length(circles), 0xFFFF}}

  defp encode_fill_circle_records([], count, acc) do
    {:ok, count, :erlang.iolist_to_binary(:lists.reverse(acc))}
  end

  defp encode_fill_circle_records([{x, y, radius, color} | rest], count, acc)
       when i16(x) and i16(y) and u16(radius) and radius >= 1 and u16(color) do
    record =
      <<x::signed-little-16, y::signed-little-16, radius::little-16, color::little-16>>

    encode_fill_circle_records(rest, count + 1, [record | acc])
  end

  defp encode_fill_circle_records([bad_circle | _rest], _count, _acc) do
    {:error, {:bad_fill_circle_record, bad_circle}}
  end

  defp validate_fill_circle_list_header(flags, count) do
    cond do
      flags != 0 ->
        {:error, {:bad_flags, flags}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp decode_fill_circle_records(<<>>, acc), do: {:ok, :lists.reverse(acc)}

  defp decode_fill_circle_records(
         <<x::little-signed-16, y::little-signed-16, radius::little-16, color::little-16,
           rest::binary>>,
         acc
       ) do
    cond do
      radius == 0 ->
        {:error, {:bad_zero_value, :radius}}

      true ->
        circle = %{x: x, y: y, radius: radius, color: color}
        decode_fill_circle_records(rest, [circle | acc])
    end
  end

  defp encode_ellipse_list_payload(_op, []), do: {:error, :empty_batch}

  defp encode_ellipse_list_payload(op, ellipses) when length(ellipses) <= 0xFFFF do
    encode_ellipse_records(op, ellipses, 0, [])
  end

  defp encode_ellipse_list_payload(op, ellipses),
    do: {:error, {:too_many_records, op, length(ellipses), 0xFFFF}}

  defp encode_ellipse_records(_op, [], count, acc) do
    {:ok, count, :erlang.iolist_to_binary(:lists.reverse(acc))}
  end

  defp encode_ellipse_records(op, [{x, y, radius_x, radius_y, color} | rest], count, acc)
       when i16(x) and i16(y) and u16(radius_x) and radius_x >= 1 and u16(radius_y) and
              radius_y >= 1 and u16(color) do
    record =
      <<x::signed-little-16, y::signed-little-16, radius_x::little-16, radius_y::little-16,
        color::little-16>>

    encode_ellipse_records(op, rest, count + 1, [record | acc])
  end

  defp encode_ellipse_records(op, [bad_ellipse | _rest], _count, _acc) do
    {:error, {bad_ellipse_record_reason(op), bad_ellipse}}
  end

  defp bad_ellipse_record_reason(:draw_ellipse_list), do: :bad_draw_ellipse_record
  defp bad_ellipse_record_reason(:fill_ellipse_list), do: :bad_fill_ellipse_record

  defp validate_ellipse_list_header(kind, reserved, flags, count) do
    cond do
      kind not in [@ellipse_list_kind_draw, @ellipse_list_kind_fill] ->
        {:error, {:bad_ellipse_list_kind, kind}}

      reserved != 0 ->
        {:error, {:bad_reserved, reserved}}

      flags != 0 ->
        {:error, {:bad_flags, flags}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp decode_ellipse_records(<<>>, acc), do: {:ok, :lists.reverse(acc)}

  defp decode_ellipse_records(
         <<x::little-signed-16, y::little-signed-16, radius_x::little-16, radius_y::little-16,
           color::little-16, rest::binary>>,
         acc
       ) do
    cond do
      radius_x == 0 ->
        {:error, {:bad_zero_value, :radius_x}}

      radius_y == 0 ->
        {:error, {:bad_zero_value, :radius_y}}

      true ->
        ellipse = %{x: x, y: y, radius_x: radius_x, radius_y: radius_y, color: color}
        decode_ellipse_records(rest, [ellipse | acc])
    end
  end

  defp encode_draw_line_list_payload([]), do: {:error, :empty_batch}

  defp encode_draw_line_list_payload(lines) when length(lines) <= 0xFFFF do
    encode_draw_line_records(lines, 0, [])
  end

  defp encode_draw_line_list_payload(lines),
    do: {:error, {:too_many_records, :draw_line_list, length(lines), 0xFFFF}}

  defp encode_draw_line_records([], count, acc) do
    {:ok, count, :erlang.iolist_to_binary(:lists.reverse(acc))}
  end

  defp encode_draw_line_records([{x0, y0, x1, y1, color} | rest], count, acc)
       when i16(x0) and i16(y0) and i16(x1) and i16(y1) and u16(color) do
    record =
      <<x0::signed-little-16, y0::signed-little-16, x1::signed-little-16, y1::signed-little-16,
        color::little-16>>

    encode_draw_line_records(rest, count + 1, [record | acc])
  end

  defp encode_draw_line_records([bad_line | _rest], _count, _acc) do
    {:error, {:bad_draw_line_record, bad_line}}
  end

  defp validate_draw_line_list_header(flags, count) do
    cond do
      flags != 0 ->
        {:error, {:bad_flags, flags}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp decode_draw_line_records(<<>>, acc), do: :lists.reverse(acc)

  defp decode_draw_line_records(
         <<x0::little-signed-16, y0::little-signed-16, x1::little-signed-16, y1::little-signed-16,
           color::little-16, rest::binary>>,
         acc
       ) do
    line = %{x0: x0, y0: y0, x1: x1, y1: y1, color: color}
    decode_draw_line_records(rest, [line | acc])
  end

  defp encode_draw_triangle_list_payload([]), do: {:error, :empty_batch}

  defp encode_draw_triangle_list_payload(triangles) when length(triangles) <= 0xFFFF do
    encode_draw_triangle_records(triangles, 0, [])
  end

  defp encode_draw_triangle_list_payload(triangles),
    do: {:error, {:too_many_records, :draw_triangle_list, length(triangles), 0xFFFF}}

  defp encode_draw_triangle_records([], count, acc) do
    {:ok, count, :erlang.iolist_to_binary(:lists.reverse(acc))}
  end

  defp encode_draw_triangle_records([{x0, y0, x1, y1, x2, y2, color} | rest], count, acc)
       when i16(x0) and i16(y0) and i16(x1) and i16(y1) and i16(x2) and i16(y2) and
              u16(color) do
    record =
      <<x0::signed-little-16, y0::signed-little-16, x1::signed-little-16, y1::signed-little-16,
        x2::signed-little-16, y2::signed-little-16, color::little-16>>

    encode_draw_triangle_records(rest, count + 1, [record | acc])
  end

  defp encode_draw_triangle_records([bad_triangle | _rest], _count, _acc) do
    {:error, {:bad_draw_triangle_record, bad_triangle}}
  end

  defp validate_draw_triangle_list_header(flags, count) do
    cond do
      flags != 0 ->
        {:error, {:bad_flags, flags}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp decode_draw_triangle_records(<<>>, acc), do: :lists.reverse(acc)

  defp decode_draw_triangle_records(
         <<x0::little-signed-16, y0::little-signed-16, x1::little-signed-16, y1::little-signed-16,
           x2::little-signed-16, y2::little-signed-16, color::little-16, rest::binary>>,
         acc
       ) do
    triangle = %{x0: x0, y0: y0, x1: x1, y1: y1, x2: x2, y2: y2, color: color}
    decode_draw_triangle_records(rest, [triangle | acc])
  end

  defp encode_fill_triangle_list_payload([]), do: {:error, :empty_batch}

  defp encode_fill_triangle_list_payload(triangles) when length(triangles) <= 0xFFFF do
    encode_fill_triangle_records(triangles, 0, [])
  end

  defp encode_fill_triangle_list_payload(triangles),
    do: {:error, {:too_many_records, :fill_triangle_list, length(triangles), 0xFFFF}}

  defp encode_fill_triangle_records([], count, acc) do
    {:ok, count, :erlang.iolist_to_binary(:lists.reverse(acc))}
  end

  defp encode_fill_triangle_records([{x0, y0, x1, y1, x2, y2, color} | rest], count, acc)
       when i16(x0) and i16(y0) and i16(x1) and i16(y1) and i16(x2) and i16(y2) and
              u16(color) do
    record =
      <<x0::signed-little-16, y0::signed-little-16, x1::signed-little-16, y1::signed-little-16,
        x2::signed-little-16, y2::signed-little-16, color::little-16>>

    encode_fill_triangle_records(rest, count + 1, [record | acc])
  end

  defp encode_fill_triangle_records([bad_triangle | _rest], _count, _acc) do
    {:error, {:bad_fill_triangle_record, bad_triangle}}
  end

  defp validate_fill_triangle_list_header(flags, count) do
    cond do
      flags != 0 ->
        {:error, {:bad_flags, flags}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp decode_fill_triangle_records(<<>>, acc), do: :lists.reverse(acc)

  defp decode_fill_triangle_records(
         <<x0::little-signed-16, y0::little-signed-16, x1::little-signed-16, y1::little-signed-16,
           x2::little-signed-16, y2::little-signed-16, color::little-16, rest::binary>>,
         acc
       ) do
    triangle = %{x0: x0, y0: y0, x1: x1, y1: y1, x2: x2, y2: y2, color: color}
    decode_fill_triangle_records(rest, [triangle | acc])
  end

  defp validate_jpeg_payload(jpeg) do
    jpeg_len = byte_size(jpeg)

    cond do
      jpeg_len == 0 ->
        {:error, :empty_jpeg}

      jpeg_len > 0xFFFFFFFF ->
        {:error, {:binary_too_large, :draw_jpg, jpeg_len, 0xFFFFFFFF}}

      true ->
        {:ok, jpeg_len}
    end
  end

  defp decode_jpeg_payload(command, jpeg_len, rest) do
    case rest do
      <<_jpeg::binary-size(jpeg_len), remaining::binary>> when jpeg_len > 0 ->
        {:ok, command, remaining}

      <<_jpeg::binary-size(jpeg_len), _remaining::binary>> ->
        {:error, :empty_jpeg}

      _ ->
        {:error, :truncated}
    end
  end

  defp normalize_image_scale(value)
       when is_integer(value) and value > 0 and value <= @max_f32 do
    {:ok, value * 1.0}
  end

  defp normalize_image_scale(value) when is_float(value) and value > 0 and value <= @max_f32 do
    {:ok, value}
  end

  defp normalize_image_scale(value), do: {:error, {:bad_image_scale, value}}

  defp validate_image_scale(value) when is_float(value) and value > 0 and value <= @max_f32,
    do: :ok

  defp validate_image_scale(value), do: {:error, {:bad_image_scale, value}}

  defp validate_push_image_rgb565_payload(width, height, pixels, stride_pixels) do
    pixels_len = byte_size(pixels)
    stride = if stride_pixels == 0, do: width, else: stride_pixels

    cond do
      width == 0 or height == 0 ->
        {:error, {:bad_image_dimensions, width, height}}

      stride < width ->
        {:error, {:bad_stride, stride_pixels, width}}

      rem(pixels_len, 2) != 0 ->
        {:error, {:pixels_size_not_even, pixels_len}}

      pixels_len > 0xFFFFFFFF ->
        {:error, {:binary_too_large, :push_image, pixels_len, 0xFFFFFFFF}}

      pixels_len < stride * height * 2 ->
        {:error, {:pixels_size_too_small, stride * height * 2, pixels_len}}

      true ->
        {:ok, pixels_len}
    end
  end

  defp encode_push_rotate_zoom_command(
         source_target,
         x,
         y,
         angle_deg,
         zoom_x,
         zoom_y,
         transparent
       ) do
    with {:ok, angle_deg} <- normalize_angle(angle_deg),
         {:ok, zoom_x} <- normalize_image_scale(zoom_x),
         {:ok, zoom_y} <- normalize_image_scale(zoom_y),
         {:ok, flags, options, transparent_value} <-
           normalize_push_rotate_zoom_options(transparent) do
      <<@op_push_rotate_zoom, options, 0, flags::little-16, source_target, 0, x::signed-little-16,
        y::signed-little-16, angle_deg::little-float-32, zoom_x::little-float-32,
        zoom_y::little-float-32, transparent_value::little-16>>
    else
      {:error, reason} -> raise ArgumentError, Errors.format_error(reason)
    end
  end

  defp normalize_push_rotate_zoom_options(nil), do: {:ok, 0, 0, 0}

  defp normalize_push_rotate_zoom_options(transparent) do
    with {:ok, flags, transparent_value} <- normalize_transparent_arg(transparent) do
      {:ok, flags, 0x01, transparent_value}
    end
  end

  defp encode_print_command(opcode, op, text) do
    case validate_print_text(op, text) do
      :ok ->
        text_len = byte_size(text)
        <<opcode, text_len::little-16, text::binary>>

      {:error, reason} ->
        raise ArgumentError, Errors.format_error(reason)
    end
  end

  defp decode_print_text(op, text_len, rest) do
    case rest do
      <<text::binary-size(text_len), remaining::binary>> ->
        if contains_nul?(text) do
          {:error, :text_contains_nul}
        else
          {:ok, %{op: op, text: text, text_len: text_len}, remaining}
        end

      _ ->
        {:error, :truncated}
    end
  end

  defp encode_arc_command(opcode, x, y, radius0, radius1, angle0, angle1, color) do
    with {:ok, angle0} <- normalize_angle(angle0),
         {:ok, angle1} <- normalize_angle(angle1) do
      <<opcode, x::signed-little-16, y::signed-little-16, radius0::little-16, radius1::little-16,
        angle0::little-float-32, angle1::little-float-32, color::little-16>>
    else
      {:error, reason} -> raise ArgumentError, Errors.format_error(reason)
    end
  end

  defp normalize_angle(value)
       when is_integer(value) and value >= -@max_f32 and value <= @max_f32 do
    {:ok, value * 1.0}
  end

  defp normalize_angle(value) when is_float(value) and value >= -@max_f32 and value <= @max_f32 do
    {:ok, value}
  end

  defp normalize_angle(value), do: {:error, {:bad_angle, value}}

  defp validate_angle(value) when is_float(value) and value >= -@max_f32 and value <= @max_f32,
    do: :ok

  defp validate_angle(value), do: {:error, {:bad_angle, value}}

  defp validate_non_zero(0, field), do: {:error, {:bad_zero_value, field}}
  defp validate_non_zero(_value, _field), do: :ok

  defp decode_text_color_rest(flags, fg, rest, true, bg_is_index) do
    case rest do
      <<bg::little-16, remaining::binary>> ->
        if bg_is_index and bg > 0xFF do
          {:error, {:bad_text_color, :bg, {:index, bg}}}
        else
          fg_color = decode_color_by_flag(flags, Protocol.text_fg_index_flag(), fg)
          bg_color = decode_color_by_flag(flags, Protocol.text_bg_index_flag(), bg)
          {:ok, %{op: :set_text_color, flags: flags, fg: fg_color, bg: bg_color}, remaining}
        end

      _ ->
        {:error, :truncated}
    end
  end

  defp decode_text_color_rest(flags, fg, rest, false, _bg_is_index) do
    fg_color = decode_color_by_flag(flags, Protocol.text_fg_index_flag(), fg)
    {:ok, %{op: :set_text_color, flags: flags, fg: fg_color}, rest}
  end

  defp validate_push_sprite_list_header(flags, transparent, count) do
    allowed_flags = @sprite_list_flag_has_transparent ||| Protocol.transparent_index_flag()
    has_transparent = flag_set?(flags, @sprite_list_flag_has_transparent)
    transparent_is_index = flag_set?(flags, Protocol.transparent_index_flag())

    cond do
      (flags &&& bnot(allowed_flags)) != 0 ->
        {:error, {:bad_flags, flags}}

      not has_transparent and transparent != 0 ->
        {:error, {:bad_transparent_color, transparent}}

      transparent_is_index and not has_transparent ->
        {:error, {:bad_flags, flags}}

      transparent_is_index and transparent > 0xFF ->
        {:error, {:bad_transparent_color, {:index, transparent}}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp validate_push_sprite_region_list_header(flags, transparent, count) do
    cond do
      (flags &&& bnot(@sprite_region_list_flag_has_transparent)) != 0 ->
        {:error, {:bad_flags, flags}}

      not flag_set?(flags, @sprite_region_list_flag_has_transparent) and transparent != 0 ->
        {:error, {:bad_transparent_color, transparent}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp validate_push_rotate_zoom_header(flags, options, transparent) do
    has_transparent = flag_set?(options, 0x01)
    transparent_is_index = flag_set?(flags, Protocol.transparent_index_flag())

    cond do
      (flags &&& bnot(Protocol.transparent_index_flag())) != 0 ->
        {:error, {:bad_flags, flags}}

      (options &&& bnot(0x01)) != 0 ->
        {:error, {:bad_options, options}}

      not has_transparent and transparent != 0 ->
        {:error, {:bad_transparent_color, transparent}}

      transparent_is_index and not has_transparent ->
        {:error, {:bad_flags, flags}}

      transparent_is_index and transparent > 0xFF ->
        {:error, {:bad_transparent_color, {:index, transparent}}}

      true ->
        :ok
    end
  end

  defp validate_push_rotate_zoom_list_header(flags, options, transparent, count) do
    has_transparent = flag_set?(options, 0x01)
    transparent_is_index = flag_set?(flags, Protocol.transparent_index_flag())

    cond do
      (flags &&& bnot(Protocol.transparent_index_flag())) != 0 ->
        {:error, {:bad_flags, flags}}

      (options &&& bnot(0x03)) != 0 ->
        {:error, {:bad_options, options}}

      not has_transparent and transparent != 0 ->
        {:error, {:bad_transparent_color, transparent}}

      transparent_is_index and not has_transparent ->
        {:error, {:bad_flags, flags}}

      transparent_is_index and transparent > 0xFF ->
        {:error, {:bad_transparent_color, {:index, transparent}}}

      count == 0 ->
        {:error, :empty_batch}

      true ->
        :ok
    end
  end

  defp decode_push_sprite_records(<<>>, acc), do: {:ok, :lists.reverse(acc)}

  defp decode_push_sprite_records(
         <<source_target, 0, x::little-signed-16, y::little-signed-16, rest::binary>>,
         acc
       ) do
    if sprite_handle(source_target) do
      instance = %{source_target: source_target, x: x, y: y}
      decode_push_sprite_records(rest, [instance | acc])
    else
      {:error, {:bad_sprite_target, source_target}}
    end
  end

  defp decode_push_sprite_records(<<_source_target, reserved, _rest::binary>>, _acc)
       when reserved != 0 do
    {:error, {:bad_reserved, reserved}}
  end

  defp decode_push_sprite_records(_truncated, _acc), do: {:error, :truncated}

  defp decode_push_sprite_region_records(<<>>, acc), do: {:ok, :lists.reverse(acc)}

  defp decode_push_sprite_region_records(
         <<source_target, 0, src_x::little-16, src_y::little-16, src_w::little-16,
           src_h::little-16, dst_x::little-signed-16, dst_y::little-signed-16, rest::binary>>,
         acc
       ) do
    cond do
      not sprite_handle(source_target) ->
        {:error, {:bad_sprite_target, source_target}}

      src_w == 0 or src_h == 0 ->
        {:error, {:bad_sprite_region, src_w, src_h}}

      true ->
        instance = %{
          source_target: source_target,
          src_x: src_x,
          src_y: src_y,
          src_w: src_w,
          src_h: src_h,
          dst_x: dst_x,
          dst_y: dst_y
        }

        decode_push_sprite_region_records(rest, [instance | acc])
    end
  end

  defp decode_push_sprite_region_records(<<_source_target, reserved, _rest::binary>>, _acc)
       when reserved != 0 do
    {:error, {:bad_reserved, reserved}}
  end

  defp decode_push_sprite_region_records(_truncated, _acc), do: {:error, :truncated}

  defp decode_push_rotate_zoom_records(<<>>, acc), do: {:ok, :lists.reverse(acc)}

  defp decode_push_rotate_zoom_records(
         <<source_target, 0, x::little-signed-16, y::little-signed-16, angle_cdeg::little-16,
           zoom_x1024::little-16, zoom_y1024::little-16, rest::binary>>,
         acc
       ) do
    cond do
      not sprite_handle(source_target) ->
        {:error, {:bad_sprite_target, source_target}}

      angle_cdeg >= 36_000 ->
        {:error, {:bad_angle_cdeg, angle_cdeg}}

      zoom_x1024 == 0 or zoom_y1024 == 0 ->
        {:error, {:bad_zoom_x1024, zoom_x1024, zoom_y1024}}

      true ->
        instance = %{
          source_target: source_target,
          x: x,
          y: y,
          angle_cdeg: angle_cdeg,
          zoom_x1024: zoom_x1024,
          zoom_y1024: zoom_y1024
        }

        decode_push_rotate_zoom_records(rest, [instance | acc])
    end
  end

  defp decode_push_rotate_zoom_records(<<_source_target, reserved, _rest::binary>>, _acc)
       when reserved != 0 do
    {:error, {:bad_reserved, reserved}}
  end

  defp decode_push_rotate_zoom_records(_truncated, _acc), do: {:error, :truncated}

  defp decode_color_by_flag(flags, index_flag, value) do
    if flag_set?(flags, index_flag) do
      {:index, value}
    else
      {:rgb565, value}
    end
  end

  defp decode_transparent_by_flags(flags, value) do
    decode_color_by_flag(flags, Protocol.transparent_index_flag(), value)
  end

  defp decode_push_sprite_list_transparent(flags, value) do
    if flag_set?(flags, @sprite_list_flag_has_transparent) do
      decode_transparent_by_flags(flags, value)
    else
      nil
    end
  end

  defp decode_push_sprite_region_list_transparent(flags, value) do
    if flag_set?(flags, @sprite_region_list_flag_has_transparent) do
      {:rgb565, value}
    else
      nil
    end
  end

  defp decode_push_rotate_zoom_transparent(flags, options, value) do
    if flag_set?(options, 0x01) do
      decode_transparent_by_flags(flags, value)
    else
      nil
    end
  end

  defp decode_push_rotate_zoom_list_transparent(flags, options, value) do
    if flag_set?(options, 0x01) do
      decode_transparent_by_flags(flags, value)
    else
      nil
    end
  end

  defp encode_bool(false), do: 0
  defp encode_bool(true), do: 1

  defp decode_bool(0), do: false
  defp decode_bool(1), do: true

  defp flag_set?(flags, flag) do
    (flags &&& flag) != 0
  end

  defp encode_push_sprite_list_payload(instances, opts) do
    with {:ok, flags, transparent_value} <- normalize_sprite_list_options(opts),
         {:ok, count, records} <- encode_push_sprite_list_records(instances) do
      {:ok, flags, transparent_value, count, records}
    end
  end

  defp normalize_sprite_list_options(opts) do
    case Keyword.fetch(opts, :transparent) do
      {:ok, transparent} ->
        case normalize_transparent_arg(transparent) do
          {:ok, flags, transparent_value} ->
            {:ok, @sprite_list_flag_has_transparent ||| flags, transparent_value}

          {:error, reason} ->
            {:error, reason}
        end

      :error ->
        {:ok, 0, 0}
    end
  end

  defp encode_push_sprite_list_records(instances) do
    case encode_push_sprite_list_records(instances, 0, []) do
      {:ok, 0, _records} -> {:error, :empty_batch}
      other -> other
    end
  end

  defp encode_push_sprite_list_records([], count, acc) do
    {:ok, count, :erlang.iolist_to_binary(:lists.reverse(acc))}
  end

  defp encode_push_sprite_list_records([{source_target, x, y} | rest], count, acc)
       when count < 0xFFFF and sprite_handle(source_target) and i16(x) and i16(y) do
    record = <<source_target, 0, x::signed-little-16, y::signed-little-16>>
    encode_push_sprite_list_records(rest, count + 1, [record | acc])
  end

  defp encode_push_sprite_list_records([_instance | _rest], count, _acc) when count >= 0xFFFF do
    {:error, {:binary_too_large, :push_sprite_list, count + 1, 0xFFFF}}
  end

  defp encode_push_sprite_list_records([bad | _rest], _count, _acc) do
    {:error, {:bad_push_sprite_list_instance, bad}}
  end

  defp encode_push_sprite_region_list_payload(instances, opts) do
    with {:ok, flags, transparent_value} <- normalize_sprite_region_list_options(opts),
         {:ok, count, records} <- encode_push_sprite_region_list_records(instances) do
      {:ok, flags, transparent_value, count, records}
    end
  end

  defp normalize_sprite_region_list_options(opts) do
    case Keyword.fetch(opts, :transparent) do
      {:ok, transparent} ->
        case normalize_rgb565_transparent_arg(transparent) do
          {:ok, transparent_value} ->
            {:ok, @sprite_region_list_flag_has_transparent, transparent_value}

          {:error, reason} ->
            {:error, reason}
        end

      :error ->
        {:ok, 0, 0}
    end
  end

  defp normalize_rgb565_transparent_arg({:rgb565, value}) when u16(value), do: {:ok, value}
  defp normalize_rgb565_transparent_arg(value) when u16(value), do: {:ok, value}
  defp normalize_rgb565_transparent_arg({:index, _value}), do: {:error, :bad_transparent}
  defp normalize_rgb565_transparent_arg(_value), do: {:error, :bad_transparent}

  defp encode_push_sprite_region_list_records(instances) do
    case encode_push_sprite_region_list_records(instances, 0, []) do
      {:ok, 0, _records} -> {:error, :empty_batch}
      other -> other
    end
  end

  defp encode_push_sprite_region_list_records([], count, acc) do
    {:ok, count, :erlang.iolist_to_binary(:lists.reverse(acc))}
  end

  defp encode_push_sprite_region_list_records(
         [{source_target, src_x, src_y, src_w, src_h, dst_x, dst_y} | rest],
         count,
         acc
       )
       when count < 0xFFFF and sprite_handle(source_target) and u16(src_x) and u16(src_y) and
              u16(src_w) and src_w >= 1 and u16(src_h) and src_h >= 1 and i16(dst_x) and
              i16(dst_y) do
    record =
      <<source_target, 0, src_x::little-16, src_y::little-16, src_w::little-16, src_h::little-16,
        dst_x::signed-little-16, dst_y::signed-little-16>>

    encode_push_sprite_region_list_records(rest, count + 1, [record | acc])
  end

  defp encode_push_sprite_region_list_records([_instance | _rest], count, _acc)
       when count >= 0xFFFF do
    {:error, {:binary_too_large, :push_sprite_region_list, count + 1, 0xFFFF}}
  end

  defp encode_push_sprite_region_list_records([bad | _rest], _count, _acc) do
    {:error, {:bad_push_sprite_region_list_instance, bad}}
  end

  defp normalize_text_scale_x1024(value) when is_number(value) and value > 0 do
    scaled = round(value * @text_scale_factor)

    if scaled >= 1 and scaled <= @max_text_scale_x1024 do
      {:ok, scaled}
    else
      {:error, {:bad_text_scale, value}}
    end
  end

  defp normalize_text_color_args(fg_color, nil) do
    with {:ok, fg_flags, fg_arg} <-
           normalize_text_color_arg(fg_color, Protocol.text_fg_index_flag(), :fg) do
      {:ok, fg_flags, [fg_arg]}
    end
  end

  defp normalize_text_color_args(fg_color, bg_color) do
    with {:ok, fg_flags, fg_arg} <-
           normalize_text_color_arg(fg_color, Protocol.text_fg_index_flag(), :fg),
         {:ok, bg_flags, bg_arg} <-
           normalize_text_color_arg(bg_color, Protocol.text_bg_index_flag(), :bg) do
      flags = Protocol.text_has_bg_flag() ||| fg_flags ||| bg_flags
      {:ok, flags, [fg_arg, bg_arg]}
    end
  end

  defp normalize_text_color_arg(color, _index_flag, _role) when rgb565(color) do
    {:ok, 0, color}
  end

  defp normalize_text_color_arg({:rgb565, color}, _index_flag, _role) when rgb565(color) do
    {:ok, 0, color}
  end

  defp normalize_text_color_arg({:index, index}, index_flag, _role) when palette_index(index) do
    {:ok, index_flag, index}
  end

  defp normalize_text_color_arg(other, _index_flag, role),
    do: {:error, {:bad_text_color, role, other}}

  defp validate_render_text(<<>>), do: {:error, :empty_text}

  defp validate_render_text(text) when is_binary(text) do
    validate_text_payload(:draw_string, text)
  end

  defp validate_print_text(op, text) when is_binary(text) do
    validate_text_payload(op, text)
  end

  defp validate_text_payload(op, text) when is_binary(text) do
    cond do
      byte_size(text) > 0xFFFF ->
        {:error, {:binary_too_large, op, byte_size(text), 0xFFFF}}

      contains_nul?(text) ->
        {:error, :text_contains_nul}

      true ->
        :ok
    end
  end

  defp contains_nul?(text) when is_binary(text) do
    :binary.match(text, <<0>>) != :nomatch
  end

  defp normalize_transparent_arg(transparent) when rgb565(transparent) do
    {:ok, 0, transparent}
  end

  defp normalize_transparent_arg({:rgb565, transparent}) when rgb565(transparent) do
    {:ok, 0, transparent}
  end

  defp normalize_transparent_arg({:index, transparent_index})
       when palette_index(transparent_index) do
    {:ok, Protocol.transparent_index_flag(), transparent_index}
  end

  defp normalize_transparent_arg(other), do: {:error, {:bad_transparent_color, other}}
end
