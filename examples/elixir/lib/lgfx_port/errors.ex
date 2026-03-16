defmodule LGFXPort.Errors do
  @moduledoc false

  def format_error({:bad_stride, stride, width}),
    do: "bad stride stride=#{stride} w=#{width}"

  def format_error({:pixels_size_not_even, size}),
    do: "pixels binary size must be even bytes got=#{size}"

  def format_error({:pixels_size_too_small, min_needed, got}),
    do: "pixels too small min_needed=#{min_needed} got=#{got}"

  def format_error({:push_image_max_binary_too_small, max_binary_bytes, row_bytes}),
    do: "push_image max_binary_bytes too small max=#{max_binary_bytes} row_bytes=#{row_bytes}"

  def format_error({:bad_caps_proto_ver, expected, got}),
    do: "caps proto_ver mismatch expected=#{expected} got=#{got}"

  def format_error({:bad_caps_payload, payload}),
    do: "bad caps payload #{inspect(payload)}"

  def format_error({:bad_last_error_payload, payload}),
    do: "bad last_error payload #{inspect(payload)}"

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

  def format_error({:bad_font_preset, preset}),
    do: "bad font preset #{inspect(preset)}"

  def format_error({:bad_touch_payload, op, payload}),
    do: "bad touch payload op=#{inspect(op)} payload=#{inspect(payload)}"

  def format_error({:bad_touch_calibrate_payload, payload}),
    do: "bad touch calibrate payload #{inspect(payload)}"

  def format_error({:bad_touch_calibrate_params, params}),
    do: "bad touch calibrate params #{inspect(params)}"

  def format_error(:empty_jpeg),
    do: "jpeg must not be empty"

  def format_error({:binary_too_large, op_name, got, max}),
    do: "#{op_name} binary too large got=#{got} max=#{max}"

  def format_error(:empty_text),
    do: "text must not be empty"

  def format_error(:text_contains_nul),
    do: "text contains NUL byte"

  def format_error({:port_call_exit, reason}),
    do: "port.call exited #{inspect(reason)}"

  def format_error({:unexpected_reply, reply}),
    do: "unexpected reply #{inspect(reply)}"

  def format_error(reason) when is_atom(reason), do: Atom.to_string(reason)

  def format_error(reason), do: inspect(reason)
end
