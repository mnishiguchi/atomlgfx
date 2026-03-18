# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule LGFXPort.Errors do
  @moduledoc false

  def format_error(:empty_text),
    do: "text must not be empty"

  def format_error(:text_contains_nul),
    do: "text must not contain NUL bytes"

  def format_error({:bad_font_preset, value}),
    do: "bad font preset #{inspect(value)}"

  def format_error({:bad_reply_value, name}),
    do: "bad reply value for #{inspect(name)}"

  def format_error({:bad_scalar_color, value}),
    do: "bad scalar color #{inspect(value)}"

  def format_error({:bad_text_color, :fg, value}),
    do: "bad text foreground color #{inspect(value)}"

  def format_error({:bad_text_color, :bg, value}),
    do: "bad text background color #{inspect(value)}"

  def format_error({:bad_text_color, value}),
    do: "bad text color #{inspect(value)}"

  def format_error({:bad_transparent_color, value}),
    do: "bad transparent color #{inspect(value)}"

  def format_error({:bad_palette_index, value}),
    do: "bad palette index #{inspect(value)}"

  def format_error({:bad_palette_color, value}),
    do: "bad palette color #{inspect(value)}"

  def format_error({:bad_text_scale, value}),
    do: "bad text scale #{inspect(value)}"

  def format_error(other), do: inspect(other)
end
