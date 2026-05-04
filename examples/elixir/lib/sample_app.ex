# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp do
  @moduledoc false

  alias SampleApp.Face
  alias SampleApp.JapaneseText
  alias SampleApp.MovingIcons
  alias SampleApp.PerfSmoke
  alias SampleApp.ProtocolSmoke
  alias SampleApp.Smoke
  alias SampleApp.SpriteProtocolSmoke

  @default_mode :moving_icons

  @valid_modes [
    :smoke,
    :protocol,
    :boot,
    :perf,
    :face,
    :japanese_text,
    :moving_icons,
    :sprites,
    :touch_calibrate,
    :all
  ]

  @bg 0x0000
  @fg 0xFFFF

  @sample_open_options [
    rgb_order: false,
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

  @rotation_preference :auto_landscape

  def start do
    start(@default_mode)
  end

  def start(mode) when is_atom(mode) do
    start(mode, [])
  end

  def start(mode, open_options) when is_atom(mode) and is_list(open_options) do
    effective_open_options = @sample_open_options ++ open_options
    {:ok, port} = AtomLGFX.open(effective_open_options)

    log_info("AtomLGFX opened open_options=#{inspect(effective_open_options)}")

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

  defp run_mode(port, :protocol) do
    run_protocol_only(port)
  end

  defp run_mode(port, :boot) do
    boot_for_display(port)
  end

  defp run_mode(port, :smoke) do
    with_boot_dims(port, fn w, h ->
      step("smoke", Smoke.run(port, w, h))
    end)
  end

  defp run_mode(port, :perf) do
    with_boot_dims(port, fn w, h ->
      step("perf_smoke", PerfSmoke.run(port, w, h))
    end)
  end

  defp run_mode(port, :face) do
    with_boot_dims(port, fn w, h ->
      step("face", Face.run(port, w, h))
    end)
  end

  defp run_mode(port, :moving_icons) do
    with_boot_dims(port, fn w, h ->
      step("moving_icons", MovingIcons.run(port, w, h))
    end)
  end

  defp run_mode(port, :sprites) do
    with_boot(port, fn ->
      step("sprite_protocol_smoke", SpriteProtocolSmoke.run(port))
    end)
  end

  defp run_mode(port, :touch_calibrate) do
    with_boot_dims(port, fn w, h ->
      step("touch_calibrate", Smoke.calibrate_touch(port, w, h))
    end)
  end

  defp run_mode(port, :japanese_text) do
    with_boot_dims(port, fn w, h ->
      step("japanese_text", JapaneseText.run(port, w, h))
    end)
  end

  defp run_mode(port, :all) do
    with_boot_dims(port, fn w, h ->
      with :ok <- step("smoke", Smoke.run(port, w, h)),
           :ok <- step("sprite_protocol_smoke", SpriteProtocolSmoke.run(port)) do
        :ok
      end
    end)
  end

  defp run_mode(_port, mode) do
    log_info("unknown mode=#{inspect(mode)}")
    log_info("valid modes: #{Enum.map_join(@valid_modes, ", ", &inspect/1)}")
    {:error, {:unknown_mode, mode}}
  end

  defp run_protocol_only(port) do
    with :ok <- step("ping", AtomLGFX.ping(port)),
         :ok <- step("protocol_smoke", ProtocolSmoke.run(port)),
         {:ok, w, h} <- init_display_after_protocol_with_dims(port),
         :ok <- ProtocolSmoke.draw_summary(port, w, h) do
      :ok
    end
  end

  defp init_display_after_protocol_with_dims(port) do
    with :ok <- step("init", AtomLGFX.init(port)),
         :ok <- step("display(init)", AtomLGFX.display(port)),
         {:ok, w0, h0} <- get_wh(port),
         :ok <- log_before_rotation(w0, h0),
         {:ok, rotation, w, h} <- configure_display_rotation(port, w0, h0) do
      finalize_boot_state(port, rotation, w, h)
      {:ok, w, h}
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
    with :ok <- step("ping", AtomLGFX.ping(port)),
         :ok <- step("protocol_smoke", ProtocolSmoke.run(port)),
         :ok <- step("init", AtomLGFX.init(port)),
         :ok <- step("write_session_smoke", Smoke.write_session(port)),
         :ok <- step("display(init)", AtomLGFX.display(port)) do
      :ok
    end
  end

  defp finalize_boot_state(port, rotation, w, h) do
    :erlang.put({:sample_app_rotation, port}, rotation)

    log_info("selected rotation=#{rotation} viewport=#{w}x#{h}")

    _ = AtomLGFX.fill_screen(port, @bg)
    _ = AtomLGFX.reset_text_state(port, 0)
    _ = AtomLGFX.set_text_wrap(port, false, 0)
    _ = AtomLGFX.set_text_size(port, 2, 0)
    _ = AtomLGFX.set_text_color(port, @fg, nil, 0)

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
    with :ok <- AtomLGFX.set_rotation(port, rotation),
         :ok <- AtomLGFX.display(port),
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
    case rotation_preference() do
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

  defp rotation_preference do
    case :erlang.get(:sample_app_rotation_preference) do
      value when value in [:auto_landscape, :landscape_cw, :landscape_ccw] -> value
      value when is_integer(value) and value in 0..7 -> value
      _other -> @rotation_preference
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

  defp get_wh(port) do
    with {:ok, w} <- AtomLGFX.width(port, 0),
         {:ok, h} <- AtomLGFX.height(port, 0) do
      {:ok, w, h}
    end
  end

  defp safe_close_port(port) do
    case AtomLGFX.close(port) do
      :ok ->
        log_info("AtomLGFX closed")
        :ok

      {:error, reason} ->
        log_failure("AtomLGFX close failed", reason)
        :ok
    end
  end

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
    IO.puts("#{prefix}: #{AtomLGFX.format_error(reason)}")
  end
end
