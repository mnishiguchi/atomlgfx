# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Native do
  @moduledoc false

  @nif_not_loaded :nif_not_loaded

  # Keep the BEAM fallback opaque to Elixir's type inference. AtomVM replaces
  # call/4 with the registered NIF, so this indirection is host-only.
  @doc false
  def nif_not_loaded, do: @nif_not_loaded

  def init, do: @nif_not_loaded
  def init(_config), do: @nif_not_loaded
  def close, do: @nif_not_loaded
  def call(_opcode, _target, _flags, _args), do: :erlang.apply(__MODULE__, :nif_not_loaded, [])

  def width, do: @nif_not_loaded
  def height, do: @nif_not_loaded

  def start_write, do: @nif_not_loaded
  def end_write, do: @nif_not_loaded
  def display, do: @nif_not_loaded

  def set_rotation(_rotation), do: @nif_not_loaded
  def set_brightness(_brightness), do: @nif_not_loaded

  def fill_screen(_color), do: @nif_not_loaded
  def clear(_color), do: @nif_not_loaded

  def draw_pixel(_x, _y, _color), do: @nif_not_loaded
  def draw_fast_vline(_x, _y, _height, _color), do: @nif_not_loaded
  def draw_fast_hline(_x, _y, _width, _color), do: @nif_not_loaded
  def draw_line(_x0, _y0, _x1, _y1, _color), do: @nif_not_loaded
  def draw_rect(_x, _y, _width, _height, _color), do: @nif_not_loaded
  def fill_rect(_x, _y, _width, _height, _color), do: @nif_not_loaded
  def draw_round_rect(_x, _y, _width, _height, _radius, _color), do: @nif_not_loaded
  def fill_round_rect(_x, _y, _width, _height, _radius, _color), do: @nif_not_loaded
  def draw_circle(_x, _y, _radius, _color), do: @nif_not_loaded
  def fill_circle(_x, _y, _radius, _color), do: @nif_not_loaded
  def draw_ellipse(_x, _y, _radius_x, _radius_y, _color), do: @nif_not_loaded
  def fill_ellipse(_x, _y, _radius_x, _radius_y, _color), do: @nif_not_loaded

  def draw_arc(_x, _y, _radius0, _radius1, _angle0, _angle1, _color),
    do: @nif_not_loaded

  def fill_arc(_x, _y, _radius0, _radius1, _angle0, _angle1, _color),
    do: @nif_not_loaded

  def draw_triangle(_x0, _y0, _x1, _y1, _x2, _y2, _color), do: @nif_not_loaded
  def fill_triangle(_x0, _y0, _x1, _y1, _x2, _y2, _color), do: @nif_not_loaded

  def set_cursor(_x, _y), do: @nif_not_loaded
  def set_text_size(_scale), do: @nif_not_loaded
  def set_text_datum(_datum), do: @nif_not_loaded
  def set_text_color(_foreground), do: @nif_not_loaded
  def set_text_color(_foreground, _background), do: @nif_not_loaded
  def draw_string(_text, _x, _y), do: @nif_not_loaded
  def print(_text), do: @nif_not_loaded
  def println(_text), do: @nif_not_loaded

  def push_image(_x, _y, _width, _height, _pixels), do: @nif_not_loaded

  def batch(_target, _command_binary), do: @nif_not_loaded
end
