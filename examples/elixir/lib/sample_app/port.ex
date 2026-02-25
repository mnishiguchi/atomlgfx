defmodule SampleApp.Port do
  @moduledoc false
  @compile {:no_warn_undefined, :port}

  import Bitwise

  @port_name "lgfx_port"
  @proto_ver 1

  # Timeouts
  @t_short 5_000
  @t_long 10_000

  # Protocol flags
  @f_text_has_bg 1 <<< 0

  # Protocol capability bits
  @cap_sprite 1 <<< 0
  @cap_pushimage 1 <<< 1
  @cap_last_error 1 <<< 4

  # -----------------------------------------------------------------------------
  # Port lifecycle
  # -----------------------------------------------------------------------------
  def open do
    :erlang.open_port({:spawn_driver, @port_name}, [])
  end

  # Exposes the raw protocol call for smoke tests and protocol checks.
  # Intentionally allows any integer target so callers can test invalid targets (e.g. 255).
  def raw_call(port, op, target, flags, args, timeout \\ @t_short)
      when is_atom(op) and is_integer(target) and is_integer(flags) and flags >= 0 and
             is_list(args) do
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

  def supports_sprite?(port) do
    with {:ok, %{feature_bits: feature_bits}} <- get_caps(port) do
      {:ok, (feature_bits &&& @cap_sprite) != 0}
    end
  end

  def supports_pushimage?(port) do
    with {:ok, %{feature_bits: feature_bits}} <- get_caps(port) do
      {:ok, (feature_bits &&& @cap_pushimage) != 0}
    end
  end

  def supports_last_error?(port) do
    with {:ok, %{feature_bits: feature_bits}} <- get_caps(port) do
      {:ok, (feature_bits &&& @cap_last_error) != 0}
    end
  end

  # Cache helper for MaxBinaryBytes (AtomVM-friendly)
  def max_binary_bytes(port) do
    key = {:lgfx_max_binary_bytes, port}

    case :erlang.get(key) do
      value when is_integer(value) and value > 0 ->
        {:ok, value}

      _ ->
        with {:ok, %{max_binary_bytes: max_binary_bytes}} <- get_caps(port) do
          :erlang.put(key, max_binary_bytes)
          {:ok, max_binary_bytes}
        end
    end
  end

  # -----------------------------------------------------------------------------
  # Setup
  # -----------------------------------------------------------------------------
  def init(port), do: call_ok(port, :init, 0, 0, [], @t_long)

  def close(port) do
    result = call_ok(port, :close, 0, 0, [], @t_long)

    if result == :ok do
      reset_port_cache(port)
    end

    result
  end

  def display(port), do: call_ok(port, :display, 0, 0, [], @t_long)

  def set_rotation(port, rotation) when is_integer(rotation) and rotation in 0..7 do
    call_ok(port, :setRotation, 0, 0, [rotation], @t_long)
  end

  def set_brightness(port, brightness) when is_integer(brightness) and brightness in 0..255 do
    call_ok(port, :setBrightness, 0, 0, [brightness], @t_long)
  end

  def set_color_depth(port, depth, target \\ 0)
      when is_integer(depth) and depth in [1, 2, 4, 8, 16, 24] and is_integer(target) and
             target in 0..254 do
    call_ok(port, :setColorDepth, target, 0, [depth], @t_long)
  end

  # -----------------------------------------------------------------------------
  # Sprite operations (LGFX_Sprite)
  # -----------------------------------------------------------------------------
  # createSprite(width, height) on sprite target
  def create_sprite(port, width, height, target)
      when is_integer(width) and width >= 1 and
             is_integer(height) and height >= 1 and
             is_integer(target) and target in 1..254 do
    call_ok(port, :createSprite, target, 0, [width, height], @t_long)
  end

  # createSprite(width, height, color_depth) on sprite target (optional form)
  def create_sprite(port, width, height, color_depth, target)
      when is_integer(width) and width >= 1 and
             is_integer(height) and height >= 1 and
             is_integer(color_depth) and color_depth in [1, 2, 4, 8, 16, 24] and
             is_integer(target) and target in 1..254 do
    call_ok(port, :createSprite, target, 0, [width, height, color_depth], @t_long)
  end

  def delete_sprite(port, target) when is_integer(target) and target in 1..254 do
    call_ok(port, :deleteSprite, target, 0, [], @t_long)
  end

  def set_pivot(port, target, x, y)
      when is_integer(target) and target in 1..254 and is_integer(x) and is_integer(y) do
    call_ok(port, :setPivot, target, 0, [x, y], @t_long)
  end

  # pushSprite(DstX, DstY) on sprite target (sprite -> LCD)
  def push_sprite(port, src_target, x, y)
      when is_integer(src_target) and src_target in 1..254 and is_integer(x) and is_integer(y) do
    call_ok(port, :pushSprite, src_target, 0, [x, y], @t_long)
  end

  # pushSprite(DstX, DstY, TransparentColor888)
  def push_sprite(port, src_target, x, y, transparent_color_888)
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(x) and is_integer(y) and
             is_integer(transparent_color_888) and transparent_color_888 in 0..0xFFFFFF do
    call_ok(port, :pushSprite, src_target, 0, [x, y, transparent_color_888], @t_long)
  end

  # pushSpriteRegion(DstX, DstY, SrcX, SrcY, W, H)
  def push_sprite_region(port, src_target, dst_x, dst_y, src_x, src_y, width, height)
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(dst_x) and is_integer(dst_y) and
             is_integer(src_x) and is_integer(src_y) and
             is_integer(width) and width >= 0 and
             is_integer(height) and height >= 0 do
    call_ok(
      port,
      :pushSpriteRegion,
      src_target,
      0,
      [dst_x, dst_y, src_x, src_y, width, height],
      @t_long
    )
  end

  # pushSpriteRegion(DstX, DstY, SrcX, SrcY, W, H, TransparentColor888)
  def push_sprite_region(
        port,
        src_target,
        dst_x,
        dst_y,
        src_x,
        src_y,
        width,
        height,
        transparent_color_888
      )
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(dst_x) and is_integer(dst_y) and
             is_integer(src_x) and is_integer(src_y) and
             is_integer(width) and width >= 0 and
             is_integer(height) and height >= 0 and
             is_integer(transparent_color_888) and transparent_color_888 in 0..0xFFFFFF do
    call_ok(
      port,
      :pushSpriteRegion,
      src_target,
      0,
      [dst_x, dst_y, src_x, src_y, width, height, transparent_color_888],
      @t_long
    )
  end

  # -----------------------------------------------------------------------------
  # Sprite rotate/zoom
  # -----------------------------------------------------------------------------
  # Host-friendly API used by demo code:
  # - angle_tenths: 900 == 90.0 degrees (rounded to integer degrees for protocol)
  # - zx_q8 / zy_q8: 256 == 1.0x
  #
  # Source sprite is selected by tuple header target (src_target).
  # Destination is LCD (protocol does not take a dst_target arg).
  def push_rotate_zoom(port, src_target, x, y, angle_tenths, zx_q8, zy_q8)
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(x) and is_integer(y) and
             is_integer(angle_tenths) and
             is_integer(zx_q8) and zx_q8 >= 0 and
             is_integer(zy_q8) and zy_q8 >= 0 do
    angle_deg = round(angle_tenths / 10)
    call_ok(port, :pushRotateZoom, src_target, 0, [x, y, angle_deg, zx_q8, zy_q8], @t_long)
  end

  # pushRotateZoom(..., TransparentColor888)
  def push_rotate_zoom(port, src_target, x, y, angle_tenths, zx_q8, zy_q8, transparent_color_888)
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(x) and is_integer(y) and
             is_integer(angle_tenths) and
             is_integer(zx_q8) and zx_q8 >= 0 and
             is_integer(zy_q8) and zy_q8 >= 0 and
             is_integer(transparent_color_888) and transparent_color_888 in 0..0xFFFFFF do
    angle_deg = round(angle_tenths / 10)

    call_ok(
      port,
      :pushRotateZoom,
      src_target,
      0,
      [x, y, angle_deg, zx_q8, zy_q8, transparent_color_888],
      @t_long
    )
  end

  # Convenience for uniform scale (Q8)
  def push_rotate_zoom(port, src_target, x, y, angle_tenths, z_q8)
      when is_integer(z_q8) and z_q8 >= 0 do
    push_rotate_zoom(port, src_target, x, y, angle_tenths, z_q8, z_q8)
  end

  # Optional helper using integer/float degrees and integer/float zoom values.
  # Still emits the same protocol shape (angle as integer degrees; zoom as q8 integers).
  def push_rotate_zoom_deg(port, src_target, x, y, angle_deg, zoom_x, zoom_y)
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(x) and is_integer(y) and
             is_number(angle_deg) and
             is_number(zoom_x) and zoom_x >= 0 and
             is_number(zoom_y) and zoom_y >= 0 do
    angle_tenths = round(angle_deg * 10)
    zx_q8 = round(zoom_x * 256)
    zy_q8 = round(zoom_y * 256)

    push_rotate_zoom(port, src_target, x, y, angle_tenths, zx_q8, zy_q8)
  end

  def push_rotate_zoom_deg(port, src_target, x, y, angle_deg, zoom)
      when is_number(angle_deg) and is_number(zoom) and zoom >= 0 do
    push_rotate_zoom_deg(port, src_target, x, y, angle_deg, zoom, zoom)
  end

  # -----------------------------------------------------------------------------
  # Primitives
  # -----------------------------------------------------------------------------
  def fill_screen(port, color888, target \\ 0)
      when is_integer(color888) and color888 in 0..0xFFFFFF and is_integer(target) and
             target in 0..254 do
    call_ok(port, :fillScreen, target, 0, [color888], @t_long)
  end

  def clear(port, color888, target \\ 0)
      when is_integer(color888) and color888 in 0..0xFFFFFF and is_integer(target) and
             target in 0..254 do
    call_ok(port, :clear, target, 0, [color888], @t_long)
  end

  def draw_pixel(port, x, y, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and
             is_integer(color888) and color888 in 0..0xFFFFFF and
             is_integer(target) and target in 0..254 do
    call_ok(port, :drawPixel, target, 0, [x, y, color888], @t_long)
  end

  def draw_fast_vline(port, x, y, height, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and
             is_integer(height) and height >= 0 and
             is_integer(color888) and color888 in 0..0xFFFFFF and
             is_integer(target) and target in 0..254 do
    call_ok(port, :drawFastVLine, target, 0, [x, y, height, color888], @t_long)
  end

  def draw_fast_hline(port, x, y, width, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and
             is_integer(width) and width >= 0 and
             is_integer(color888) and color888 in 0..0xFFFFFF and
             is_integer(target) and target in 0..254 do
    call_ok(port, :drawFastHLine, target, 0, [x, y, width, color888], @t_long)
  end

  def draw_line(port, x0, y0, x1, y1, color888, target \\ 0)
      when is_integer(x0) and is_integer(y0) and is_integer(x1) and is_integer(y1) and
             is_integer(color888) and color888 in 0..0xFFFFFF and
             is_integer(target) and target in 0..254 do
    call_ok(port, :drawLine, target, 0, [x0, y0, x1, y1, color888], @t_long)
  end

  def draw_rect(port, x, y, width, height, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and
             is_integer(width) and width >= 0 and
             is_integer(height) and height >= 0 and
             is_integer(color888) and color888 in 0..0xFFFFFF and
             is_integer(target) and target in 0..254 do
    call_ok(port, :drawRect, target, 0, [x, y, width, height, color888], @t_long)
  end

  def fill_rect(port, x, y, width, height, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and
             is_integer(width) and width >= 0 and
             is_integer(height) and height >= 0 and
             is_integer(color888) and color888 in 0..0xFFFFFF and
             is_integer(target) and target in 0..254 do
    call_ok(port, :fillRect, target, 0, [x, y, width, height, color888], @t_long)
  end

  def draw_circle(port, x, y, radius, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and
             is_integer(radius) and radius >= 0 and
             is_integer(color888) and color888 in 0..0xFFFFFF and
             is_integer(target) and target in 0..254 do
    call_ok(port, :drawCircle, target, 0, [x, y, radius, color888], @t_long)
  end

  def fill_circle(port, x, y, radius, color888, target \\ 0)
      when is_integer(x) and is_integer(y) and
             is_integer(radius) and radius >= 0 and
             is_integer(color888) and color888 in 0..0xFFFFFF and
             is_integer(target) and target in 0..254 do
    call_ok(port, :fillCircle, target, 0, [x, y, radius, color888], @t_long)
  end

  def draw_triangle(port, x0, y0, x1, y1, x2, y2, color888, target \\ 0)
      when is_integer(x0) and is_integer(y0) and
             is_integer(x1) and is_integer(y1) and
             is_integer(x2) and is_integer(y2) and
             is_integer(color888) and color888 in 0..0xFFFFFF and
             is_integer(target) and target in 0..254 do
    call_ok(port, :drawTriangle, target, 0, [x0, y0, x1, y1, x2, y2, color888], @t_long)
  end

  def fill_triangle(port, x0, y0, x1, y1, x2, y2, color888, target \\ 0)
      when is_integer(x0) and is_integer(y0) and
             is_integer(x1) and is_integer(y1) and
             is_integer(x2) and is_integer(y2) and
             is_integer(color888) and color888 in 0..0xFFFFFF and
             is_integer(target) and target in 0..254 do
    call_ok(port, :fillTriangle, target, 0, [x0, y0, x1, y1, x2, y2, color888], @t_long)
  end

  # -----------------------------------------------------------------------------
  # Text (with simple caching for AtomVM)
  # -----------------------------------------------------------------------------
  def set_text_size(port, size, target \\ 0)
      when is_integer(size) and size in 1..255 and is_integer(target) and target in 0..254 do
    result = call_ok(port, :setTextSize, target, 0, [size], @t_long)

    if result == :ok do
      :erlang.put({:lgfx_text_size, port, target}, size)
    end

    result
  end

  def set_text_datum(port, datum, target \\ 0)
      when is_integer(datum) and datum in 0..255 and is_integer(target) and target in 0..254 do
    call_ok(port, :setTextDatum, target, 0, [datum], @t_long)
  end

  def set_text_wrap(port, wrap, target \\ 0)
      when is_boolean(wrap) and is_integer(target) and target in 0..254 do
    call_ok(port, :setTextWrap, target, 0, [wrap], @t_long)
  end

  def set_text_font(port, font_id, target \\ 0)
      when is_integer(font_id) and font_id in 0..255 and is_integer(target) and target in 0..254 do
    call_ok(port, :setTextFont, target, 0, [font_id], @t_long)
  end

  def set_text_color(port, fg888, bg888 \\ nil, target \\ 0)
      when is_integer(fg888) and fg888 in 0..0xFFFFFF and is_integer(target) and target in 0..254 do
    case bg888 do
      nil ->
        result = call_ok(port, :setTextColor, target, 0, [fg888], @t_long)

        if result == :ok do
          :erlang.put({:lgfx_text_color, port, target}, {fg888, nil})
        end

        result

      bg when is_integer(bg) and bg in 0..0xFFFFFF ->
        result = call_ok(port, :setTextColor, target, @f_text_has_bg, [fg888, bg], @t_long)

        if result == :ok do
          :erlang.put({:lgfx_text_color, port, target}, {fg888, bg})
        end

        result

      _ ->
        {:error, {:bad_text_color, bg888}}
    end
  end

  def draw_string(port, x, y, text, target \\ 0)
      when is_integer(x) and is_integer(y) and is_binary(text) and is_integer(target) and
             target in 0..254 do
    case validate_text_binary(text) do
      :ok -> call_ok(port, :drawString, target, 0, [x, y, text], @t_long)
      {:error, reason} -> {:error, reason}
    end
  end

  def draw_string_bg(port, x, y, fg888, bg888, size, text, target \\ 0)
      when is_integer(fg888) and fg888 in 0..0xFFFFFF and
             is_integer(bg888) and bg888 in 0..0xFFFFFF and
             is_integer(size) and size in 1..255 and
             is_binary(text) and
             is_integer(target) and target in 0..254 do
    with :ok <- validate_text_binary(text),
         :ok <- maybe_set_text_color(port, fg888, bg888, target),
         :ok <- maybe_set_text_size(port, size, target),
         :ok <- draw_string(port, x, y, text, target) do
      :ok
    end
  end

  def reset_text_state(port, target \\ 0) when is_integer(target) and target in 0..254 do
    :erlang.erase({:lgfx_text_color, port, target})
    :erlang.erase({:lgfx_text_size, port, target})
    :ok
  end

  # -----------------------------------------------------------------------------
  # Pixel/image transfer (RGB565)
  # -----------------------------------------------------------------------------
  # pushImage(X, Y, W, H, StridePixels, Data)
  # - chunks automatically if needed based on getCaps().max_binary_bytes
  def push_image_rgb565(port, x, y, width, height, pixels, stride_pixels \\ 0, target \\ 0)
      when is_integer(x) and is_integer(y) and
             is_integer(width) and width >= 0 and
             is_integer(height) and height >= 0 and
             is_binary(pixels) and
             is_integer(stride_pixels) and stride_pixels >= 0 and
             is_integer(target) and target in 0..254 do
    cond do
      width == 0 or height == 0 ->
        :ok

      rem(byte_size(pixels), 2) != 0 ->
        {:error, {:pixels_size_not_even, byte_size(pixels)}}

      true ->
        stride =
          case stride_pixels do
            0 -> width
            value -> value
          end

        cond do
          stride < width ->
            {:error, {:bad_stride, stride, width}}

          true ->
            min_bytes = stride * height * 2

            if byte_size(pixels) < min_bytes do
              {:error, {:pixels_size_too_small, min_bytes, byte_size(pixels)}}
            else
              case max_binary_bytes(port) do
                {:ok, max_binary_bytes}
                when is_integer(max_binary_bytes) and max_binary_bytes > 0 ->
                  if min_bytes <= max_binary_bytes do
                    push_image_rgb565_raw(
                      port,
                      x,
                      y,
                      width,
                      height,
                      pixels,
                      stride_pixels,
                      target
                    )
                  else
                    push_image_rgb565_chunked(
                      port,
                      x,
                      y,
                      width,
                      height,
                      pixels,
                      stride,
                      max_binary_bytes,
                      target
                    )
                  end

                _ ->
                  push_image_rgb565_raw(port, x, y, width, height, pixels, stride_pixels, target)
              end
            end
        end
    end
  end

  # -----------------------------------------------------------------------------
  # Request/response transport (term protocol)
  # -----------------------------------------------------------------------------
  defp call_ok(port, op, target, flags, args, timeout) do
    case call(port, op, target, flags, args, timeout) do
      {:ok, _result} -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

  defp call(port, op, target, flags, args, timeout)
       when is_atom(op) and is_integer(target) and is_integer(flags) and flags >= 0 and
              is_list(args) do
    request = :erlang.list_to_tuple([:lgfx, @proto_ver, op, target, flags | args])

    try do
      case :port.call(port, request, timeout) do
        {:ok, result} ->
          {:ok, result}

        {:error, reason} ->
          {:error, reason}

        other ->
          {:error, {:unexpected_reply, other}}
      end
    catch
      :exit, reason -> {:error, {:port_call_exit, reason}}
    end
  end

  # -----------------------------------------------------------------------------
  # Decoding helpers
  # -----------------------------------------------------------------------------
  defp decode_caps({:caps, proto_ver, max_binary_bytes, max_sprites, feature_bits})
       when is_integer(proto_ver) and is_integer(max_binary_bytes) and
              is_integer(max_sprites) and is_integer(feature_bits) do
    cond do
      proto_ver != @proto_ver ->
        {:error, {:bad_caps_proto_ver, @proto_ver, proto_ver}}

      max_binary_bytes <= 0 ->
        {:error, {:bad_caps_payload, {:max_binary_bytes, max_binary_bytes}}}

      max_sprites < 0 ->
        {:error, {:bad_caps_payload, {:max_sprites, max_sprites}}}

      feature_bits < 0 ->
        {:error, {:bad_caps_payload, {:feature_bits, feature_bits}}}

      true ->
        {:ok,
         %{
           proto_ver: proto_ver,
           max_binary_bytes: max_binary_bytes,
           max_sprites: max_sprites,
           feature_bits: feature_bits
         }}
    end
  end

  defp decode_caps(other), do: {:error, {:bad_caps_payload, other}}

  defp decode_last_error({:last_error, last_op, reason, last_flags, last_target, esp_err})
       when is_integer(last_flags) and is_integer(last_target) and is_integer(esp_err) do
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
  # Internal helpers
  # -----------------------------------------------------------------------------
  defp validate_text_binary(text) when is_binary(text) do
    cond do
      byte_size(text) == 0 ->
        {:error, :empty_text}

      :binary.match(text, <<0>>) != :nomatch ->
        {:error, :text_contains_nul}

      true ->
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

  defp push_image_rgb565_raw(port, x, y, width, height, pixels, stride_pixels, target) do
    call_ok(port, :pushImage, target, 0, [x, y, width, height, stride_pixels, pixels], @t_long)
  end

  defp push_image_rgb565_chunked(
         port,
         x,
         y,
         width,
         height,
         pixels,
         stride,
         max_binary_bytes,
         target
       ) do
    row_bytes = width * 2
    stride_bytes = stride * 2

    cond do
      max_binary_bytes < row_bytes ->
        {:error, {:push_image_max_binary_too_small, max_binary_bytes, row_bytes}}

      true ->
        rows_per_chunk =
          case div(max_binary_bytes, row_bytes) do
            0 -> 1
            rows -> rows
          end

        # Send packed strips with stride=0:
        #   pushImage(x, y + row, width, chunk_h, 0, packed_chunk)
        do_push_chunks(port, x, y, width, height, pixels, stride_bytes, rows_per_chunk, target, 0)
    end
  end

  defp do_push_chunks(
         _port,
         _x,
         _y,
         _width,
         height,
         _pixels,
         _stride_bytes,
         _rows_per_chunk,
         _target,
         row
       )
       when row >= height,
       do: :ok

  defp do_push_chunks(
         port,
         x,
         y,
         width,
         height,
         pixels,
         stride_bytes,
         rows_per_chunk,
         target,
         row
       ) do
    chunk_height =
      case height - row do
        remaining when remaining < rows_per_chunk -> remaining
        _ -> rows_per_chunk
      end

    chunk = pack_rows(pixels, stride_bytes, width * 2, row, chunk_height)

    with :ok <- push_image_rgb565_raw(port, x, y + row, width, chunk_height, chunk, 0, target) do
      do_push_chunks(
        port,
        x,
        y,
        width,
        height,
        pixels,
        stride_bytes,
        rows_per_chunk,
        target,
        row + chunk_height
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

  defp reset_port_cache(port) do
    :erlang.erase({:lgfx_max_binary_bytes, port})

    for target <- 0..254 do
      :erlang.erase({:lgfx_text_color, port, target})
      :erlang.erase({:lgfx_text_size, port, target})
    end

    :ok
  end

  # -----------------------------------------------------------------------------
  # Error formatting
  # -----------------------------------------------------------------------------
  def format_error({:bad_stride, stride, width}), do: "bad stride stride=#{stride} w=#{width}"

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

  def format_error({:bad_text_color, value}),
    do: "bad text bg color #{inspect(value)}"

  def format_error(:empty_text), do: "text must not be empty"
  def format_error(:text_contains_nul), do: "text contains NUL byte"

  def format_error({:port_call_exit, reason}),
    do: "port.call exited #{inspect(reason)}"

  def format_error({:unexpected_reply, reply}),
    do: "unexpected reply #{inspect(reply)}"

  def format_error(reason) when is_atom(reason), do: Atom.to_string(reason)
  def format_error(reason), do: inspect(reason)
end
