defmodule LGFXPort do
  @moduledoc """
  Elixir client for the `lgfx_port` AtomVM port driver.

  Typical bring-up:

      port =
        LGFXPort.open(
          panel_driver: :ili9341_2,
          width: 240,
          height: 320,
          offset_rotation: 4,
          invert: true,
          rgb_order: false,
          lcd_spi_host: :spi2_host,
          touch_cs_gpio: 44
        )

      :ok = LGFXPort.ping(port)
      :ok = LGFXPort.init(port)
      :ok = LGFXPort.display(port)

  Lifecycle notes:

  - The native driver uses a singleton device model.
  - `open/1` remembers open-time config per port.
  - `init/1` applies the remembered config for that same port.
  - Only one port can own the live native device at a time.
  - `close/1` performs full device teardown and clears this module's runtime caches.
  - `close/1` does not close the port handle and does not forget that port's remembered open-time config.

  Data / reply notes:

  - Omitted open options keep build defaults.
  - `get_touch/1` and `get_touch_raw/1` return `{:ok, :none}` or `{:ok, {x, y, size}}`.
  - Transparent keys for sprite push operations are RGB565 `u16` values (`0x0000..0xFFFF`), not RGB888 (`0x00RRGGBB`).
  - `push_rotate_zoom*` uses protocol units:
    - angle: centi-degrees (`1.00° = 100`)
    - zoom: x1024 fixed-point (`1.0x = 1024`)
    - `dst_target`: `0` for LCD or `1..254` for sprite
  - `set_text_datum/3` is a numeric passthrough. It accepts `0..255` and forwards the raw value to the pinned native driver.
  - `set_text_font/3` is a numeric passthrough. It accepts `0..255` and forwards the raw value to the pinned native driver.
  - For stable protocol-owned font selection, prefer `set_text_font_preset/3`.
  """

  @compile {:no_warn_undefined, :port}

  import Bitwise

  @port_name "lgfx_port"
  @proto_ver 1
  @max_open_config_i32 0x7FFF_FFFF

  @t_short 5_000
  @t_long 10_000
  @t_touch_calibrate 60_000

  @f_text_has_bg 1 <<< 0

  @cap_sprite 1 <<< 0
  @cap_pushimage 1 <<< 1
  @cap_last_error 1 <<< 2
  @cap_touch 1 <<< 3

  @font_preset_ascii 0
  @font_preset_jp_small 1
  @font_preset_jp_medium 2
  @font_preset_jp_large 3

  @valid_color_depths [1, 2, 4, 8, 16, 24]

  @panel_driver_atoms [:ili9488, :ili9341, :ili9341_2, :st7789]
  @spi_host_atoms [:spi2_host, :spi3_host]

  # Keep aligned with:
  # - lgfx_port/lgfx_port.c
  # - src/lgfx_device.h
  @open_option_rules [
    panel_driver: :panel_driver,
    width: :positive_u16,
    height: :positive_u16,
    offset_x: :i32,
    offset_y: :i32,
    offset_rotation: :rotation,
    readable: :boolean,
    invert: :boolean,
    rgb_order: :boolean,
    dlen_16bit: :boolean,
    lcd_spi_mode: :spi_mode,
    lcd_freq_write_hz: :positive_i32,
    lcd_freq_read_hz: :positive_i32,
    lcd_dma_channel: :dma_channel,
    lcd_spi_3wire: :boolean,
    lcd_use_lock: :boolean,
    lcd_bus_shared: :boolean,
    spi_sclk_gpio: :gpio,
    spi_mosi_gpio: :gpio,
    spi_miso_gpio: :gpio_or_disabled,
    lcd_spi_host: :spi_host,
    lcd_cs_gpio: :gpio,
    lcd_dc_gpio: :gpio,
    lcd_rst_gpio: :gpio_or_disabled,
    lcd_pin_busy: :gpio_or_disabled,
    touch_cs_gpio: :gpio_or_disabled,
    touch_irq_gpio: :gpio_or_disabled,
    touch_spi_host: :spi_host,
    touch_spi_freq_hz: :positive_i32,
    touch_offset_rotation: :rotation,
    touch_bus_shared: :boolean
  ]

  @supported_open_config_keys Keyword.keys(@open_option_rules)

  defguardp i16(v) when is_integer(v) and v >= -0x8000 and v <= 0x7FFF
  defguardp u16(v) when is_integer(v) and v >= 0 and v <= 0xFFFF
  defguardp i32(v) when is_integer(v) and v >= -0x8000_0000 and v <= 0x7FFF_FFFF
  defguardp u8(v) when is_integer(v) and v >= 0 and v <= 0xFF
  defguardp positive_i32(v) when is_integer(v) and v >= 1 and v <= 0x7FFF_FFFF

  defguardp target_any(v) when is_integer(v) and v >= 0 and v <= 254
  defguardp sprite_handle(v) when is_integer(v) and v >= 1 and v <= 254

  defguardp color888(v) when is_integer(v) and v >= 0 and v <= 0xFFFFFF
  defguardp rgb565(v) when is_integer(v) and v >= 0 and v <= 0xFFFF

  def open, do: open([])

  def open(options) when is_list(options) do
    open_with(options, &:erlang.open_port/2)
  end

  def open(other) do
    raise ArgumentError,
          "LGFXPort.open/1 expects a keyword list or proplist, got: #{inspect(other)}"
  end

  @doc false
  def open_with(options, open_port_fun)
      when is_list(options) and is_function(open_port_fun, 2) do
    normalized_open_config = normalize_open_options!(options)
    port = open_port_fun.({:spawn_driver, @port_name}, normalized_open_config)
    remember_open_config(port, normalized_open_config)
    port
  end

  @doc false
  def normalize_open_config(options) when is_list(options) do
    normalize_open_options(options)
  end

  def normalize_open_config(other) do
    {:error, {:bad_open_options_shape, other}}
  end

  # Raw protocol call for smoke tests. Target is intentionally not range-checked.
  def raw_call(port, op, target, flags, args, timeout \\ @t_short)
      when is_atom(op) and is_integer(target) and is_integer(flags) and flags >= 0 and
             is_list(args) do
    call(port, op, target, flags, args, timeout)
  end

  def ping(port), do: call_ok(port, :ping, 0, 0, [], @t_short)

  def get_caps(port) do
    with {:ok, payload} <- call(port, :getCaps, 0, 0, [], @t_short) do
      decode_caps(payload)
    end
  end

  def get_open_config(port) do
    case cache_get(open_config_cache_key(port)) do
      value when is_list(value) -> {:ok, value}
      _ -> {:ok, []}
    end
  end

  def get_last_error(port) do
    with {:ok, payload} <- call(port, :getLastError, 0, 0, [], @t_short) do
      decode_last_error(payload)
    end
  end

  def width(port, target \\ 0) when target_any(target) do
    integer_query(port, :width, :width, target)
  end

  def height(port, target \\ 0) when target_any(target) do
    integer_query(port, :height, :height, target)
  end

  def supports_sprite?(port), do: supports_cap?(port, @cap_sprite)
  def supports_pushimage?(port), do: supports_cap?(port, @cap_pushimage)
  def supports_last_error?(port), do: supports_cap?(port, @cap_last_error)
  def supports_touch?(port), do: supports_cap?(port, @cap_touch)

  def max_binary_bytes(port) do
    key = max_binary_bytes_cache_key(port)

    case cache_get(key) do
      value when is_integer(value) and value > 0 ->
        {:ok, value}

      _ ->
        with {:ok, %{max_binary_bytes: max_binary_bytes}} <- get_caps(port) do
          cache_put(key, max_binary_bytes)
          {:ok, max_binary_bytes}
        end
    end
  end

  def init(port), do: call_ok(port, :init, 0, 0, [], @t_long)

  def close(port) do
    call_ok(port, :close, 0, 0, [], @t_long)
    |> after_ok(fn -> reset_runtime_cache(port) end)
  end

  def display(port), do: call_ok(port, :display, 0, 0, [], @t_long)

  def set_rotation(port, rotation) when is_integer(rotation) and rotation in 0..7 do
    call_ok(port, :setRotation, 0, 0, [rotation], @t_long)
  end

  def set_brightness(port, brightness) when u8(brightness) do
    call_ok(port, :setBrightness, 0, 0, [brightness], @t_long)
  end

  def set_color_depth(port, depth, target \\ 0)
      when is_integer(depth) and depth in @valid_color_depths and target_any(target) do
    call_ok(port, :setColorDepth, target, 0, [depth], @t_long)
  end

  def create_sprite(port, width, height, target)
      when u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             sprite_handle(target) do
    call_ok(port, :createSprite, target, 0, [width, height], @t_long)
  end

  def create_sprite(port, width, height, color_depth, target)
      when u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             is_integer(color_depth) and color_depth in @valid_color_depths and
             sprite_handle(target) do
    call_ok(port, :createSprite, target, 0, [width, height, color_depth], @t_long)
  end

  def delete_sprite(port, target) when sprite_handle(target) do
    call_ok(port, :deleteSprite, target, 0, [], @t_long)
  end

  def set_pivot(port, target, x, y)
      when sprite_handle(target) and i16(x) and i16(y) do
    call_ok(port, :setPivot, target, 0, [x, y], @t_long)
  end

  def push_sprite_to(port, src_target, dst_target, x, y)
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) do
    call_ok(port, :pushSprite, src_target, 0, [dst_target, x, y], @t_long)
  end

  def push_sprite_to(port, src_target, dst_target, x, y, transparent565)
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             rgb565(transparent565) do
    call_ok(port, :pushSprite, src_target, 0, [dst_target, x, y, transparent565], @t_long)
  end

  def push_sprite(port, src_target, x, y)
      when sprite_handle(src_target) and i16(x) and i16(y) do
    push_sprite_to(port, src_target, 0, x, y)
  end

  def push_sprite(port, src_target, x, y, transparent565)
      when sprite_handle(src_target) and
             i16(x) and i16(y) and
             rgb565(transparent565) do
    push_sprite_to(port, src_target, 0, x, y, transparent565)
  end

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
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             i32(angle_centi_deg) and
             i32(zoom_x1024) and zoom_x1024 > 0 and
             i32(zoom_y1024) and zoom_y1024 > 0 do
    call_ok(
      port,
      :pushRotateZoom,
      src_target,
      0,
      [dst_target, x, y, angle_centi_deg, zoom_x1024, zoom_y1024],
      @t_long
    )
  end

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
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             i32(angle_centi_deg) and
             i32(zoom_x1024) and zoom_x1024 > 0 and
             i32(zoom_y1024) and zoom_y1024 > 0 and
             rgb565(transparent565) do
    call_ok(
      port,
      :pushRotateZoom,
      src_target,
      0,
      [dst_target, x, y, angle_centi_deg, zoom_x1024, zoom_y1024, transparent565],
      @t_long
    )
  end

  # Convenience wrapper: accepts degrees / float zoom and converts them to protocol units.
  def push_rotate_zoom_deg_to(port, src_target, dst_target, x, y, angle_deg, zoom_x, zoom_y)
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             is_number(angle_deg) and
             is_number(zoom_x) and zoom_x > 0 and
             is_number(zoom_y) and zoom_y > 0 do
    angle_centi_deg = round(angle_deg * 100)
    zx1024 = max(1, round(zoom_x * 1024))
    zy1024 = max(1, round(zoom_y * 1024))

    push_rotate_zoom_to(port, src_target, dst_target, x, y, angle_centi_deg, zx1024, zy1024)
  end

  def push_rotate_zoom_deg_to(port, src_target, dst_target, x, y, angle_deg, zoom)
      when sprite_handle(src_target) and
             target_any(dst_target) and
             i16(x) and i16(y) and
             is_number(angle_deg) and
             is_number(zoom) and zoom > 0 do
    push_rotate_zoom_deg_to(port, src_target, dst_target, x, y, angle_deg, zoom, zoom)
  end

  def fill_screen(port, color888, target \\ 0)
      when color888(color888) and target_any(target) do
    call_ok(port, :fillScreen, target, 0, [color888], @t_long)
  end

  def clear(port, color888, target \\ 0)
      when color888(color888) and target_any(target) do
    call_ok(port, :clear, target, 0, [color888], @t_long)
  end

  def draw_pixel(port, x, y, color888, target \\ 0)
      when i16(x) and i16(y) and
             color888(color888) and
             target_any(target) do
    call_ok(port, :drawPixel, target, 0, [x, y, color888], @t_long)
  end

  def draw_fast_vline(port, x, y, height, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(height) and
             color888(color888) and
             target_any(target) do
    call_ok(port, :drawFastVLine, target, 0, [x, y, height, color888], @t_long)
  end

  def draw_fast_hline(port, x, y, width, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and
             color888(color888) and
             target_any(target) do
    call_ok(port, :drawFastHLine, target, 0, [x, y, width, color888], @t_long)
  end

  def draw_line(port, x0, y0, x1, y1, color888, target \\ 0)
      when i16(x0) and i16(y0) and i16(x1) and i16(y1) and
             color888(color888) and
             target_any(target) do
    call_ok(port, :drawLine, target, 0, [x0, y0, x1, y1, color888], @t_long)
  end

  def draw_rect(port, x, y, width, height, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and
             u16(height) and
             color888(color888) and
             target_any(target) do
    call_ok(port, :drawRect, target, 0, [x, y, width, height, color888], @t_long)
  end

  def fill_rect(port, x, y, width, height, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and
             u16(height) and
             color888(color888) and
             target_any(target) do
    call_ok(port, :fillRect, target, 0, [x, y, width, height, color888], @t_long)
  end

  def draw_circle(port, x, y, radius, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius) and
             color888(color888) and
             target_any(target) do
    call_ok(port, :drawCircle, target, 0, [x, y, radius, color888], @t_long)
  end

  def fill_circle(port, x, y, radius, color888, target \\ 0)
      when i16(x) and i16(y) and
             u16(radius) and
             color888(color888) and
             target_any(target) do
    call_ok(port, :fillCircle, target, 0, [x, y, radius, color888], @t_long)
  end

  def draw_triangle(port, x0, y0, x1, y1, x2, y2, color888, target \\ 0)
      when i16(x0) and i16(y0) and
             i16(x1) and i16(y1) and
             i16(x2) and i16(y2) and
             color888(color888) and
             target_any(target) do
    call_ok(port, :drawTriangle, target, 0, [x0, y0, x1, y1, x2, y2, color888], @t_long)
  end

  def fill_triangle(port, x0, y0, x1, y1, x2, y2, color888, target \\ 0)
      when i16(x0) and i16(y0) and
             i16(x1) and i16(y1) and
             i16(x2) and i16(y2) and
             color888(color888) and
             target_any(target) do
    call_ok(port, :fillTriangle, target, 0, [x0, y0, x1, y1, x2, y2, color888], @t_long)
  end

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

  def set_text_size(port, size, target \\ 0)
      when is_integer(size) and size in 1..255 and target_any(target) do
    call_ok(port, :setTextSize, target, 0, [size], @t_long)
    |> after_ok(fn -> cache_put(text_size_cache_key(port, target), {size, size}) end)
  end

  def set_text_size_xy(port, sx, sy, target \\ 0)
      when is_integer(sx) and sx in 1..255 and
             is_integer(sy) and sy in 1..255 and
             target_any(target) do
    call_ok(port, :setTextSize, target, 0, [sx, sy], @t_long)
    |> after_ok(fn -> cache_put(text_size_cache_key(port, target), {sx, sy}) end)
  end

  @doc """
  Sets the text datum as a raw driver-facing `u8` passthrough.

  Accepted range is `0..255`. This API does not define a smaller stable subset.
  """
  def set_text_datum(port, datum, target \\ 0)
      when u8(datum) and target_any(target) do
    call_ok(port, :setTextDatum, target, 0, [datum], @t_long)
  end

  def set_text_wrap(port, wrap, target \\ 0)
      when is_boolean(wrap) and target_any(target) do
    call_ok(port, :setTextWrap, target, 0, [wrap], @t_long)
  end

  def set_text_wrap_xy(port, wrap_x, wrap_y, target \\ 0)
      when is_boolean(wrap_x) and is_boolean(wrap_y) and target_any(target) do
    call_ok(port, :setTextWrap, target, 0, [wrap_x, wrap_y], @t_long)
  end

  @doc """
  Sets the text font as a raw driver-facing `u8` passthrough.

  Accepted range is `0..255`. For stable protocol-owned font choices, prefer
  `set_text_font_preset/3`.
  """
  def set_text_font(port, font_id, target \\ 0)
      when u8(font_id) and target_any(target) do
    call_ok(port, :setTextFont, target, 0, [font_id], @t_long)
    |> after_ok(fn ->
      cache_put(text_font_selection_cache_key(port, target), {:font_id, font_id})
    end)
  end

  # Preset selection also updates cached font selection and implied text size.
  def set_text_font_preset(port, preset, target \\ 0)
      when target_any(target) do
    with {:ok, preset_id, canonical_preset} <- font_preset_to_wire(preset) do
      call_ok(port, :setTextFontPreset, target, 0, [preset_id], @t_long)
      |> after_ok(fn ->
        cache_put(text_font_selection_cache_key(port, target), {:preset, canonical_preset})

        cache_put(
          text_size_cache_key(port, target),
          implied_text_size_for_preset(canonical_preset)
        )
      end)
    end
  end

  def set_text_color(port, fg888, bg888 \\ nil, target \\ 0)
      when color888(fg888) and target_any(target) do
    case normalize_text_color_args(fg888, bg888) do
      {:ok, flags, args, desired} ->
        call_ok(port, :setTextColor, target, flags, args, @t_long)
        |> after_ok(fn -> cache_put(text_color_cache_key(port, target), desired) end)

      {:error, reason} ->
        {:error, reason}
    end
  end

  def draw_string(port, x, y, text, target \\ 0)
      when i16(x) and i16(y) and is_binary(text) and target_any(target) do
    with :ok <- validate_text_binary(text) do
      call_ok(port, :drawString, target, 0, [x, y, text], @t_long)
    end
  end

  def draw_string_bg(port, x, y, fg888, bg888, size, text, target \\ 0)
      when i16(x) and i16(y) and
             color888(fg888) and
             color888(bg888) and
             is_integer(size) and size in 1..255 and
             is_binary(text) and
             target_any(target) do
    with :ok <- validate_text_binary(text),
         :ok <- maybe_set_text_color(port, fg888, bg888, target),
         :ok <- maybe_set_text_size(port, size, target),
         :ok <- draw_string(port, x, y, text, target) do
      :ok
    end
  end

  def reset_text_state(port, target \\ 0) when target_any(target) do
    erase_text_cache(port, target)
    :ok
  end

  def push_image_rgb565(port, x, y, width, height, pixels, stride_pixels \\ 0, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and
             u16(height) and
             is_binary(pixels) and
             u16(stride_pixels) and
             target_any(target) do
    with :ok <- validate_non_empty_image_dims(width, height),
         :ok <- validate_even_pixel_binary(pixels),
         {:ok, stride} <- normalize_stride_pixels(width, stride_pixels),
         :ok <- validate_pixel_binary_size(pixels, stride, height) do
      push_image_rgb565_transfer(
        port,
        x,
        y,
        width,
        height,
        pixels,
        stride_pixels,
        stride,
        target
      )
    else
      :skip -> :ok
      {:error, reason} -> {:error, reason}
    end
  end

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

  defp decode_caps({:caps, proto_ver, max_binary_bytes, max_sprites, feature_bits})
       when is_integer(proto_ver) and is_integer(max_binary_bytes) and
              is_integer(max_sprites) and is_integer(feature_bits) do
    sprite_cap_set? = (feature_bits &&& @cap_sprite) != 0

    cond do
      proto_ver != @proto_ver ->
        {:error, {:bad_caps_proto_ver, @proto_ver, proto_ver}}

      max_binary_bytes <= 0 ->
        {:error, {:bad_caps_payload, {:max_binary_bytes, max_binary_bytes}}}

      max_sprites < 0 ->
        {:error, {:bad_caps_payload, {:max_sprites, max_sprites}}}

      not sprite_cap_set? and max_sprites != 0 ->
        {:error,
         {:bad_caps_payload, {:max_sprites_without_cap_sprite, max_sprites, feature_bits}}}

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

  defp remember_open_config(port, normalized_open_config) when is_list(normalized_open_config) do
    cache_put(open_config_cache_key(port), normalized_open_config)
    :ok
  end

  defp normalize_open_options!(options) do
    case normalize_open_config(options) do
      {:ok, normalized_options} ->
        normalized_options

      {:error, reason} ->
        raise ArgumentError, format_open_option_error(reason)
    end
  end

  defp normalize_open_options(options) when is_list(options) do
    with {:ok, normalized_map} <- normalize_open_option_entries(options, %{}, options) do
      {:ok, open_config_map_to_keyword(normalized_map)}
    end
  end

  defp normalize_open_option_entries([], normalized_map, _original_options) do
    {:ok, normalized_map}
  end

  defp normalize_open_option_entries(
         [{key, _value} = entry | rest],
         normalized_map,
         original_options
       )
       when is_atom(key) do
    case normalize_open_option_entry(entry, original_options) do
      {:ok, {normalized_key, normalized_value}} ->
        normalize_open_option_entries(
          rest,
          Map.put(normalized_map, normalized_key, normalized_value),
          original_options
        )

      {:error, _reason} = err ->
        err
    end
  end

  defp normalize_open_option_entries(_entries, _normalized_map, original_options) do
    {:error, {:bad_open_options_shape, original_options}}
  end

  defp normalize_open_option_entry({key, value}, _original_options) when is_atom(key) do
    case normalize_open_option_value(key, value) do
      {:ok, normalized_value} -> {:ok, {key, normalized_value}}
      {:error, _reason} = err -> err
    end
  end

  defp normalize_open_option_entry(_entry, original_options) do
    {:error, {:bad_open_options_shape, original_options}}
  end

  defp normalize_open_option_value(key, value) do
    case Keyword.fetch(@open_option_rules, key) do
      {:ok, rule} -> normalize_open_option_by_rule(rule, key, value)
      :error -> {:error, {:unknown_open_option, key}}
    end
  end

  defp normalize_open_option_by_rule(:panel_driver, _key, value)
       when value in @panel_driver_atoms,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:panel_driver, key, value) do
    {:error, {:bad_open_option_value, key, value, ":ili9488, :ili9341, :ili9341_2, or :st7789"}}
  end

  defp normalize_open_option_by_rule(:positive_u16, _key, value)
       when u16(value) and value > 0,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:positive_u16, key, value) do
    {:error, {:bad_open_option_value, key, value, "a positive integer in 1..65535"}}
  end

  defp normalize_open_option_by_rule(:positive_i32, _key, value)
       when positive_i32(value),
       do: {:ok, value}

  defp normalize_open_option_by_rule(:positive_i32, key, value) do
    {:error,
     {:bad_open_option_value, key, value, "a positive integer in 1..#{@max_open_config_i32}"}}
  end

  defp normalize_open_option_by_rule(:i32, _key, value) when i32(value), do: {:ok, value}

  defp normalize_open_option_by_rule(:i32, key, value) do
    {:error, {:bad_open_option_value, key, value, "a signed 32-bit integer"}}
  end

  defp normalize_open_option_by_rule(:rotation, _key, value)
       when is_integer(value) and value in 0..7,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:rotation, key, value) do
    {:error, {:bad_open_option_value, key, value, "an integer in 0..7"}}
  end

  defp normalize_open_option_by_rule(:spi_mode, _key, value)
       when is_integer(value) and value in 0..3,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:spi_mode, key, value) do
    {:error, {:bad_open_option_value, key, value, "an integer in 0..3"}}
  end

  defp normalize_open_option_by_rule(:boolean, _key, value) when is_boolean(value),
    do: {:ok, value}

  defp normalize_open_option_by_rule(:boolean, key, value) do
    {:error, {:bad_open_option_value, key, value, "a boolean"}}
  end

  defp normalize_open_option_by_rule(:gpio, _key, value)
       when is_integer(value) and value >= 0 and value <= 255,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:gpio, key, value) do
    {:error, {:bad_open_option_value, key, value, "a GPIO integer in 0..255"}}
  end

  defp normalize_open_option_by_rule(:gpio_or_disabled, _key, value)
       when is_integer(value) and value >= -1 and value <= 255,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:gpio_or_disabled, key, value) do
    {:error, {:bad_open_option_value, key, value, "a GPIO integer in -1..255 (-1 disables)"}}
  end

  defp normalize_open_option_by_rule(:spi_host, _key, value)
       when value in @spi_host_atoms,
       do: {:ok, value}

  defp normalize_open_option_by_rule(:spi_host, key, value) do
    {:error, {:bad_open_option_value, key, value, ":spi2_host or :spi3_host"}}
  end

  defp normalize_open_option_by_rule(:dma_channel, _key, value) when value in [1, 2],
    do: {:ok, value}

  defp normalize_open_option_by_rule(:dma_channel, _key, :spi_dma_ch_auto),
    do: {:ok, :spi_dma_ch_auto}

  defp normalize_open_option_by_rule(:dma_channel, key, value) do
    {:error, {:bad_open_option_value, key, value, ":spi_dma_ch_auto, 1, or 2"}}
  end

  defp open_config_map_to_keyword(normalized_map) when is_map(normalized_map) do
    Enum.reduce(@supported_open_config_keys, [], fn key, acc ->
      case Map.fetch(normalized_map, key) do
        {:ok, value} -> [{key, value} | acc]
        :error -> acc
      end
    end)
    |> :lists.reverse()
  end

  defp format_open_option_error({:bad_open_options_shape, options}) do
    "LGFXPort.open/1 expects a keyword list or proplist with atom keys, got: #{inspect(options)}"
  end

  defp format_open_option_error({:unknown_open_option, key}) do
    "unknown LGFXPort.open/1 option #{inspect(key)}; supported keys: #{inspect(@supported_open_config_keys)}"
  end

  defp format_open_option_error({:bad_open_option_value, key, value, expected}) do
    "bad LGFXPort.open/1 value for #{inspect(key)}: #{inspect(value)} (expected #{expected})"
  end

  defp validate_text_binary(<<>>), do: {:error, :empty_text}

  defp validate_text_binary(text) when is_binary(text) do
    if contains_nul?(text) do
      {:error, :text_contains_nul}
    else
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

  defp normalize_u16_8([p0, p1, p2, p3, p4, p5, p6, p7] = list) do
    if u16?(p0) and u16?(p1) and u16?(p2) and u16?(p3) and
         u16?(p4) and u16?(p5) and u16?(p6) and u16?(p7) do
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

  defp implied_text_size_for_preset(:ascii), do: {1, 1}
  defp implied_text_size_for_preset(:jp_small), do: {1, 1}
  defp implied_text_size_for_preset(:jp_medium), do: {2, 2}
  defp implied_text_size_for_preset(:jp_large), do: {3, 3}

  defp maybe_set_text_color(port, fg888, bg888, target) do
    desired = {fg888, bg888}

    case cache_get(text_color_cache_key(port, target)) do
      ^desired -> :ok
      _ -> set_text_color(port, fg888, bg888, target)
    end
  end

  defp maybe_set_text_size(port, size, target) do
    desired = {size, size}

    case cache_get(text_size_cache_key(port, target)) do
      ^desired -> :ok
      _ -> set_text_size(port, size, target)
    end
  end

  defp normalize_text_color_args(fg888, nil), do: {:ok, 0, [fg888], {fg888, nil}}

  defp normalize_text_color_args(fg888, bg888) when color888(bg888) do
    {:ok, @f_text_has_bg, [fg888, bg888], {fg888, bg888}}
  end

  defp normalize_text_color_args(_fg888, bg888), do: {:error, {:bad_text_color, bg888}}

  defp validate_non_empty_image_dims(width, height) when width == 0 or height == 0, do: :skip
  defp validate_non_empty_image_dims(_width, _height), do: :ok

  defp validate_even_pixel_binary(pixels) when rem(byte_size(pixels), 2) != 0 do
    {:error, {:pixels_size_not_even, byte_size(pixels)}}
  end

  defp validate_even_pixel_binary(_pixels), do: :ok

  defp normalize_stride_pixels(width, 0), do: {:ok, width}

  defp normalize_stride_pixels(width, stride) when stride < width do
    {:error, {:bad_stride, stride, width}}
  end

  defp normalize_stride_pixels(_width, stride), do: {:ok, stride}

  defp validate_pixel_binary_size(pixels, stride, height) do
    min_bytes = stride * height * 2

    if byte_size(pixels) < min_bytes do
      {:error, {:pixels_size_too_small, min_bytes, byte_size(pixels)}}
    else
      :ok
    end
  end

  # Split large RGB565 transfers when they exceed the device's MaxBinaryBytes limit.
  defp push_image_rgb565_transfer(
         port,
         x,
         y,
         width,
         height,
         pixels,
         stride_pixels,
         stride,
         target
       ) do
    min_bytes = stride * height * 2

    case max_binary_bytes(port) do
      {:ok, max_binary_bytes} when is_integer(max_binary_bytes) and max_binary_bytes > 0 ->
        if min_bytes <= max_binary_bytes do
          push_image_rgb565_raw(port, x, y, width, height, pixels, stride_pixels, target)
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

    if max_binary_bytes < row_bytes do
      {:error, {:push_image_max_binary_too_small, max_binary_bytes, row_bytes}}
    else
      rows_per_chunk = max(1, div(max_binary_bytes, row_bytes))
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
    chunk_height = min(height - row, rows_per_chunk)
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

  defp integer_query(port, op, name, target) do
    with {:ok, value} <- call(port, op, target, 0, [], @t_short),
         true <- is_integer(value) do
      {:ok, value}
    else
      false -> {:error, {:bad_reply_value, name}}
      {:error, reason} -> {:error, reason}
    end
  end

  defp supports_cap?(port, cap_bit) do
    with {:ok, %{feature_bits: feature_bits}} <- get_caps(port) do
      {:ok, (feature_bits &&& cap_bit) != 0}
    end
  end

  defp after_ok(:ok = result, fun) do
    fun.()
    result
  end

  defp after_ok(result, _fun), do: result

  # Clear per-port runtime caches after close/1 so later calls do not reuse stale state.
  defp reset_runtime_cache(port) do
    cache_erase(max_binary_bytes_cache_key(port))
    reset_runtime_cache_targets(port, 0)
  end

  defp reset_runtime_cache_targets(_port, target) when target > 254, do: :ok

  defp reset_runtime_cache_targets(port, target) do
    erase_text_cache(port, target)
    reset_runtime_cache_targets(port, target + 1)
  end

  defp erase_text_cache(port, target) do
    cache_erase(text_color_cache_key(port, target))
    cache_erase(text_size_cache_key(port, target))
    cache_erase(text_font_selection_cache_key(port, target))
  end

  defp cache_get(key), do: :erlang.get(key)
  defp cache_put(key, value), do: :erlang.put(key, value)
  defp cache_erase(key), do: :erlang.erase(key)

  defp open_config_cache_key(port), do: {:lgfx_open_config, port}
  defp max_binary_bytes_cache_key(port), do: {:lgfx_max_binary_bytes, port}
  defp text_color_cache_key(port, target), do: {:lgfx_text_color, port, target}
  defp text_size_cache_key(port, target), do: {:lgfx_text_size, port, target}
  defp text_font_selection_cache_key(port, target), do: {:lgfx_text_font_selection, port, target}

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
