defmodule SampleApp.Assets do
  @moduledoc false

  @icon_w 32
  @icon_h 32

  # Assumes this file lives at: lib/sample_app/assets.ex
  # So: __DIR__ = .../lib/sample_app
  # And priv is at project root: ../../priv/...
  @icons_dir Path.expand("../../priv/assets/icons", __DIR__)

  @info_path Path.join(@icons_dir, "info.rgb565")
  @alert_path Path.join(@icons_dir, "alert.rgb565")
  @close_path Path.join(@icons_dir, "close.rgb565")

  @external_resource @info_path
  @external_resource @alert_path
  @external_resource @close_path

  # Compile-time embed: binaries end up inside the BEAM.
  @info File.read!(@info_path)
  @alert File.read!(@alert_path)
  @close File.read!(@close_path)

  def icon_w, do: @icon_w
  def icon_h, do: @icon_h

  def icon(:info), do: @info
  def icon(:alert), do: @alert
  def icon(:close), do: @close
end
