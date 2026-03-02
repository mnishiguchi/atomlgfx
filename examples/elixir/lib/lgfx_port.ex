defmodule LGFXPort do
  @moduledoc """
  Minimal Elixir client for the `lgfx_port` AtomVM port driver.

  This module wraps the Erlang-term protocol documented in `docs/LGFX_PORT_PROTOCOL.md`
  and provides small conveniences (timeouts, chunking for RGB565 pushes, and a bit of caching).

  Typical bring-up:

      port = LGFXPort.open()
      :ok = LGFXPort.ping(port)
      :ok = LGFXPort.init(port)
      :ok = LGFXPort.display(port)

  Notes:

  - `getTouch/getTouchRaw` returns `{:ok, :none}` or `{:ok, {x, y, size}}`.
  - Sprite compositing transparent keys for `pushSprite*` / `pushRotateZoom` are RGB565 `u16`
    (0x0000..0xFFFF), not RGB888 (0x00RRGGBB).
  - `pushRotateZoom` uses protocol units:
    - angle: centi-degrees (1.00° = 100)
    - zoom: x1024 fixed-point (1.0x = 1024)
    - dst_target: 0 (LCD) or 1..254 (sprite)
  """
  @compile {:no_warn_undefined, :port}

  import Bitwise

  @port_name "lgfx_port"
  @proto_ver 1

  # Timeouts
  @t_short 5_000
  @t_long 10_000
  @t_touch_calibrate 60_000

  # Protocol flags
  @f_text_has_bg 1 <<< 0

  # Protocol capability bits
  @cap_sprite 1 <<< 0
  @cap_pushimage 1 <<< 1
  @cap_last_error 1 <<< 4
  @cap_touch 1 <<< 6

  # Font preset wire IDs (setFontPreset)
  @font_preset_ascii 0
  @font_preset_jp_small 1
  @font_preset_jp_medium 2
  @font_preset_jp_large 3

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

  def supports_touch?(port) do
    with {:ok, %{feature_bits: feature_bits}} <- get_caps(port) do
      {:ok, (feature_bits &&& @cap_touch) != 0}
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
  def create_sprite(port, width, height, target)
      when is_integer(width) and width >= 1 and
             is_integer(height) and height >= 1 and
             is_integer(target) and target in 1..254 do
    call_ok(port, :createSprite, target, 0, [width, height], @t_long)
  end

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

  def push_sprite(port, src_target, x, y)
      when is_integer(src_target) and src_target in 1..254 and is_integer(x) and is_integer(y) do
    call_ok(port, :pushSprite, src_target, 0, [x, y], @t_long)
  end

  # Transparent key is RGB565 u16 (protocol)
  def push_sprite(port, src_target, x, y, transparent565)
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(x) and is_integer(y) and
             is_integer(transparent565) and transparent565 in 0..0xFFFF do
    call_ok(port, :pushSprite, src_target, 0, [x, y, transparent565], @t_long)
  end

  def push_sprite_region(port, src_target, dst_x, dst_y, src_x, src_y, width, height)
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(dst_x) and is_integer(dst_y) and
             is_integer(src_x) and is_integer(src_y) and
             is_integer(width) and width >= 1 and
             is_integer(height) and height >= 1 do
    call_ok(
      port,
      :pushSpriteRegion,
      src_target,
      0,
      [dst_x, dst_y, src_x, src_y, width, height],
      @t_long
    )
  end

  # Transparent key is RGB565 u16 (protocol)
  def push_sprite_region(
        port,
        src_target,
        dst_x,
        dst_y,
        src_x,
        src_y,
        width,
        height,
        transparent565
      )
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(dst_x) and is_integer(dst_y) and
             is_integer(src_x) and is_integer(src_y) and
             is_integer(width) and width >= 1 and
             is_integer(height) and height >= 1 and
             is_integer(transparent565) and transparent565 in 0..0xFFFF do
    call_ok(
      port,
      :pushSpriteRegion,
      src_target,
      0,
      [dst_x, dst_y, src_x, src_y, width, height, transparent565],
      @t_long
    )
  end

  # -----------------------------------------------------------------------------
  # Sprite rotate/zoom (protocol units)
  # -----------------------------------------------------------------------------
  # Wire shape:
  #   {lgfx, ver, pushRotateZoom, SrcSprite, Flags,
  #      DstTarget, X, Y, AngleCentiDegI32, ZoomXX1024I32, ZoomYX1024I32 [, Transparent565]}
  #
  # - AngleCentiDegI32: 100 = 1.00°
  # - ZoomX1024I32:     1024 = 1.0x
  # - DstTarget:        0 (LCD) or 1..254 (sprite)
  def push_rotate_zoom_to(
        port,
        src_target,
        dst_target,
        x,
        y,
        angle_centi_deg,
        zoom_x1024,
        zoom_y1024
      )
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(dst_target) and dst_target in 0..254 and
             is_integer(x) and is_integer(y) and
             is_integer(angle_centi_deg) and
             is_integer(zoom_x1024) and zoom_x1024 > 0 and
             is_integer(zoom_y1024) and zoom_y1024 > 0 do
    call_ok(
      port,
      :pushRotateZoom,
      src_target,
      0,
      [dst_target, x, y, angle_centi_deg, zoom_x1024, zoom_y1024],
      @t_long
    )
  end

  # Transparent key is RGB565 u16 (protocol)
  def push_rotate_zoom_to(
        port,
        src_target,
        dst_target,
        x,
        y,
        angle_centi_deg,
        zoom_x1024,
        zoom_y1024,
        transparent565
      )
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(dst_target) and dst_target in 0..254 and
             is_integer(x) and is_integer(y) and
             is_integer(angle_centi_deg) and
             is_integer(zoom_x1024) and zoom_x1024 > 0 and
             is_integer(zoom_y1024) and zoom_y1024 > 0 and
             is_integer(transparent565) and transparent565 in 0..0xFFFF do
    call_ok(
      port,
      :pushRotateZoom,
      src_target,
      0,
      [dst_target, x, y, angle_centi_deg, zoom_x1024, zoom_y1024, transparent565],
      @t_long
    )
  end

  # Convenience: accept degrees + float zoom; convert to protocol units (centi-deg / x1024)
  def push_rotate_zoom_deg_to(port, src_target, dst_target, x, y, angle_deg, zoom_x, zoom_y)
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(dst_target) and dst_target in 0..254 and
             is_integer(x) and is_integer(y) and
             is_number(angle_deg) and
             is_number(zoom_x) and zoom_x > 0 and
             is_number(zoom_y) and zoom_y > 0 do
    angle_centi_deg = round(angle_deg * 100)
    zx1024 = round(zoom_x * 1024)
    zy1024 = round(zoom_y * 1024)
    push_rotate_zoom_to(port, src_target, dst_target, x, y, angle_centi_deg, zx1024, zy1024)
  end

  def push_rotate_zoom_deg_to(port, src_target, dst_target, x, y, angle_deg, zoom)
      when is_integer(src_target) and src_target in 1..254 and
             is_integer(dst_target) and dst_target in 0..254 and
             is_integer(x) and is_integer(y) and
             is_number(angle_deg) and
             is_number(zoom) and zoom > 0 do
    push_rotate_zoom_deg_to(port, src_target, dst_target, x, y, angle_deg, zoom, zoom)
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
  # Touch (LCD-only)
  # -----------------------------------------------------------------------------
  #
  # getTouch/getTouchRaw reply:
  # - {:ok, :none} or {:ok, {x, y, size}}
  #
  # calibrateTouch reply:
  # - {:ok, {p0, p1, p2, p3, p4, p5, p6, p7}}
  #
  def get_touch(port) do
    with {:ok, payload} <- call(port, :getTouch, 0, 0, [], @t_short) do
      decode_touch_payload(:getTouch, payload)
    end
  end

  def get_touch_raw(port) do
    with {:ok, payload} <- call(port, :getTouchRaw, 0, 0, [], @t_short) do
      decode_touch_payload(:getTouchRaw, payload)
    end
  end

  def set_touch_calibrate(port, params8) do
    with {:ok, params_list} <- normalize_u16_8(params8) do
      call_ok(port, :setTouchCalibrate, 0, 0, params_list, @t_long)
    end
  end

  def calibrate_touch(port) do
    with {:ok, payload} <- call(port, :calibrateTouch, 0, 0, [], @t_touch_calibrate) do
      decode_calibrate_payload(payload)
    end
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
    result = call_ok(port, :setTextFont, target, 0, [font_id], @t_long)

    if result == :ok do
      :erlang.put({:lgfx_text_font_selection, port, target}, {:font_id, font_id})
      # setTextFont does not change size on device, so do not touch cached text_size here.
    end

    result
  end

  # Font preset helper (driver-defined names mapped to wire preset IDs).
  # Note: preset selection may also change text size on the device (single-font strategy).
  def set_font_preset(port, preset, target \\ 0)
      when is_integer(target) and target in 0..254 do
    case font_preset_to_wire(preset) do
      {:ok, preset_id, canonical_preset} ->
        result = call_ok(port, :setFontPreset, target, 0, [preset_id], @t_long)

        if result == :ok do
          :erlang.put({:lgfx_text_font_selection, port, target}, {:preset, canonical_preset})

          :erlang.put(
            {:lgfx_text_size, port, target},
            implied_text_size_for_preset(canonical_preset)
          )
        end

        result

      {:error, reason} ->
        {:error, reason}
    end
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

  # Minimal convenience helper used heavily by SampleApp.
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

  # Minimal convenience: clears host-side cached text state.
  def reset_text_state(port, target \\ 0) when is_integer(target) and target in 0..254 do
    :erlang.erase({:lgfx_text_color, port, target})
    :erlang.erase({:lgfx_text_size, port, target})
    :erlang.erase({:lgfx_text_font_selection, port, target})
    :ok
  end

  # -----------------------------------------------------------------------------
  # Pixel/image transfer (RGB565)
  # -----------------------------------------------------------------------------
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

  defp decode_touch_payload(_op, :none), do: {:ok, :none}

  defp decode_touch_payload(_op, {x, y, size})
       when is_integer(x) and is_integer(y) and is_integer(size) do
    {:ok, {x, y, size}}
  end

  defp decode_touch_payload(op, other), do: {:error, {:bad_touch_payload, op, other}}

  defp decode_calibrate_payload({p0, p1, p2, p3, p4, p5, p6, p7} = tuple)
       when is_integer(p0) and is_integer(p1) and is_integer(p2) and is_integer(p3) and
              is_integer(p4) and is_integer(p5) and is_integer(p6) and is_integer(p7) do
    {:ok, tuple}
  end

  defp decode_calibrate_payload(other), do: {:error, {:bad_touch_calibrate_payload, other}}

  # -----------------------------------------------------------------------------
  # Internal helpers
  # -----------------------------------------------------------------------------
  defp validate_text_binary(text) when is_binary(text) do
    cond do
      byte_size(text) == 0 ->
        {:error, :empty_text}

      contains_nul?(text) ->
        {:error, :text_contains_nul}

      true ->
        :ok
    end
  end

  defp contains_nul?(<<>>), do: false
  defp contains_nul?(<<0, _::binary>>), do: true

  defp contains_nul?(<<a, b, c, d, e, f, g, h, rest::binary>>) do
    a == 0 or b == 0 or c == 0 or d == 0 or e == 0 or f == 0 or g == 0 or h == 0 or
      contains_nul?(rest)
  end

  defp contains_nul?(<<_byte, rest::binary>>), do: contains_nul?(rest)

  defp normalize_u16_8({p0, p1, p2, p3, p4, p5, p6, p7}) do
    normalize_u16_8([p0, p1, p2, p3, p4, p5, p6, p7])
  end

  defp normalize_u16_8(list) when is_list(list) and length(list) == 8 do
    if Enum.all?(list, &u16?/1) do
      {:ok, list}
    else
      {:error, {:bad_touch_calibrate_params, list}}
    end
  end

  defp normalize_u16_8(other), do: {:error, {:bad_touch_calibrate_params, other}}

  defp u16?(v) when is_integer(v) and v >= 0 and v <= 0xFFFF, do: true
  defp u16?(_), do: false

  defp font_preset_to_wire(:ascii), do: {:ok, @font_preset_ascii, :ascii}
  defp font_preset_to_wire(:jp_small), do: {:ok, @font_preset_jp_small, :jp_small}
  defp font_preset_to_wire(:jp_medium), do: {:ok, @font_preset_jp_medium, :jp_medium}
  defp font_preset_to_wire(:jp_large), do: {:ok, @font_preset_jp_large, :jp_large}
  defp font_preset_to_wire(:jp), do: {:ok, @font_preset_jp_medium, :jp_medium}
  defp font_preset_to_wire(other), do: {:error, {:bad_font_preset, other}}

  # Single-font strategy mapping (host cache only; device is source of truth).
  defp implied_text_size_for_preset(:ascii), do: 1
  defp implied_text_size_for_preset(:jp_small), do: 1
  defp implied_text_size_for_preset(:jp_medium), do: 2
  defp implied_text_size_for_preset(:jp_large), do: 3

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
    part = :binary.part(pixels, offset, row_bytes)
    pack_rows_iolist(pixels, stride_bytes, row_bytes, row + 1, row_end, [part | acc])
  end

  defp reset_port_cache(port) do
    :erlang.erase({:lgfx_max_binary_bytes, port})

    for target <- 0..254 do
      :erlang.erase({:lgfx_text_color, port, target})
      :erlang.erase({:lgfx_text_size, port, target})
      :erlang.erase({:lgfx_text_font_selection, port, target})
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

  def format_error({:bad_caps_payload, payload}), do: "bad caps payload #{inspect(payload)}"

  def format_error({:bad_last_error_payload, payload}),
    do: "bad last_error payload #{inspect(payload)}"

  def format_error({:bad_reply_value, name}), do: "bad reply value for #{inspect(name)}"
  def format_error({:bad_text_color, value}), do: "bad text bg color #{inspect(value)}"
  def format_error({:bad_font_preset, preset}), do: "bad font preset #{inspect(preset)}"

  def format_error({:bad_touch_payload, op, payload}),
    do: "bad touch payload op=#{inspect(op)} payload=#{inspect(payload)}"

  def format_error({:bad_touch_calibrate_payload, payload}),
    do: "bad touch calibrate payload #{inspect(payload)}"

  def format_error({:bad_touch_calibrate_params, params}),
    do: "bad touch calibrate params #{inspect(params)}"

  def format_error(:empty_text), do: "text must not be empty"
  def format_error(:text_contains_nul), do: "text contains NUL byte"
  def format_error({:port_call_exit, reason}), do: "port.call exited #{inspect(reason)}"
  def format_error({:unexpected_reply, reply}), do: "unexpected reply #{inspect(reply)}"
  def format_error(reason) when is_atom(reason), do: Atom.to_string(reason)
  def format_error(reason), do: inspect(reason)
end
