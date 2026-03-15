defmodule SampleApp do
  @moduledoc false

  alias LGFXPort, as: Port
  alias SampleApp.ClipSmoke
  alias SampleApp.DrawStringStress
  alias SampleApp.MovingIcons
  alias SampleApp.ProtocolSmoke
  alias SampleApp.PushImageStress
  alias SampleApp.SpriteProtocolSmoke
  alias SampleApp.TextProbe
  alias SampleApp.TouchCalibrate
  alias SampleApp.TouchProbe
  alias SampleApp.WriteSessionSmoke

  @default_mode :all

  @valid_modes [
    :protocol,
    :boot,
    :clip,
    :sprites,
    :text,
    :touch,
    :calibrate,
    :stress,
    :moving_icons,
    :all
  ]

  @push_rounds 300
  @text_rounds 500

  @bg 0x000000
  @fg 0xFFFFFF

  # Sample board wiring profile.
  #
  # These are open-time overrides passed explicitly to LGFXPort.open/1 so the
  # bring-up example does not depend entirely on build-time defaults.
  #
  # Duplicate keys are allowed by LGFXPort.open/1; later keys win. Callers may
  # pass extra open options to start/2 to override any of these.
  @sample_open_options [
    lcd_spi_host: :spi2_host,
    touch_spi_host: :spi2_host,
    lcd_bus_shared: true,
    touch_bus_shared: true,
    spi_sclk_gpio: 7,
    spi_mosi_gpio: 9,
    spi_miso_gpio: 8,
    lcd_cs_gpio: 43,
    lcd_dc_gpio: 3,
    lcd_rst_gpio: 2,
    touch_cs_gpio: 44,
    touch_irq_gpio: -1
  ]

  # Rotation preference for bring-up:
  #
  # - :auto_landscape
  #     If the panel boots portrait-ish, try common landscape rotations first.
  #
  # - :landscape_cw
  #     Force rotation 1.
  #
  # - :landscape_ccw
  #     Force rotation 3.
  #
  # - integer 0..7
  #     Force that exact rotation.
  #
  @rotation_preference :auto_landscape

  # -----------------------------------------------------------------------------
  # Public entry
  # -----------------------------------------------------------------------------

  # AtomVM app entrypoint uses this.
  #
  # Modes:
  # - :protocol      -> protocol-only checks (no display init)
  # - :boot          -> init + display + rotation baseline
  # - :clip          -> clipping smoke (requires boot)
  # - :sprites       -> sprite protocol smoke (requires boot)
  # - :text          -> text + font probe (requires boot)
  # - :touch         -> touch probe (requires boot)
  # - :calibrate     -> interactive touch calibration (requires boot)
  # - :stress        -> push_image + draw_string stress (requires boot)
  # - :moving_icons  -> moving icons demo loop (requires boot)
  # - :all           -> stress + sprites + moving_icons (requires boot)
  def start do
    start(@default_mode)
  end

  def start(mode) when is_atom(mode) do
    start(mode, [])
  end

  def start(mode, open_options) when is_atom(mode) and is_list(open_options) do
    effective_open_options = @sample_open_options ++ open_options
    port = Port.open(effective_open_options)

    log_info("Port opened open_options=#{inspect(effective_open_options)}")

    try do
      case run_mode(port, mode) do
        :ok ->
          :ok

        {:error, reason} = err ->
          log_failure("sample_app failed mode=#{inspect(mode)}", reason)
          err
      end
    after
      safe_close_port(port)
    end
  end

  # -----------------------------------------------------------------------------
  # Mode dispatch
  # -----------------------------------------------------------------------------

  defp run_mode(port, :protocol) do
    run_protocol_only(port)
  end

  defp run_mode(port, :boot) do
    boot_for_display(port)
  end

  defp run_mode(port, :clip) do
    with_boot_dims(port, fn w, h ->
      step("clip_smoke", ClipSmoke.run(port, w, h))
    end)
  end

  defp run_mode(port, :sprites) do
    with_boot(port, fn ->
      step("sprite_protocol_smoke", SpriteProtocolSmoke.run(port))
    end)
  end

  defp run_mode(port, :text) do
    with_boot_dims(port, fn w, h ->
      step("text_probe", TextProbe.run(port, w, h))
    end)
  end

  defp run_mode(port, :touch) do
    with_boot_dims(port, fn w, h ->
      step("touch_probe", TouchProbe.run(port, w, h))
    end)
  end

  defp run_mode(port, :calibrate) do
    with_boot_dims(port, fn w, h ->
      step("touch_calibrate", TouchCalibrate.run(port, w, h))
    end)
  end

  defp run_mode(port, :stress) do
    with_boot(port, fn ->
      run_stress_suite(port)
    end)
  end

  defp run_mode(port, :moving_icons) do
    with_boot_dims(port, fn w, h ->
      MovingIcons.run(port, w, h)
    end)
  end

  defp run_mode(port, :all) do
    with_boot_dims(port, fn w, h ->
      with :ok <- run_stress_suite(port),
           :ok <- step("sprite_protocol_smoke", SpriteProtocolSmoke.run(port)),
           :ok <- MovingIcons.run(port, w, h) do
        :ok
      end
    end)
  end

  defp run_mode(_port, mode) do
    log_info("unknown mode=#{inspect(mode)}")
    log_info("valid modes: #{Enum.map_join(@valid_modes, ", ", &inspect/1)}")
    {:error, {:unknown_mode, mode}}
  end

  # -----------------------------------------------------------------------------
  # Mode helpers
  # -----------------------------------------------------------------------------

  defp run_protocol_only(port) do
    with :ok <- step("ping", Port.ping(port)),
         :ok <- step("protocol_smoke", ProtocolSmoke.run(port)) do
      :ok
    end
  end

  defp run_stress_suite(port) do
    with :ok <- step("push_image_stress", PushImageStress.run(port, @push_rounds)),
         :ok <- step("draw_string_stress", DrawStringStress.run(port, @text_rounds)) do
      :ok
    end
  end

  defp with_boot(port, fun) when is_function(fun, 0) do
    with :ok <- boot_for_display(port),
         :ok <- fun.() do
      :ok
    end
  end

  defp with_boot_dims(port, fun) when is_function(fun, 2) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- fun.(w, h) do
      :ok
    end
  end

  # -----------------------------------------------------------------------------
  # Boot / rotation
  # -----------------------------------------------------------------------------

  defp boot_for_display(port) do
    case boot_for_display_with_dims(port) do
      {:ok, _w, _h} -> :ok
      {:error, _reason} = err -> err
    end
  end

  defp boot_for_display_with_dims(port) do
    with :ok <- run_boot_handshake(port),
         {:ok, w0, h0} <- get_wh(port),
         :ok <- log_before_rotation(w0, h0),
         {:ok, rotation, w, h} <- configure_display_rotation(port, w0, h0) do
      finalize_boot_state(port, rotation, w, h)
      {:ok, w, h}
    end
  end

  defp run_boot_handshake(port) do
    with :ok <- step("ping", Port.ping(port)),
         :ok <- step("protocol_smoke", ProtocolSmoke.run(port)),
         :ok <- step("init", Port.init(port)),
         :ok <- step("write_session_smoke", WriteSessionSmoke.run(port)),
         :ok <- step("display(init)", Port.display(port)) do
      :ok
    end
  end

  defp finalize_boot_state(port, rotation, w, h) do
    :erlang.put({:sample_app_rotation, port}, rotation)

    log_info("selected rotation=#{rotation} viewport=#{w}x#{h}")

    _ = Port.fill_screen(port, @bg)
    _ = Port.reset_text_state(port, 0)
    _ = Port.set_text_wrap(port, false, 0)
    _ = Port.set_text_size(port, 2, 0)
    _ = Port.set_text_color(port, @fg, nil, 0)

    :ok
  end

  defp log_before_rotation(w, h) do
    log_info("before rotation: w=#{w} h=#{h}")
    :ok
  end

  defp configure_display_rotation(port, w0, h0) do
    prefer_landscape = prefer_landscape?(w0, h0)
    candidates = rotation_candidates(w0, h0)

    log_info(
      "rotation candidates=#{inspect(candidates)} preferred=#{orientation_name(prefer_landscape)}"
    )

    try_rotation_candidates(port, candidates, prefer_landscape, nil)
  end

  defp try_rotation_candidates(_port, [], _prefer_landscape, {:ok, rotation, w, h}) do
    {:ok, rotation, w, h}
  end

  defp try_rotation_candidates(_port, [], _prefer_landscape, {:error, _reason} = err) do
    err
  end

  defp try_rotation_candidates(_port, [], _prefer_landscape, nil) do
    {:error, :no_rotation_candidate}
  end

  defp try_rotation_candidates(port, [rotation | rest], prefer_landscape, fallback) do
    case apply_rotation(port, rotation) do
      {:ok, rotation, raw_w, raw_h, w, h} = ok ->
        log_info("rotation probe rot=#{rotation} raw=#{raw_w}x#{raw_h} normalized=#{w}x#{h}")

        if landscape?(w, h) == prefer_landscape do
          {:ok, rotation, w, h}
        else
          try_rotation_candidates(port, rest, prefer_landscape, remember_fallback(fallback, ok))
        end

      {:error, reason} = err ->
        log_failure("rotation probe rot=#{rotation} failed", reason)
        try_rotation_candidates(port, rest, prefer_landscape, remember_fallback(fallback, err))
    end
  end

  defp apply_rotation(port, rotation) do
    with :ok <- Port.set_rotation(port, rotation),
         :ok <- Port.display(port),
         {:ok, raw_w, raw_h} <- get_wh(port) do
      {w, h} = normalize_dims_for_rotation(rotation, raw_w, raw_h)
      {:ok, rotation, raw_w, raw_h, w, h}
    end
  end

  defp remember_fallback(nil, {:ok, rotation, _raw_w, _raw_h, w, h}) do
    {:ok, rotation, w, h}
  end

  defp remember_fallback(nil, {:error, _reason} = err), do: err
  defp remember_fallback(fallback, _result), do: fallback

  defp rotation_candidates(w, h) do
    case @rotation_preference do
      :auto_landscape ->
        if prefer_landscape?(w, h) do
          [1, 3, 0, 2]
        else
          [0, 2, 1, 3]
        end

      :landscape_cw ->
        [1]

      :landscape_ccw ->
        [3]

      rotation when is_integer(rotation) and rotation in 0..7 ->
        [rotation]
    end
  end

  defp normalize_dims_for_rotation(rotation, w, h) when is_integer(rotation) do
    if rem(rotation, 2) == 1 and w < h do
      {h, w}
    else
      {w, h}
    end
  end

  defp prefer_landscape?(w, h), do: w <= h
  defp landscape?(w, h), do: w >= h
  defp orientation_name(true), do: "landscape"
  defp orientation_name(false), do: "portrait"

  # -----------------------------------------------------------------------------
  # Small port helpers
  # -----------------------------------------------------------------------------

  defp get_wh(port) do
    with {:ok, w} <- Port.width(port, 0),
         {:ok, h} <- Port.height(port, 0) do
      {:ok, w, h}
    end
  end

  defp safe_close_port(port) do
    case Port.close(port) do
      :ok ->
        log_info("Port closed")
        :ok

      {:error, reason} ->
        log_failure("Port close failed", reason)
        :ok
    end
  end

  # -----------------------------------------------------------------------------
  # Logging helpers
  # -----------------------------------------------------------------------------

  defp step(label, :ok) do
    log_info("#{label} ok")
    :ok
  end

  defp step(label, {:error, reason} = err) do
    log_failure("#{label} failed", reason)
    err
  end

  defp log_info(message) when is_binary(message) do
    IO.puts(message)
  end

  defp log_failure(prefix, reason) when is_binary(prefix) do
    IO.puts("#{prefix}: #{Port.format_error(reason)}")
  end
end
