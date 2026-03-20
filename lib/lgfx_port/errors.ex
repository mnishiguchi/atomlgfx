# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule LGFXPort.Errors do
  @moduledoc false

  def format_error(:empty_text),
    do: "text must not be empty"

  def format_error(:text_contains_nul),
    do: "text must not contain NUL bytes"

  def format_error(:empty_jpeg),
    do: "JPEG binary must not be empty"

  def format_error(:missing_scalar_color),
    do: "missing scalar color"

  def format_error({:bad_font_preset, value}),
    do: "bad font preset #{inspect(value)}"

  def format_error({:bad_reply_value, name}),
    do: "bad reply value for #{inspect(name)}"

  def format_error({:bad_cursor_reply, value}),
    do: "bad cursor reply #{inspect(value)}"

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

  def format_error({:bad_angle_deg, value}),
    do: "bad angle in degrees #{inspect(value)}"

  def format_error({:bad_zoom, value}),
    do: "bad zoom #{inspect(value)}"

  def format_error({:bad_image_scale, value}),
    do: "bad image scale #{inspect(value)}"

  def format_error({:bad_image_dimensions, width, height}),
    do: "image dimensions must be non-zero, got width=#{inspect(width)} height=#{inspect(height)}"

  def format_error({:binary_too_large, op_name, payload_size, max_binary_bytes}),
    do:
      "#{inspect(op_name)} payload too large: #{payload_size} bytes exceeds #{max_binary_bytes} bytes"

  def format_error({:pixels_size_not_even, size}),
    do: "RGB565 pixel payload size must be even, got #{size} bytes"

  def format_error({:bad_stride, stride, width}),
    do: "bad stride #{inspect(stride)} for width #{inspect(width)}"

  def format_error({:pixels_size_too_small, min_bytes, actual_bytes}),
    do: "pixel payload too small: need #{min_bytes} bytes, got #{actual_bytes} bytes"

  def format_error({:push_image_max_binary_too_small, max_binary_bytes, row_bytes}),
    do:
      "pushImage max binary too small: #{max_binary_bytes} bytes is smaller than one row #{row_bytes} bytes"

  def format_error({:bad_touch_payload, op, value}),
    do: "bad touch payload for #{inspect(op)}: #{inspect(value)}"

  def format_error({:bad_touch_calibrate_payload, value}),
    do: "bad touch calibration payload #{inspect(value)}"

  def format_error({:bad_touch_calibrate_params, value}),
    do: "bad touch calibration params #{inspect(value)}"

  def format_error({:unexpected_reply, value}),
    do: "unexpected port reply #{inspect(value)}"

  def format_error({:port_call_exit, reason}),
    do: "port call exited with #{inspect(reason)}"

  def format_error({:bad_caps_proto_ver, expected, actual}),
    do: "bad caps protocol version: expected #{inspect(expected)}, got #{inspect(actual)}"

  def format_error({:bad_caps_payload, value}),
    do: "bad caps payload #{inspect(value)}"

  def format_error({:bad_last_error_payload, value}),
    do: "bad last error payload #{inspect(value)}"

  def format_error(other), do: inspect(other)
end
