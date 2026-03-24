# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX do
  @moduledoc """
  Elixir client for the `lgfx_port` AtomVM port driver.

  The public API is intentionally shaped like a small LovyanGFX-style wrapper:

  - use natural text scales like `1`, `2`, or `1.5`
  - use natural rotation angles in degrees
  - use natural zoom values like `1.0`, `2.0`, or `0.5`

  Rotation and scale APIs use direct LovyanGFX-style numeric semantics.

  Typical bring-up:

      {:ok, port} =
        AtomLGFX.open(
          panel_driver: :ili9341_2,
          width: 240,
          height: 320,
          offset_rotation: 4,
          invert: true,
          rgb_order: false,
          lcd_spi_host: :spi2_host,
          touch_cs_gpio: 44
        )

      :ok = AtomLGFX.ping(port)
      :ok = AtomLGFX.init(port)
      :ok = AtomLGFX.display(port)

  Lifecycle notes:

  - The native driver uses a singleton device model.
  - `open/1` remembers open-time config per port.
  - `init/1` applies the remembered config for that same port.
  - Only one port can own the live native device at a time.
  - `close/1` performs full device teardown and clears this module's runtime caches.
  - `close/1` does not close the port handle and does not forget that port's remembered open-time config.
  - `start_write/1` and `end_write/1` map directly to LovyanGFX `startWrite()` / `endWrite()`.
  - Write sessions are nestable and should be paired by the caller.
  - `close/1` force-unwinds any still-open native write nesting for the owning port during teardown.

  Data / reply notes:

  - Omitted open options keep build defaults.
  - `get_touch/1` and `get_touch_raw/1` return `{:ok, :none}` or `{:ok, {x, y, size}}`.
  - Primitive and text scalar colors accept either:
    - RGB888 integers like `0x112233`
    - indexed tuples like `{:index, 3}` on palette-backed sprite targets
  - Transparent keys for sprite push operations accept either:
    - RGB565 `u16` values (`0x0000..0xFFFF`)
    - indexed tuples like `{:index, 0}` on palette-backed source sprites
  - `set_text_datum/3` is a numeric passthrough. It accepts `0..255` and forwards the raw value to the pinned native driver.
  - `set_text_size/3` and `set_text_size_xy/4` accept direct LovyanGFX-style scale values.
  - `set_text_wrap/3` follows LovyanGFX one-argument semantics:
    - `set_text_wrap(port, wrap, target)` sets `wrap_x = wrap` and `wrap_y = false`
    - `set_text_wrap_xy/4` sets both axes explicitly
  - `set_cursor/4` and `get_cursor/2` operate on either the LCD target `0` or a sprite target `1..254`.
  - `print/3` and `println/3` use the target's current cursor state.
  - `draw_jpg/5` draws a JPEG binary at `{x, y}` on the selected target.
  - `draw_jpg/11` accepts direct LovyanGFX-style scale values.
  - `set_clip_rect/6` and `clear_clip_rect/2` apply to the selected target.
    LCD and sprite clip states are independent.
  - `create_palette/2` and `set_palette_color/4` manage palette backing for paletted sprite targets.
  - `set_text_font_preset/3` provides stable protocol-owned font selection.
    Supported presets are `:ascii` and `:jp`.
  - Font preset and text scale are independent concerns.
    - Use `set_text_font_preset/3` to choose the glyph source.
    - Use `set_text_size/3` or `set_text_size_xy/4` to control rendered size.
  - `push_rotate_zoom_to/7`, `/8`, and `/9` use direct degree and zoom values.
  """

  alias AtomLGFX.Cache
  alias AtomLGFX.Clip
  alias AtomLGFX.Device
  alias AtomLGFX.Errors
  alias AtomLGFX.Images
  alias AtomLGFX.OpenConfig
  alias AtomLGFX.Primitives
  alias AtomLGFX.Protocol
  alias AtomLGFX.Sprites
  alias AtomLGFX.Text
  alias AtomLGFX.Touch

  @port_name "lgfx_port"

  @doc """
  Opens the `lgfx_port` driver with optional open-time configuration.

  Omitted options keep the driver's build defaults.
  """
  def open(options \\ [])

  def open(options) when is_list(options) do
    open_with(options, &:erlang.open_port/2)
  end

  def open(other) do
    raise ArgumentError,
          "AtomLGFX.open/1 expects a keyword list or proplist, got: #{inspect(other)}"
  end

  @doc false
  def open_with(options, open_port_fun)
      when is_list(options) and is_function(open_port_fun, 2) do
    normalized_open_config = OpenConfig.normalize_open_options!(options)
    port = open_port_fun.({:spawn_driver, @port_name}, normalized_open_config)
    Cache.remember_open_config(port, normalized_open_config)
    {:ok, port}
  end

  @doc """
  Normalizes open-time configuration without opening the driver.

  Returns `{:ok, keyword}` on success or `{:error, reason}` on invalid input.
  """
  def normalize_open_config(options), do: OpenConfig.normalize_open_config(options)

  @doc """
  Sends a raw protocol request tuple to the driver.

  This is mainly useful for smoke tests and protocol-level experiments.
  """
  def raw_call(port, op, target, flags, args, timeout \\ Protocol.short_timeout())
      when is_atom(op) and
             is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    Protocol.raw_call(port, op, target, flags, args, timeout)
  end

  @doc """
  Verifies basic protocol reachability.
  """
  def ping(port), do: Protocol.ping(port)

  @doc """
  Returns the driver's advertised protocol capabilities.
  """
  def get_caps(port), do: Protocol.get_caps(port)

  @doc """
  Returns the remembered open-time configuration for this port.
  """
  def get_open_config(port), do: Cache.get_open_config(port)

  @doc """
  Returns the last protocol-level error snapshot from the driver.
  """
  def get_last_error(port), do: Protocol.get_last_error(port)

  @doc """
  Returns the width of the selected target.

  Target `0` is the LCD. Targets `1..254` are sprite handles.
  """
  def width(port, target \\ 0), do: Protocol.width(port, target)

  @doc """
  Returns the height of the selected target.

  Target `0` is the LCD. Targets `1..254` are sprite handles.
  """
  def height(port, target \\ 0), do: Protocol.height(port, target)

  @doc """
  Returns whether sprite operations are advertised by the driver.
  """
  def supports_sprite?(port), do: Protocol.supports_sprite?(port)

  @doc """
  Returns whether `pushImage` is advertised by the driver.
  """
  def supports_pushimage?(port), do: Protocol.supports_pushimage?(port)

  @doc """
  Returns whether `getLastError` is advertised by the driver.
  """
  def supports_last_error?(port), do: Protocol.supports_last_error?(port)

  @doc """
  Returns whether touch operations are advertised by the driver.
  """
  def supports_touch?(port), do: Protocol.supports_touch?(port)

  @doc """
  Returns whether palette lifecycle operations are advertised by the driver.
  """
  def supports_palette?(port), do: Protocol.supports_palette?(port)

  @doc """
  Returns the maximum accepted binary payload size for this driver instance.
  """
  def max_binary_bytes(port), do: Protocol.max_binary_bytes(port)

  @doc """
  Initializes the native device using this port's remembered open-time configuration.
  """
  def init(port), do: Device.init(port)

  @doc """
  Starts a LovyanGFX write session on the LCD device.

  This maps directly to native `startWrite()` and participates in LovyanGFX's
  nested write counter. Calls should normally be paired with `end_write/1`.
  """
  def start_write(port), do: Device.start_write(port)

  @doc """
  Ends a LovyanGFX write session on the LCD device.

  This maps directly to native `endWrite()` and decrements LovyanGFX's nested
  write counter.
  """
  def end_write(port), do: Device.end_write(port)

  @doc """
  Flushes or presents the LCD display according to the native driver behavior.
  """
  def display(port), do: Device.display(port)

  @doc """
  Sets the LCD rotation.

  Accepted values are `0..7`.
  """
  def set_rotation(port, rotation), do: Device.set_rotation(port, rotation)

  @doc """
  Sets LCD brightness using the driver's raw `u8` brightness value.
  """
  def set_brightness(port, brightness), do: Device.set_brightness(port, brightness)

  @doc """
  Sets the color depth for the selected target.

  Valid depths are `1`, `2`, `4`, `8`, `16`, and `24`.
  """
  def set_color_depth(port, depth, target \\ 0), do: Device.set_color_depth(port, depth, target)

  @doc """
  Closes the native device owned by this port and clears runtime caches.

  This does not close the BEAM port handle itself and does not forget the
  remembered open-time configuration for the port.
  """
  def close(port) do
    with :ok <- Device.close(port) do
      Cache.reset_runtime_cache(port)
      :ok
    end
  end

  @doc """
  Sets a clip rectangle on the selected target.

  Target `0` is the LCD. Targets `1..254` are sprite handles.
  """
  def set_clip_rect(port, x, y, width, height, target \\ 0) do
    Clip.set_clip_rect(port, x, y, width, height, target)
  end

  @doc """
  Clears the active clip rectangle on the selected target.
  """
  def clear_clip_rect(port, target \\ 0), do: Clip.clear_clip_rect(port, target)

  @doc """
  Fills the selected target with the given scalar color.

  Accepts either RGB888 integers like `0x112233` or indexed tuples like `{:index, 3}`.
  Indexed color mode is valid only on palette-backed sprite targets.
  """
  def fill_screen(port, color, target \\ 0), do: Primitives.fill_screen(port, color, target)

  @doc """
  Clears the selected target with the given scalar color.

  Accepts either RGB888 integers like `0x112233` or indexed tuples like `{:index, 3}`.
  Indexed color mode is valid only on palette-backed sprite targets.
  """
  def clear(port, color, target \\ 0), do: Primitives.clear(port, color, target)

  @doc """
  Draws a single pixel using the given scalar color.
  """
  def draw_pixel(port, x, y, color, target \\ 0),
    do: Primitives.draw_pixel(port, x, y, color, target)

  @doc """
  Draws a fast vertical line using the given scalar color.
  """
  def draw_fast_vline(port, x, y, height, color, target \\ 0),
    do: Primitives.draw_fast_vline(port, x, y, height, color, target)

  @doc """
  Draws a fast horizontal line using the given scalar color.
  """
  def draw_fast_hline(port, x, y, width, color, target \\ 0),
    do: Primitives.draw_fast_hline(port, x, y, width, color, target)

  @doc """
  Draws a line using the given scalar color.
  """
  def draw_line(port, x0, y0, x1, y1, color, target \\ 0),
    do: Primitives.draw_line(port, x0, y0, x1, y1, color, target)

  @doc """
  Draws a rectangle outline using the given scalar color.
  """
  def draw_rect(port, x, y, width, height, color, target \\ 0),
    do: Primitives.draw_rect(port, x, y, width, height, color, target)

  @doc """
  Fills a rectangle using the given scalar color.
  """
  def fill_rect(port, x, y, width, height, color, target \\ 0),
    do: Primitives.fill_rect(port, x, y, width, height, color, target)

  @doc """
  Draws a rounded rectangle outline using the given scalar color.
  """
  def draw_round_rect(port, x, y, width, height, radius, color, target \\ 0),
    do: Primitives.draw_round_rect(port, x, y, width, height, radius, color, target)

  @doc """
  Fills a rounded rectangle using the given scalar color.
  """
  def fill_round_rect(port, x, y, width, height, radius, color, target \\ 0),
    do: Primitives.fill_round_rect(port, x, y, width, height, radius, color, target)

  @doc """
  Draws a circle outline using the given scalar color.
  """
  def draw_circle(port, x, y, radius, color, target \\ 0),
    do: Primitives.draw_circle(port, x, y, radius, color, target)

  @doc """
  Fills a circle using the given scalar color.
  """
  def fill_circle(port, x, y, radius, color, target \\ 0),
    do: Primitives.fill_circle(port, x, y, radius, color, target)

  @doc """
  Draws an ellipse outline using the given scalar color.
  """
  def draw_ellipse(port, x, y, radius_x, radius_y, color, target \\ 0),
    do: Primitives.draw_ellipse(port, x, y, radius_x, radius_y, color, target)

  @doc """
  Fills an ellipse using the given scalar color.
  """
  def fill_ellipse(port, x, y, radius_x, radius_y, color, target \\ 0),
    do: Primitives.fill_ellipse(port, x, y, radius_x, radius_y, color, target)

  @doc """
  Draws a triangle outline using the given scalar color.
  """
  def draw_triangle(port, x0, y0, x1, y1, x2, y2, color, target \\ 0),
    do: Primitives.draw_triangle(port, x0, y0, x1, y1, x2, y2, color, target)

  @doc """
  Fills a triangle using the given scalar color.
  """
  def fill_triangle(port, x0, y0, x1, y1, x2, y2, color, target \\ 0),
    do: Primitives.fill_triangle(port, x0, y0, x1, y1, x2, y2, color, target)

  @doc """
  Creates a sprite at the given handle using the target's default sprite color depth.
  """
  def create_sprite(port, width, height, target),
    do: Sprites.create_sprite(port, width, height, target)

  @doc """
  Creates a sprite at the given handle with an explicit color depth.
  """
  def create_sprite(port, width, height, color_depth, target),
    do: Sprites.create_sprite(port, width, height, color_depth, target)

  @doc """
  Deletes the sprite at the given handle.
  """
  def delete_sprite(port, target), do: Sprites.delete_sprite(port, target)

  @doc """
  Creates palette backing for an existing paletted sprite target.
  """
  def create_palette(port, target), do: Sprites.create_palette(port, target)

  @doc """
  Sets one palette entry on a palette-backed sprite target using RGB888.
  """
  def set_palette_color(port, target, palette_index, rgb888),
    do: Sprites.set_palette_color(port, target, palette_index, rgb888)

  @doc """
  Sets the pivot point for the selected target.

  Target `0` is the LCD. Targets `1..254` are sprite handles.
  """
  def set_pivot(port, target, x, y), do: Sprites.set_pivot(port, target, x, y)

  @doc """
  Pushes a source sprite to the destination target at `{x, y}`.
  """
  def push_sprite_to(port, src_target, dst_target, x, y),
    do: Sprites.push_sprite_to(port, src_target, dst_target, x, y)

  @doc """
  Pushes a source sprite to the destination target at `{x, y}` using a transparent key.

  Accepts either an RGB565 integer or an indexed tuple like `{:index, 0}`.
  Indexed transparent mode is valid only on palette-backed source sprites.
  """
  def push_sprite_to(port, src_target, dst_target, x, y, transparent),
    do: Sprites.push_sprite_to(port, src_target, dst_target, x, y, transparent)

  @doc """
  Pushes a source sprite to the LCD at `{x, y}`.
  """
  def push_sprite(port, src_target, x, y), do: Sprites.push_sprite(port, src_target, x, y)

  @doc """
  Pushes a source sprite to the LCD at `{x, y}` using a transparent key.

  Accepts either an RGB565 integer or an indexed tuple like `{:index, 0}`.
  Indexed transparent mode is valid only on palette-backed source sprites.
  """
  def push_sprite(port, src_target, x, y, transparent),
    do: Sprites.push_sprite(port, src_target, x, y, transparent)

  @doc """
  Pushes a source sprite to the destination target using direct degree and zoom values.
  """
  def push_rotate_zoom_to(port, src_target, dst_target, x, y, angle, zoom) do
    Sprites.push_rotate_zoom_to(port, src_target, dst_target, x, y, angle, zoom)
  end

  @doc """
  Pushes a source sprite to the destination target using direct degree and zoom values.
  """
  def push_rotate_zoom_to(port, src_target, dst_target, x, y, angle, zoom_x, zoom_y) do
    Sprites.push_rotate_zoom_to(
      port,
      src_target,
      dst_target,
      x,
      y,
      angle,
      zoom_x,
      zoom_y
    )
  end

  @doc """
  Pushes a source sprite to the destination target using direct degree and zoom values
  and a transparent key.
  """
  def push_rotate_zoom_to(
        port,
        src_target,
        dst_target,
        x,
        y,
        angle,
        zoom_x,
        zoom_y,
        transparent
      ) do
    Sprites.push_rotate_zoom_to(
      port,
      src_target,
      dst_target,
      x,
      y,
      angle,
      zoom_x,
      zoom_y,
      transparent
    )
  end

  @doc """
  Returns the current touch point in screen-space coordinates.

  Returns `{:ok, :none}` or `{:ok, {x, y, size}}`.
  """
  def get_touch(port), do: Touch.get_touch(port)

  @doc """
  Returns the current raw touch point in controller-space coordinates.

  Returns `{:ok, :none}` or `{:ok, {x, y, size}}`.
  """
  def get_touch_raw(port), do: Touch.get_touch_raw(port)

  @doc """
  Sets the persisted touch calibration parameters.

  The payload must contain exactly 8 unsigned 16-bit integers.
  """
  def set_touch_calibrate(port, params8), do: Touch.set_touch_calibrate(port, params8)

  @doc """
  Runs interactive touch calibration and returns the resulting 8-value tuple.
  """
  def calibrate_touch(port), do: Touch.calibrate_touch(port)

  @doc """
  Sets text size using direct LovyanGFX-style scale values.
  """
  def set_text_size(port, scale, target \\ 0), do: Text.set_text_size(port, scale, target)

  @doc """
  Sets text size independently for both axes using direct LovyanGFX-style scale values.
  """
  def set_text_size_xy(port, sx, sy, target \\ 0), do: Text.set_text_size_xy(port, sx, sy, target)

  @doc """
  Sets the text datum as a raw driver-facing `u8` passthrough.

  Accepted range is `0..255`. This API does not define a smaller stable subset.
  """
  def set_text_datum(port, datum, target \\ 0), do: Text.set_text_datum(port, datum, target)

  @doc """
  Sets text wrapping using LovyanGFX one-argument semantics.

  This means:

  - `wrap_x = wrap`
  - `wrap_y = false`

  Use `set_text_wrap_xy/4` when both axes must be controlled explicitly.
  """
  def set_text_wrap(port, wrap, target \\ 0), do: Text.set_text_wrap(port, wrap, target)

  @doc """
  Sets text wrapping for both axes explicitly.
  """
  def set_text_wrap_xy(port, wrap_x, wrap_y, target \\ 0),
    do: Text.set_text_wrap_xy(port, wrap_x, wrap_y, target)

  @doc """
  Sets a stable protocol-owned text font preset.

  Supported presets are `:ascii` and `:jp`.

  This selects the glyph source and normalizes cached text scale to `1.0x`.
  Use `set_text_size/3` or `set_text_size_xy/4` to control rendered size.
  """
  def set_text_font_preset(port, preset, target \\ 0),
    do: Text.set_text_font_preset(port, preset, target)

  @doc """
  Sets the text foreground color, and optionally the background color.

  Accepts RGB888 integers like `0x112233` or indexed tuples like `{:index, 3}`.
  Indexed color mode is valid only on palette-backed sprite targets.
  """
  def set_text_color(port, fg_color, bg_color \\ nil, target \\ 0),
    do: Text.set_text_color(port, fg_color, bg_color, target)

  @doc """
  Sets the current text cursor for the selected target.
  """
  def set_cursor(port, x, y, target \\ 0), do: Text.set_cursor(port, x, y, target)

  @doc """
  Returns the current text cursor for the selected target as `{:ok, {x, y}}`.
  """
  def get_cursor(port, target \\ 0), do: Text.get_cursor(port, target)

  @doc """
  Draws a UTF-8 string at `{x, y}` on the selected target.
  """
  def draw_string(port, x, y, text, target \\ 0), do: Text.draw_string(port, x, y, text, target)

  @doc """
  Prints UTF-8 text at the current cursor of the selected target.
  """
  def print(port, text, target \\ 0), do: Text.print(port, text, target)

  @doc """
  Prints UTF-8 text followed by newline behavior at the current cursor of the selected target.
  """
  def println(port, text, target \\ 0), do: Text.println(port, text, target)

  @doc """
  Convenience helper that sets text color and scale before drawing a string.
  """
  def draw_string_bg(port, x, y, fg_color, bg_color, scale, text, target \\ 0),
    do: Text.draw_string_bg(port, x, y, fg_color, bg_color, scale, text, target)

  @doc """
  Clears cached text state tracked by the Elixir wrapper for the selected target.
  """
  def reset_text_state(port, target \\ 0), do: Text.reset_text_state(port, target)

  @doc """
  Draws a JPEG binary at `{x, y}` on the selected target.

  This uses the short `drawJpg` protocol form.
  """
  def draw_jpg(port, x, y, jpeg, target \\ 0), do: Images.draw_jpg(port, x, y, jpeg, target)

  @doc """
  Draws a JPEG binary using the extended `drawJpg` protocol form.

  `scale_x` and `scale_y` are direct values like `1`, `2`, or `0.5`.
  """
  def draw_jpg(
        port,
        x,
        y,
        max_width,
        max_height,
        off_x,
        off_y,
        scale_x,
        scale_y,
        jpeg,
        target \\ 0
      ) do
    Images.draw_jpg(
      port,
      x,
      y,
      max_width,
      max_height,
      off_x,
      off_y,
      scale_x,
      scale_y,
      jpeg,
      target
    )
  end

  @doc """
  Convenience wrapper for extended JPEG drawing.

  Accepts a single natural Elixir scale value for both axes.
  """
  def draw_jpg_scaled(
        port,
        x,
        y,
        max_width,
        max_height,
        off_x,
        off_y,
        scale,
        jpeg,
        target \\ 0
      ) do
    Images.draw_jpg_scaled(
      port,
      x,
      y,
      max_width,
      max_height,
      off_x,
      off_y,
      scale,
      jpeg,
      target
    )
  end

  @doc """
  Convenience wrapper for extended JPEG drawing.

  Accepts independent direct scale values for X and Y.
  """
  def draw_jpg_scaled(
        port,
        x,
        y,
        max_width,
        max_height,
        off_x,
        off_y,
        scale_x,
        scale_y,
        jpeg,
        target
      ) do
    Images.draw_jpg_scaled(
      port,
      x,
      y,
      max_width,
      max_height,
      off_x,
      off_y,
      scale_x,
      scale_y,
      jpeg,
      target
    )
  end

  @doc """
  Pushes an RGB565 image binary to the selected target.

  The payload is interpreted as big-endian RGB565 pixels. Large payloads may be
  chunked automatically by the Elixir wrapper to stay within the driver's
  advertised binary limit.
  """
  def push_image_rgb565(port, x, y, width, height, pixels, stride_pixels \\ 0, target \\ 0) do
    Images.push_image_rgb565(port, x, y, width, height, pixels, stride_pixels, target)
  end

  @doc """
  Formats a wrapper or protocol error into a readable string.
  """
  def format_error(reason), do: Errors.format_error(reason)
end
