defmodule SampleApp do
  @moduledoc false

  alias LGFXPort, as: Port
  alias SampleApp.ProtocolSmoke
  alias SampleApp.SpriteProtocolSmoke
  alias SampleApp.PushImageStress
  alias SampleApp.DrawStringStress
  alias SampleApp.TextProbe
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
  # Modes:
  # - :protocol      -> protocol-only checks (no display init)
  # - :boot          -> init + display + rotation baseline
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

  defp run_mode(port, :protocol) do
    with :ok <- step("ping", Port.ping(port)),
         :ok <- step("protocol_smoke", ProtocolSmoke.run(port)) do
      :ok
    end
  end

  defp run_mode(port, :boot) do
    boot_for_display(port)
  end

  defp run_mode(port, :sprites) do
    with :ok <- boot_for_display(port),
         :ok <- step("sprite_protocol_smoke", SpriteProtocolSmoke.run(port)) do
      :ok
    end
  end

  defp run_mode(port, :text) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- step("text_probe", TextProbe.run(port, w, h)) do
      :ok
    end
  end

  defp run_mode(port, :touch) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- step("touch_probe", TouchProbe.run(port, w, h)) do
      :ok
    end
  end

  defp run_mode(port, :calibrate) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- step("touch_calibrate", TouchCalibrate.run(port, w, h)) do
      :ok
    end
  end

  defp run_mode(port, :stress) do
    with :ok <- boot_for_display(port),
         :ok <- step("push_image_stress", PushImageStress.run(port, @push_rounds)),
         :ok <- step("draw_string_stress", DrawStringStress.run(port, @text_rounds)) do
      :ok
    end
  end

  defp run_mode(port, :moving_icons) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- MovingIcons.run(port, w, h) do
      :ok
    end
  end

  defp run_mode(port, :all) do
    with {:ok, w, h} <- boot_for_display_with_dims(port),
         :ok <- step("push_image_stress", PushImageStress.run(port, @push_rounds)),
         :ok <- step("draw_string_stress", DrawStringStress.run(port, @text_rounds)),
         :ok <- step("sprite_protocol_smoke", SpriteProtocolSmoke.run(port)),
         :ok <- MovingIcons.run(port, w, h) do
      :ok
    end
  end

  defp run_mode(_port, mode) do
    IO.puts("unknown mode=#{inspect(mode)}")

    IO.puts(
      "valid modes: :protocol, :boot, :sprites, :text, :touch, :calibrate, :stress, :moving_icons, :all"
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
         {:ok, w, h} <- final_dims_after_rotation(port) do
      _ = Port.fill_screen(port, @bg)
      _ = Port.reset_text_state(port, 0)
      _ = Port.set_text_wrap(port, false, 0)
      _ = Port.set_text_size(port, 2, 0)
      _ = Port.set_text_color(port, @fg, nil, 0)

      {:ok, w, h}
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

  defp final_dims_after_rotation(port) do
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
