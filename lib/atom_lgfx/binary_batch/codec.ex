# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatch.Codec do
  @moduledoc false
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
  @op_set_clip_rect OpSchema.opcode!(:set_clip_rect)
  @op_clear_clip_rect OpSchema.opcode!(:clear_clip_rect)
  @op_display OpSchema.opcode!(:display)
  @op_set_palette_color OpSchema.opcode!(:set_palette_color)
  @op_set_pivot OpSchema.opcode!(:set_pivot)
  @op_push_sprite OpSchema.opcode!(:push_sprite)
  @op_push_rotate_zoom OpSchema.opcode!(:push_rotate_zoom)
  @op_push_rotate_zoom_list OpSchema.opcode!(:push_rotate_zoom_list)

  @render_op_target 0xF0
  @render_op_color_mode 0xF1
  @render_op_push_sprite_transparent 0xF2

  @render_private_opcodes [
    target: @render_op_target,
    color_mode: @render_op_color_mode,
    push_sprite_transparent: @render_op_push_sprite_transparent
  ]

  @render_extended_opcodes []

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

  @doc false
  @spec __render_private_opcodes__() :: [{atom(), byte()}]
  def __render_private_opcodes__ do
    @render_private_opcodes
  end

  @doc false
  @spec __render_extended_opcodes__() :: [{atom(), byte()}]
  def __render_extended_opcodes__ do
    @render_extended_opcodes
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
  Sets one palette color on a palette-backed sprite target.
  """
  @spec set_palette_color(integer(), integer()) :: binary()
  def set_palette_color(palette_index, rgb888)
      when palette_index(palette_index) and color888(rgb888) do
    <<@op_set_palette_color, palette_index, 0, rgb888::little-32>>
  end

  @doc """
  Sets the sprite pivot used by transformed sprite drawing.
  """
  @spec set_pivot(integer(), integer()) :: binary()
  def set_pivot(x, y) when i16(x) and i16(y) do
    <<@op_set_pivot, x::signed-little-16, y::signed-little-16>>
  end

  @doc """
  Pushes a source sprite to the current render target.
  """
  @spec push_sprite(integer(), integer(), integer()) :: binary()
  def push_sprite(source_target, x, y) when sprite_handle(source_target) and i16(x) and i16(y) do
    <<@op_push_sprite, source_target, x::signed-little-16, y::signed-little-16>>
  end

  @doc """
  Pushes a source sprite to the current render target using a transparent key.

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
  Pushes a transformed source sprite with uniform zoom.

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
      decode_commands(command_binary, 0, [])
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

  @doc false
  @spec decode_with_partial(iodata()) :: {:ok, [map()]} | {:error, term(), [map()]}
  def decode_with_partial(commands) do
    command_binary = batch(commands)

    if command_binary == <<>> do
      {:error, :empty_batch, []}
    else
      decode_commands_with_partial(command_binary, 0, [])
    end
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
