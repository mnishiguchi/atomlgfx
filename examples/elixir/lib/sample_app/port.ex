defmodule SampleApp.Port do
  @moduledoc false
  @compile {:no_warn_undefined, :port}

  import Bitwise

  @port_name "lgfx_port"
  @proto_ver 1

  # Timeouts
  @t_short 5_000
  @t_long 10_000

  # Flags (protocol docs)
  @f_text_has_bg 1 <<< 0

  # -----------------------------------------------------------------------------
  # Port lifecycle
  # -----------------------------------------------------------------------------
  def open do
    :erlang.open_port({:spawn_driver, @port_name}, [])
  end

  # This simply exposes your existing private call/6 for protocol-level checks.
  def raw_call(port, op, target, flags, args, timeout \\ @t_short)
      when is_atom(op) and is_integer(target) and target in 0..254 and is_integer(flags) and
             flags >= 0 and is_list(args) do
    call(port, op, target, flags, args, timeout)
  end

  # -----------------------------------------------------------------------------
  # Control / introspection
  # -----------------------------------------------------------------------------
  def ping(port), do: call_ok(port, :ping, 0, 0, [], @t_short)

  def get_caps(port) do
    with {:ok, payload} <- call(port, :getCaps, 0, 0, [], @t_short) do
      decode_caps(payload)
    end
  end

  def get_last_error(port) do
    with {:ok, payload} <- call(port, :getLastError, 0, 0, [], @t_short) do
      decode_last_error(payload)
    end
  end

  def width(port, target \\ 0) when is_integer(target) and target in 0..254 do
    with {:ok, value} <- call(port, :width, target, 0, [], @t_short),
         true <- is_integer(value) do
      {:ok, value}
    else
      false -> {:error, {:bad_reply_value, :width}}
      {:error, reason} -> {:error, reason}
    end
  end

  def height(port, target \\ 0) when is_integer(target) and target in 0..254 do
    with {:ok, value} <- call(port, :height, target, 0, [], @t_short),
         true <- is_integer(value) do
      {:ok, value}
    else
      false -> {:error, {:bad_reply_value, :height}}
      {:error, reason} -> {:error, reason}
    end
  end

  # Cache helper for max binary bytes (AtomVM-friendly)
  def max_binary_bytes(port) do
    key = {:lgfx_max_binary_bytes, port}

    case :erlang.get(key) do
      v when is_integer(v) and v > 0 ->
        {:ok, v}

      _ ->
        with {:ok, %{max_binary_bytes: m}} <- get_caps(port) do
          :erlang.put(key, m)
          {:ok, m}
        end
    end
  end

  # -----------------------------------------------------------------------------
  # Setup
  # -----------------------------------------------------------------------------
  def init(port), do: call_ok(port, :init, 0, 0, [], @t_long)
  def close(port), do: call_ok(port, :close, 0, 0, [], @t_long)
  def display(port), do: call_ok(port, :display, 0, 0, [], @t_long)

  def set_rotation(port, rotation) when is_integer(rotation) and rotation in 0..7 do
    call_ok(port, :setRotation, 0, 0, [rotation], @t_long)
  end

  def set_brightness(port, b) when is_integer(b) and b in 0..255 do
    call_ok(port, :setBrightness, 0, 0, [b], @t_long)
  end

  def set_color_depth(port, depth, target \\ 0)
      when is_integer(depth) and depth in [1, 2, 4, 8, 16, 24] and target in 0..254 do
    call_ok(port, :setColorDepth, target, 0, [depth], @t_long)
  end

  # -----------------------------------------------------------------------------
  # Primitives
  # -----------------------------------------------------------------------------
  def fill_screen(port, color888, target \\ 0)
      when is_integer(color888) and color888 in 0..0xFFFFFF and target in 0..254 do
    call_ok(port, :fillScreen, target, 0, [color888], @t_long)
  end

  def clear(port, color888, target \\ 0)
      when is_integer(color888) and color888 in 0..0xFFFFFF and target in 0..254 do
    call_ok(port, :clear, target, 0, [color888], @t_long)
  end

  def draw_pixel(port, x, y, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and is_integer(color888) and color888 in 0..0xFFFFFF and
             target in 0..254 do
    call_ok(port, :drawPixel, target, 0, [x, y, color888], @t_long)
  end

  def draw_fast_vline(port, x, y, h, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and is_integer(h) and h >= 0 and is_integer(color888) and
             color888 in 0..0xFFFFFF and target in 0..254 do
    call_ok(port, :drawFastVLine, target, 0, [x, y, h, color888], @t_long)
  end

  def draw_fast_hline(port, x, y, w, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and is_integer(w) and w >= 0 and is_integer(color888) and
             color888 in 0..0xFFFFFF and target in 0..254 do
    call_ok(port, :drawFastHLine, target, 0, [x, y, w, color888], @t_long)
  end

  def draw_line(port, x0, y0, x1, y1, color888, target \\ 0)
      when is_integer(x0) and is_integer(y0) and is_integer(x1) and is_integer(y1) and
             is_integer(color888) and color888 in 0..0xFFFFFF and target in 0..254 do
    call_ok(port, :drawLine, target, 0, [x0, y0, x1, y1, color888], @t_long)
  end

  def draw_rect(port, x, y, w, h, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and is_integer(w) and w >= 0 and is_integer(h) and
             h >= 0 and
             is_integer(color888) and color888 in 0..0xFFFFFF and target in 0..254 do
    call_ok(port, :drawRect, target, 0, [x, y, w, h, color888], @t_long)
  end

  def fill_rect(port, x, y, w, h, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and is_integer(w) and w >= 0 and is_integer(h) and
             h >= 0 and
             is_integer(color888) and color888 in 0..0xFFFFFF and target in 0..254 do
    call_ok(port, :fillRect, target, 0, [x, y, w, h, color888], @t_long)
  end

  def draw_circle(port, x, y, r, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and is_integer(r) and r >= 0 and is_integer(color888) and
             color888 in 0..0xFFFFFF and target in 0..254 do
    call_ok(port, :drawCircle, target, 0, [x, y, r, color888], @t_long)
  end

  def fill_circle(port, x, y, r, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and is_integer(r) and r >= 0 and is_integer(color888) and
             color888 in 0..0xFFFFFF and target in 0..254 do
    call_ok(port, :fillCircle, target, 0, [x, y, r, color888], @t_long)
  end

  def draw_triangle(port, x0, y0, x1, y1, x2, y2, color888, target \\ 0)
      when is_integer(x0) and is_integer(y0) and is_integer(x1) and is_integer(y1) and
             is_integer(x2) and
             is_integer(y2) and is_integer(color888) and color888 in 0..0xFFFFFF and
             target in 0..254 do
    call_ok(port, :drawTriangle, target, 0, [x0, y0, x1, y1, x2, y2, color888], @t_long)
  end

  def fill_triangle(port, x0, y0, x1, y1, x2, y2, color888, target \\ 0)
      when is_integer(x0) and is_integer(y0) and is_integer(x1) and is_integer(y1) and
             is_integer(x2) and
             is_integer(y2) and is_integer(color888) and color888 in 0..0xFFFFFF and
             target in 0..254 do
    call_ok(port, :fillTriangle, target, 0, [x0, y0, x1, y1, x2, y2, color888], @t_long)
  end

  # -----------------------------------------------------------------------------
  # Text (with simple caching for AtomVM)
  # -----------------------------------------------------------------------------
  def set_text_size(port, size, target \\ 0)
      when is_integer(size) and size in 1..255 and target in 0..254 do
    res = call_ok(port, :setTextSize, target, 0, [size], @t_long)

    if res == :ok do
      :erlang.put({:lgfx_text_size, port, target}, size)
    end

    res
  end

  def set_text_datum(port, datum, target \\ 0)
      when is_integer(datum) and datum in 0..255 and target in 0..254 do
    call_ok(port, :setTextDatum, target, 0, [datum], @t_long)
  end

  def set_text_wrap(port, wrap, target \\ 0) when is_boolean(wrap) and target in 0..254 do
    call_ok(port, :setTextWrap, target, 0, [wrap], @t_long)
  end

  def set_text_font(port, font_id, target \\ 0)
      when is_integer(font_id) and font_id in 0..255 and target in 0..254 do
    call_ok(port, :setTextFont, target, 0, [font_id], @t_long)
  end

  def set_text_color(port, fg888, bg888 \\ nil, target \\ 0)
      when is_integer(fg888) and fg888 in 0..0xFFFFFF and target in 0..254 do
    {flags, args} =
      case bg888 do
        nil ->
          {0, [fg888]}

        bg when is_integer(bg) and bg in 0..0xFFFFFF ->
          {@f_text_has_bg, [fg888, bg]}
      end

    res = call_ok(port, :setTextColor, target, flags, args, @t_long)

    if res == :ok do
      :erlang.put({:lgfx_text_color, port, target}, {fg888, bg888})
    end

    res
  end

  def draw_string(port, x, y, text, target \\ 0)
      when is_integer(x) and is_integer(y) and is_binary(text) and byte_size(text) in 1..255 and
             target in 0..254 do
    call_ok(port, :drawString, target, 0, [x, y, text], @t_long)
  end

  def draw_string_bg(port, x, y, fg888, bg888, size, text, target \\ 0)
      when is_integer(fg888) and fg888 in 0..0xFFFFFF and is_integer(bg888) and
             bg888 in 0..0xFFFFFF and
             is_integer(size) and size in 1..255 and is_binary(text) and byte_size(text) in 1..255 and
             target in 0..254 do
    with :ok <- maybe_set_text_color(port, fg888, bg888, target),
         :ok <- maybe_set_text_size(port, size, target),
         :ok <- draw_string(port, x, y, text, target) do
      :ok
    end
  end

  defp maybe_set_text_color(port, fg888, bg888, target) do
    key = {:lgfx_text_color, port, target}
    desired = {fg888, bg888}

    case :erlang.get(key) do
      ^desired -> :ok
      _ -> set_text_color(port, fg888, bg888, target)
    end
  end

  defp maybe_set_text_size(port, size, target) do
    key = {:lgfx_text_size, port, target}

    case :erlang.get(key) do
      ^size -> :ok
      _ -> set_text_size(port, size, target)
    end
  end

  def reset_text_state(port, target \\ 0) when target in 0..254 do
    :erlang.put({:lgfx_text_color, port, target}, :undefined)
    :erlang.put({:lgfx_text_size, port, target}, :undefined)
    :ok
  end

  # -----------------------------------------------------------------------------
  # Pixel/image transfer (RGB565)
  # -----------------------------------------------------------------------------
  # pushImage(X, Y, W, H, StridePixels, Data)
  # - chunks automatically if Data exceeds getCaps().max_binary_bytes
  def push_image_rgb565(port, x, y, w, h, pixels, stride_pixels \\ 0, target \\ 0)
      when is_integer(x) and is_integer(y) and is_integer(w) and w >= 0 and is_integer(h) and
             h >= 0 and
             is_binary(pixels) and is_integer(stride_pixels) and stride_pixels >= 0 and
             target in 0..254 do
    cond do
      w == 0 or h == 0 ->
        :ok

      true ->
        stride =
          case stride_pixels do
            0 -> w
            s -> s
          end

        cond do
          stride < w ->
            {:error, {:bad_stride, stride, w}}

          true ->
            min_bytes = stride * h * 2

            if byte_size(pixels) < min_bytes do
              {:error, {:pixels_size_too_small, min_bytes, byte_size(pixels)}}
            else
              case max_binary_bytes(port) do
                {:ok, max_bin} when is_integer(max_bin) and max_bin > 0 ->
                  packed_total = w * h * 2

                  if packed_total <= max_bin do
                    push_image_rgb565_raw(port, x, y, w, h, pixels, stride_pixels, target)
                  else
                    push_image_rgb565_chunked(port, x, y, w, h, pixels, stride, max_bin, target)
                  end

                _ ->
                  push_image_rgb565_raw(port, x, y, w, h, pixels, stride_pixels, target)
              end
            end
        end
    end
  end

  defp push_image_rgb565_raw(port, x, y, w, h, pixels, stride_pixels, target) do
    call_ok(port, :pushImage, target, 0, [x, y, w, h, stride_pixels, pixels], @t_long)
  end

  defp push_image_rgb565_chunked(port, x, y, w, h, pixels, stride, max_bin, target) do
    row_bytes = w * 2
    stride_bytes = stride * 2

    cond do
      max_bin < row_bytes ->
        {:error, {:push_image_max_binary_too_small, max_bin, row_bytes}}

      true ->
        rows_per_chunk =
          case div(max_bin, row_bytes) do
            0 -> 1
            n -> n
          end

        # Send packed strips with stride=0: (x, y+offset, w, chunk_h, 0, packed_chunk)
        do_push_chunks(port, x, y, w, h, pixels, stride_bytes, rows_per_chunk, target, 0)
    end
  end

  defp do_push_chunks(_port, _x, _y, _w, h, _pixels, _stride_bytes, _rows_per_chunk, _target, row)
       when row >= h,
       do: :ok

  defp do_push_chunks(port, x, y, w, h, pixels, stride_bytes, rows_per_chunk, target, row) do
    chunk_h =
      case h - row do
        remaining when remaining < rows_per_chunk -> remaining
        _ -> rows_per_chunk
      end

    chunk = pack_rows(pixels, stride_bytes, w * 2, row, chunk_h)

    with :ok <- push_image_rgb565_raw(port, x, y + row, w, chunk_h, chunk, 0, target) do
      do_push_chunks(
        port,
        x,
        y,
        w,
        h,
        pixels,
        stride_bytes,
        rows_per_chunk,
        target,
        row + chunk_h
      )
    end
  end

  defp pack_rows(pixels, stride_bytes, row_bytes, row_start, row_count) do
    pack_rows_iolist(pixels, stride_bytes, row_bytes, row_start, row_start + row_count, [])
    |> :lists.reverse()
    |> :erlang.iolist_to_binary()
  end

  defp pack_rows_iolist(_pixels, _stride_bytes, _row_bytes, row, row_end, acc)
       when row >= row_end,
       do: acc

  defp pack_rows_iolist(pixels, stride_bytes, row_bytes, row, row_end, acc) do
    offset = row * stride_bytes
    part = :erlang.binary_part(pixels, offset, row_bytes)
    pack_rows_iolist(pixels, stride_bytes, row_bytes, row + 1, row_end, [part | acc])
  end

  # -----------------------------------------------------------------------------
  # Request/response transport (term protocol)
  # -----------------------------------------------------------------------------
  defp call_ok(port, op, target, flags, args, timeout) do
    case call(port, op, target, flags, args, timeout) do
      {:ok, _} -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

  defp call(port, op, target, flags, args, timeout)
       when is_atom(op) and is_integer(target) and target in 0..254 and is_integer(flags) and
              flags >= 0 and
              is_list(args) do
    req = :erlang.list_to_tuple([:lgfx, @proto_ver, op, target, flags | args])

    case :port.call(port, req, timeout) do
      {:ok, result} ->
        {:ok, result}

      {:error, reason} ->
        {:error, reason}

      other ->
        {:error, {:unexpected_reply, other}}
    end
  end

  # -----------------------------------------------------------------------------
  # Decoding helpers
  # -----------------------------------------------------------------------------
  defp decode_caps({:caps, proto_ver, max_binary_bytes, max_sprites, feature_bits})
       when is_integer(proto_ver) and is_integer(max_binary_bytes) and is_integer(max_sprites) and
              is_integer(feature_bits) do
    {:ok,
     %{
       proto_ver: proto_ver,
       max_binary_bytes: max_binary_bytes,
       max_sprites: max_sprites,
       feature_bits: feature_bits
     }}
  end

  defp decode_caps(other), do: {:error, {:bad_caps_payload, other}}

  defp decode_last_error({:last_error, last_op, reason, last_flags, last_target, esp_err}) do
    {:ok,
     %{
       last_op: last_op,
       reason: reason,
       last_flags: last_flags,
       last_target: last_target,
       esp_err: esp_err
     }}
  end

  defp decode_last_error(other), do: {:error, {:bad_last_error_payload, other}}

  # -----------------------------------------------------------------------------
  # Error formatting
  # -----------------------------------------------------------------------------
  def format_error({:bad_stride, stride, w}), do: "bad stride stride=#{stride} w=#{w}"

  def format_error({:pixels_size_too_small, min_needed, got}),
    do: "pixels too small min_needed=#{min_needed} got=#{got}"

  def format_error({:push_image_max_binary_too_small, max_bin, row_bytes}),
    do: "push_image max_binary_bytes too small max=#{max_bin} row_bytes=#{row_bytes}"

  def format_error({:bad_caps_payload, payload}),
    do: "bad caps payload #{inspect(payload)}"

  def format_error({:bad_last_error_payload, payload}),
    do: "bad last_error payload #{inspect(payload)}"

  def format_error({:bad_reply_value, name}),
    do: "bad reply value for #{inspect(name)}"

  def format_error({:unexpected_reply, reply}),
    do: "unexpected reply #{inspect(reply)}"

  def format_error(reason) when is_atom(reason), do: Atom.to_string(reason)
  def format_error(reason), do: inspect(reason)
end
