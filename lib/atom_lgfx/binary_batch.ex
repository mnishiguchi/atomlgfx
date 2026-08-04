# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatch do
  @moduledoc """
  Compatibility facade for packed binary-batch command builders.

  `AtomLGFX.BinaryBatch` remains the public low-level API for callers that need
  exact wire-level control. The implementation lives in smaller internal
  modules so the public module can stay stable while command encoding,
  decoding, submission, validation, and diagnostics are simplified behind it.
  """

  alias AtomLGFX.BinaryBatch.Codec
  alias AtomLGFX.BinaryBatch.Diagnostics
  alias AtomLGFX.BinaryBatch.Submission
  alias AtomLGFX.BinaryBatch.Validation

  @doc false
  @spec __render_private_opcodes__() :: [{atom(), byte()}]
  defdelegate __render_private_opcodes__(), to: Codec

  @doc false
  @spec __render_extended_opcodes__() :: [{atom(), byte()}]
  defdelegate __render_extended_opcodes__(), to: Codec

  @doc false
  @spec __known_batch_opcodes__() :: [byte()]
  defdelegate __known_batch_opcodes__(), to: Codec

  @doc """
  Combines packed binary command fragments into one command stream.
  """
  @spec batch(iodata()) :: binary()
  defdelegate batch(commands), to: Codec

  @doc """
  Submits a binary batch command stream.
  """
  @spec render(port(), iodata()) :: :ok | {:error, term()}
  defdelegate render(port, commands), to: Submission

  @doc """
  Validates and then submits a binary batch command stream.
  """
  @spec render_checked(port(), iodata()) :: :ok | {:error, term()}
  defdelegate render_checked(port, commands), to: Submission

  @doc """
  Validates a binary-batch command stream without submitting it.
  """
  @spec validate(iodata()) :: :ok | {:error, term()}
  defdelegate validate(commands), to: Validation

  @doc """
  Validates a binary-batch command stream or raises `ArgumentError`.
  """
  @spec validate!(iodata()) :: :ok
  defdelegate validate!(commands), to: Validation

  @doc """
  Selects the current render target for following binary-batch commands.
  """
  @spec target(integer()) :: binary()
  defdelegate target(target), to: Codec

  @doc """
  Selects how following scalar color fields are interpreted.
  """
  @spec color_mode(:rgb565 | :palette_index) :: binary()
  defdelegate color_mode(mode), to: Codec

  @doc """
  Presents the current frame.
  """
  @spec display() :: binary()
  defdelegate display(), to: Codec

  @doc "Encodes a command that fills the current target with a color."
  @spec fill_screen(integer()) :: binary()
  defdelegate fill_screen(color), to: Codec

  @doc "Encodes a command that clears the current target with a color."
  @spec clear(integer()) :: binary()
  defdelegate clear(color), to: Codec

  @doc "Encodes a command that draws one pixel on the current target."
  @spec draw_pixel(integer(), integer(), integer()) :: binary()
  defdelegate draw_pixel(x, y, color), to: Codec

  @doc "Encodes a command that draws a fast vertical line."
  @spec draw_fast_vline(integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_fast_vline(x, y, height, color), to: Codec

  @doc "Encodes a command that draws a fast horizontal line."
  @spec draw_fast_hline(integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_fast_hline(x, y, width, color), to: Codec

  @doc "Encodes a command that draws a line."
  @spec draw_line(integer(), integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_line(x0, y0, x1, y1, color), to: Codec

  @doc "Encodes a command that draws a rectangle outline."
  @spec draw_rect(integer(), integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_rect(x, y, width, height, color), to: Codec

  @doc "Encodes a command that fills a rectangle."
  @spec fill_rect(integer(), integer(), integer(), integer(), integer()) :: binary()
  defdelegate fill_rect(x, y, width, height, color), to: Codec

  @doc "Encodes a command that draws a rounded rectangle outline."
  @spec draw_round_rect(integer(), integer(), integer(), integer(), integer(), integer()) ::
          binary()
  defdelegate draw_round_rect(x, y, width, height, radius, color), to: Codec

  @doc "Encodes a command that fills a rounded rectangle."
  @spec fill_round_rect(integer(), integer(), integer(), integer(), integer(), integer()) ::
          binary()
  defdelegate fill_round_rect(x, y, width, height, radius, color), to: Codec

  @doc "Encodes a command that draws a circle outline."
  @spec draw_circle(integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_circle(x, y, radius, color), to: Codec

  @doc "Encodes a command that fills a circle."
  @spec fill_circle(integer(), integer(), integer(), integer()) :: binary()
  defdelegate fill_circle(x, y, radius, color), to: Codec

  @doc "Encodes a command that draws an ellipse outline."
  @spec draw_ellipse(integer(), integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_ellipse(x, y, radius_x, radius_y, color), to: Codec

  @doc "Encodes a command that fills an ellipse."
  @spec fill_ellipse(integer(), integer(), integer(), integer(), integer()) :: binary()
  defdelegate fill_ellipse(x, y, radius_x, radius_y, color), to: Codec

  @doc "Encodes a command that draws an arc outline."
  @spec draw_arc(integer(), integer(), integer(), integer(), number(), number(), integer()) ::
          binary()
  defdelegate draw_arc(x, y, radius0, radius1, angle0, angle1, color), to: Codec

  @doc "Encodes a command that fills an arc."
  @spec fill_arc(integer(), integer(), integer(), integer(), number(), number(), integer()) ::
          binary()
  defdelegate fill_arc(x, y, radius0, radius1, angle0, angle1, color), to: Codec

  @doc "Encodes a command that draws a quadratic Bezier curve."
  @spec draw_bezier(
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer()
        ) :: binary()
  defdelegate draw_bezier(x0, y0, x1, y1, x2, y2, color), to: Codec

  @doc "Encodes a command that draws a cubic Bezier curve."
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
  defdelegate draw_bezier(x0, y0, x1, y1, x2, y2, x3, y3, color), to: Codec

  @doc "Encodes a command that draws a triangle outline."
  @spec draw_triangle(integer(), integer(), integer(), integer(), integer(), integer(), integer()) ::
          binary()
  defdelegate draw_triangle(x0, y0, x1, y1, x2, y2, color), to: Codec

  @doc "Encodes a command that fills a triangle."
  @spec fill_triangle(integer(), integer(), integer(), integer(), integer(), integer(), integer()) ::
          binary()
  defdelegate fill_triangle(x0, y0, x1, y1, x2, y2, color), to: Codec

  @doc """
  Sets a clip rectangle on the current render target.
  """
  @spec set_clip_rect(integer(), integer(), integer(), integer()) :: binary()
  defdelegate set_clip_rect(x, y, width, height), to: Codec

  @doc """
  Clears the clip rectangle on the current render target.
  """
  @spec clear_clip_rect() :: binary()
  defdelegate clear_clip_rect(), to: Codec

  @doc """
  Selects a text font preset for the current render target.
  """
  @spec set_text_font_preset(:ascii | :jp) :: binary()
  defdelegate set_text_font_preset(preset), to: Codec

  @doc """
  Sets text size for the current render target.
  """
  @spec set_text_size(number()) :: binary()
  defdelegate set_text_size(scale), to: Codec

  @doc """
  Sets independent X/Y text size for the current render target.
  """
  @spec set_text_size_xy(number(), number()) :: binary()
  defdelegate set_text_size_xy(scale_x, scale_y), to: Codec

  @doc """
  Sets text datum for the current render target.
  """
  @spec set_text_datum(non_neg_integer()) :: binary()
  defdelegate set_text_datum(datum), to: Codec

  @doc """
  Sets text wrapping for the current render target.
  """
  @spec set_text_wrap(boolean()) :: binary()
  defdelegate set_text_wrap(wrap), to: Codec

  @doc """
  Sets independent X/Y text wrapping for the current render target.
  """
  @spec set_text_wrap_xy(boolean(), boolean()) :: binary()
  defdelegate set_text_wrap_xy(wrap_x, wrap_y), to: Codec

  @doc """
  Sets text cursor position for the current render target.
  """
  @spec set_cursor(integer(), integer()) :: binary()
  defdelegate set_cursor(x, y), to: Codec

  @doc """
  Sets text foreground color, optionally with a background color.
  """
  @spec set_text_color(integer() | {:rgb565, integer()} | {:index, integer()}) :: binary()
  defdelegate set_text_color(fg_color), to: Codec

  @doc "Sets text foreground and optional background colors for the current target."
  @spec set_text_color(
          integer() | {:rgb565, integer()} | {:index, integer()},
          nil | integer() | {:rgb565, integer()} | {:index, integer()}
        ) :: binary()
  defdelegate set_text_color(fg_color, bg_color), to: Codec

  @doc """
  Draws text at an absolute position on the current render target.
  """
  @spec draw_string(integer(), integer(), binary()) :: binary()
  defdelegate draw_string(x, y, text), to: Codec

  @doc """
  Prints text at the current cursor position.
  """
  @spec print(binary()) :: binary()
  defdelegate print(text), to: Codec

  @doc """
  Prints text plus a newline at the current cursor position.
  """
  @spec println() :: binary()
  defdelegate println(), to: Codec

  @doc "Prints text plus a newline at the current cursor position."
  @spec println(binary()) :: binary()
  defdelegate println(text), to: Codec

  @doc """
  Sets a palette entry on the current paletted sprite target.
  """
  @spec set_palette_color(integer(), integer()) :: binary()
  defdelegate set_palette_color(palette_index, rgb888), to: Codec

  @doc """
  Sets the pivot for the current render target.
  """
  @spec set_pivot(integer(), integer()) :: binary()
  defdelegate set_pivot(x, y), to: Codec

  @doc """
  Pushes a sprite target onto the current render target.
  """
  @spec push_sprite(integer(), integer(), integer()) :: binary()
  defdelegate push_sprite(source_target, x, y), to: Codec

  @doc """
  Pushes a sprite target with a transparent color onto the current render target.
  """
  @spec push_sprite(
          integer(),
          integer(),
          integer(),
          integer() | {:rgb565, integer()} | {:index, integer()}
        ) ::
          binary()
  defdelegate push_sprite(source_target, x, y, transparent), to: Codec

  @doc """
  Pushes a sprite target using rotate/zoom options.
  """
  @spec push_rotate_zoom(integer(), integer(), integer(), number(), number()) :: binary()
  defdelegate push_rotate_zoom(source_target, x, y, angle_deg, zoom), to: Codec

  @doc "Pushes a sprite target using independent X/Y zoom values."
  @spec push_rotate_zoom(integer(), integer(), integer(), number(), number(), number()) ::
          binary()
  defdelegate push_rotate_zoom(source_target, x, y, angle_deg, zoom_x, zoom_y), to: Codec

  @doc "Pushes a sprite target using rotate/zoom values and a transparent color."
  @spec push_rotate_zoom(
          integer(),
          integer(),
          integer(),
          number(),
          number(),
          number(),
          integer() | {:rgb565, integer()} | {:index, integer()}
        ) :: binary()
  defdelegate push_rotate_zoom(source_target, x, y, angle_deg, zoom_x, zoom_y, transparent),
    to: Codec

  @doc """
  Pushes many sprites using one packed rotate/zoom command.
  """
  @spec push_rotate_zoom_list(list()) :: binary()
  defdelegate push_rotate_zoom_list(instances), to: Codec

  @doc "Pushes many sprites with one packed rotate/zoom command and options."
  @spec push_rotate_zoom_list(list(), keyword()) :: binary()
  defdelegate push_rotate_zoom_list(instances, opts), to: Codec

  @doc """
  Decodes a binary-batch command stream without calling the native driver.
  """
  @spec decode(iodata()) :: {:ok, [map()]} | {:error, term()}
  defdelegate decode(commands), to: Codec

  @doc """
  Decodes a binary-batch command stream or raises `ArgumentError`.
  """
  @spec decode!(iodata()) :: [map()]
  defdelegate decode!(commands), to: Codec

  @doc """
  Returns a binary-batch diagnostic summary.
  """
  @spec summary(iodata()) :: {:ok, map()} | {:error, term()}
  defdelegate summary(commands), to: Diagnostics

  @doc """
  Returns a binary-batch diagnostic summary or raises `ArgumentError`.
  """
  @spec summary!(iodata()) :: map()
  defdelegate summary!(commands), to: Diagnostics

  @doc """
  Returns a structured diagnostic report for a binary-batch command stream.
  """
  @spec diagnose(iodata()) :: {:ok, map()} | {:error, map()}
  defdelegate diagnose(commands), to: Diagnostics

  @doc """
  Returns a successful binary-batch diagnostic report or raises `ArgumentError`.
  """
  @spec diagnose!(iodata()) :: map()
  defdelegate diagnose!(commands), to: Diagnostics

  @doc """
  Compares two binary-batch command streams using `summary/1` metrics.
  """
  @spec compare(iodata(), iodata()) ::
          {:ok, map()} | {:error, {:baseline, term()}} | {:error, {:candidate, term()}}
  defdelegate compare(baseline_commands, candidate_commands), to: Diagnostics

  @doc """
  Compares two binary-batch command streams or raises `ArgumentError`.
  """
  @spec compare!(iodata(), iodata()) :: map()
  defdelegate compare!(baseline_commands, candidate_commands), to: Diagnostics

  @doc """
  Checks a binary-batch command stream against caller-provided diagnostic limits.
  """
  @spec check_budget(iodata(), map() | keyword()) ::
          {:ok, map()} | {:error, term()} | {:error, {:budget_exceeded, map()}}
  defdelegate check_budget(commands, limits), to: Diagnostics

  @doc """
  Checks a binary-batch command stream against diagnostic limits or raises `ArgumentError`.
  """
  @spec check_budget!(iodata(), map() | keyword()) :: map()
  defdelegate check_budget!(commands, limits), to: Diagnostics
end
