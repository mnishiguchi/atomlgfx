# /examples/elixir/lib/sample_app.ex
defmodule SampleApp do
  @moduledoc false

  alias SampleApp.Port
  alias SampleApp.ProtocolSmoke
  alias SampleApp.SpriteProtocolSmoke
  alias SampleApp.PushImageStress
  alias SampleApp.DrawStringStress
  alias SampleApp.TextSmoke
  alias SampleApp.FontProbe
  alias SampleApp.MovingIcons
  alias SampleApp.TouchProbe
  alias SampleApp.TouchCalibrate

  @default_mode :all

  @push_rounds 300
  @text_rounds 500

  @bg 0x000000
  @fg 0xFFFFFF

  # -----------------------------------------------------------------------------
  # Public entry
  # -----------------------------------------------------------------------------

  # AtomVM app entrypoint uses this.
  #
  # Common modes:
  # - :smoke  -> protocol + display boot + sprite smoke
  # - :stress -> push_image + draw_string stress
  # - :text   -> deterministic text rendering smoke
  # - :fonts  -> font probe matrix
  # - :demo   -> moving_icons only
  # - :touch  -> touch probe (poll + draw markers)
  # - :all    -> full_demo (stress + sprite smoke + moving_icons)
  #
  # Canonical modes also supported:
  # :protocol_smoke, :display_boot, :sprite_protocol_smoke,
  # :push_image_stress, :draw_string_stress, :stress_suite,
  # :unicode_probe, :text_smoke, :font_probe, :moving_icons, :full_demo,
  # :touch_probe, :touch_calibrate
  def start do
    start(@default_mode)
  end

  def start(mode) when is_atom(mode) do
    port = Port.open()
    IO.puts("Port opened")

    try do
      case run_mode(port, mode) do
        :ok ->
          :ok

        {:error, reason} = err ->
          IO.puts("sample_app failed mode=#{inspect(mode)} reason=#{Port.format_error(reason)}")
          err
      end
    after
      safe_close_port(port)
    end
  end

  # -----------------------------------------------------------------------------
  # Mode dispatch
  # -----------------------------------------------------------------------------

  # Friendly aliases
  defp run_mode(port, :smoke), do: run_mode(port, :smoke_suite)
  defp run_mode(port, :stress), do: run_mode(port, :stress_suite)
  defp run_mode(port, :text), do: run_mode(port, :text_smoke)
  defp run_mode(port, :fonts), do: run_mode(port, :font_probe)
  defp run_mode(port, :demo), do: run_mode(port, :moving_icons)
  defp run_mode(port, :all), do: run_mode(port, :full_demo)
  defp run_mode(port, :touch), do: run_mode(port, :touch_probe)

  # Bring-up only (protocol smoke + init/display/rotation/text baseline)
  defp run_mode(port, :display_boot) do
    boot_for_display(port)
  end

  # Protocol-only checks (no display init required)
  defp run_mode(port, :protocol_smoke) do
    with :ok <- step("ping", Port.ping(port)),
         :ok <- step("protocol_smoke", ProtocolSmoke.run(port)) do
      :ok
    end
  end

  # Display bring-up + sprite protocol checks
  defp run_mode(port, :sprite_protocol_smoke) do
    with :ok <- boot_for_display(port),
         :ok <- step("sprite_protocol_smoke", SpriteProtocolSmoke.run(port)) do
      :ok
    end
  end

  # Daily smoke path (boot_for_display already includes protocol_smoke)
  defp run_mode(port, :smoke_suite) do
    with :ok <- boot_for_display(port),
         :ok <- step("sprite_protocol_smoke", SpriteProtocolSmoke.run(port)) do
      :ok
    end
  end

  defp run_mode(port, :push_image_stress) do
    with :ok <- boot_for_display(port),
         :ok <- step("push_image_stress", PushImageStress.run(port, @push_rounds)) do
      :ok
    end
  end

  defp run_mode(port, :draw_string_stress) do
    with :ok <- boot_for_display(port),
         :ok <- step("draw_string_stress", DrawStringStress.run(port, @text_rounds)) do
      :ok
    end
  end

  defp run_mode(port, :stress_suite) do
    with :ok <- boot_for_display(port),
         :ok <- step("push_image_stress", PushImageStress.run(port, @push_rounds)),
         :ok <- step("draw_string_stress", DrawStringStress.run(port, @text_rounds)) do
      :ok
    end
  end

  defp run_mode(port, :unicode_probe) do
    with :ok <- boot_for_display(port),
         :ok <- step("unicode_probe", unicode_probe(port)) do
      :ok
    end
  end

  defp run_mode(port, :text_smoke) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- step("text_smoke", TextSmoke.run(port, w, h)) do
      :ok
    end
  end

  defp run_mode(port, :font_probe) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- step("font_probe", FontProbe.run(port, w, h)) do
      :ok
    end
  end

  # Touch probe (poll + marker drawing)
  defp run_mode(port, :touch_probe) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- step("touch_probe", TouchProbe.run(port, w, h)) do
      :ok
    end
  end

  # Interactive calibration (blocking on device) + apply params
  defp run_mode(port, :touch_calibrate) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- step("touch_calibrate", TouchCalibrate.run(port, w, h)) do
      :ok
    end
  end

  # Run stress tests first, then enter the moving demo loop
  defp run_mode(port, :full_demo) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- step("push_image_stress", PushImageStress.run(port, @push_rounds)),
         :ok <- step("draw_string_stress", DrawStringStress.run(port, @text_rounds)),
         :ok <- step("sprite_protocol_smoke", SpriteProtocolSmoke.run(port)),
         :ok <- MovingIcons.run(port, w, h) do
      :ok
    end
  end

  # Interactive demo only
  defp run_mode(port, :moving_icons) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- MovingIcons.run(port, w, h) do
      :ok
    end
  end

  defp run_mode(_port, mode) do
    IO.puts("unknown mode=#{inspect(mode)}")

    IO.puts(
      "valid modes: :protocol_smoke, :display_boot, :sprite_protocol_smoke, :smoke_suite (:smoke), " <>
        ":push_image_stress, :draw_string_stress, :stress_suite (:stress), :unicode_probe, " <>
        ":text_smoke (:text), :font_probe (:fonts), :moving_icons (:demo), :full_demo (:all), " <>
        ":touch_probe (:touch), :touch_calibrate"
    )

    {:error, {:unknown_mode, mode}}
  end

  # -----------------------------------------------------------------------------
  # Common boot sequence
  # -----------------------------------------------------------------------------

  defp boot_for_display(port) do
    case boot_for_display_with_dims(port) do
      {:ok, _w, _h} -> :ok
      {:error, _reason} = err -> err
    end
  end

  defp boot_for_display_with_dims(port) do
    with :ok <- step("ping", Port.ping(port)),
         :ok <- step("protocol_smoke", ProtocolSmoke.run(port)),
         :ok <- step("init", Port.init(port)),
         :ok <- step("display(init)", Port.display(port)),
         {:ok, w0, h0} <- get_wh(port),
         :ok <- maybe_log_before_rotation(w0, h0),
         :ok <- set_rotation_for_dims(port, w0, h0),
         {:ok, w, h} <- final_dims_after_rotation(port, w0, h0) do
      _ = Port.fill_screen(port, @bg)
      _ = Port.reset_text_state(port, 0)
      _ = Port.set_text_wrap(port, false, 0)
      _ = Port.set_text_size(port, 2, 0)
      _ = Port.set_text_color(port, @fg, nil, 0)

      {:ok, w, h}
    end
  end

  # -----------------------------------------------------------------------------
  # Unicode probe (UTF-8 transport + JP font preset check)
  # -----------------------------------------------------------------------------

  defp unicode_probe(port) do
    with :ok <- Port.fill_screen(port, @bg),
         :ok <- Port.reset_text_state(port, 0),
         :ok <- Port.set_text_wrap(port, false, 0),
         :ok <- Port.set_text_color(port, @fg, nil, 0),
         :ok <- Port.set_text_font(port, 1, 0),
         :ok <- Port.set_text_size(port, 1, 0),
         :ok <- Port.draw_string(port, 4, 4, "ASCII: ABC 123 !?", 0),
         :ok <- maybe_use_japanese_font_preset(port),
         # Do not call set_text_size here: presets intentionally set size on the device.
         :ok <- Port.draw_string(port, 4, 40, "日本語: 設定 戻る 次へ", 0),
         :ok <- Port.draw_string(port, 4, 72, "確認 更新 取消 決定", 0),
         :ok <- Port.draw_string(port, 4, 104, "状態 接続 切断 電池 充電", 0),
         :ok <- Port.draw_string(port, 4, 136, "通知 警報 異常 正常", 0),
         :ok <- Port.draw_string(port, 4, 168, "ひらがな カタカナ 漢字", 0),
         :ok <- Port.draw_string(port, 4, 200, "。、！？「」（）ー・", 0) do
      IO.puts(
        "unicode_probe bytes jp1=#{byte_size("日本語: 設定 戻る 次へ")} jp2=#{byte_size("ひらがな カタカナ 漢字")}"
      )

      :ok
    end
  end

  defp maybe_use_japanese_font_preset(port) do
    # Prefer small first for UI legibility; fall back to the others.
    try_japanese_font_presets(port, [:jp_small, :jp_medium, :jp_large, :jp])
  end

  defp try_japanese_font_presets(_port, []) do
    IO.puts("unicode_probe warning: no Japanese font preset available, tofu is expected")
    :ok
  end

  defp try_japanese_font_presets(port, [preset | rest]) do
    case Port.set_font_preset(port, preset, 0) do
      :ok ->
        IO.puts("unicode_probe using font preset #{inspect(preset)}")
        :ok

      {:error, reason} ->
        IO.puts(
          "unicode_probe preset #{inspect(preset)} unavailable: #{Port.format_error(reason)}"
        )

        try_japanese_font_presets(port, rest)
    end
  end

  # -----------------------------------------------------------------------------
  # Boot helpers
  # -----------------------------------------------------------------------------

  defp maybe_log_before_rotation(w0, h0) do
    IO.puts("before rotation: w=#{w0} h=#{h0}")
    :ok
  end

  defp set_rotation_for_dims(port, w0, h0) do
    rotation = choose_rotation(w0, h0)

    with :ok <- step("set_rotation", Port.set_rotation(port, rotation)),
         :ok <- step("display(rot)", Port.display(port)) do
      :erlang.put({:sample_app_rotation, port}, rotation)
      :ok
    end
  end

  defp final_dims_after_rotation(port, _w0, _h0) do
    with {:ok, w1, h1} <- get_wh(port) do
      rotation =
        case :erlang.get({:sample_app_rotation, port}) do
          r when is_integer(r) -> r
          _ -> 0
        end

      {w, h} = normalize_dims_for_rotation(rotation, w1, h1)
      IO.puts("after rotation:  w=#{w1} h=#{h1} (using #{w}x#{h})")
      {:ok, w, h}
    end
  end

  defp step(label, :ok) do
    IO.puts("#{label} ok")
    :ok
  end

  defp step(label, {:error, reason} = err) do
    IO.puts("#{label} failed: #{Port.format_error(reason)}")
    err
  end

  defp get_wh(port) do
    with {:ok, w} <- Port.width(port, 0),
         {:ok, h} <- Port.height(port, 0) do
      {:ok, w, h}
    end
  end

  defp choose_rotation(w, h) do
    if w < h do
      1
    else
      0
    end
  end

  # If rotation is odd (90/270), the logical drawing space should be landscape.
  # If the driver already reports rotated width/height, this is a no-op.
  # If it reports raw panel dimensions, this swaps them for correct bounds.
  defp normalize_dims_for_rotation(rotation, w, h) when is_integer(rotation) do
    if rem(rotation, 2) == 1 and w < h do
      {h, w}
    else
      {w, h}
    end
  end

  defp safe_close_port(port) do
    case Port.close(port) do
      :ok ->
        IO.puts("Port closed")
        :ok

      {:error, reason} ->
        IO.puts("Port close failed: #{Port.format_error(reason)}")
        :ok
    end
  end
end
