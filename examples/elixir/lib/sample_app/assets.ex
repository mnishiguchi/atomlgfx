defmodule SampleApp.Assets do
  @moduledoc false

  @icon_w 32
  @icon_h 32

  @icons_dir Application.app_dir(:sample_app, "priv/assets/icons")
  @icon_names [:info, :alert, :close, :piyopiyo]

  @icon_paths Map.new(@icon_names, &{&1, Path.join(@icons_dir, "#{&1}.rgb565")})

  Enum.each(Map.values(@icon_paths), fn path ->
    Module.put_attribute(__MODULE__, :external_resource, path)
  end)

  @icons Map.new(@icon_names, fn name -> {name, File.read!(Map.fetch!(@icon_paths, name))} end)

  def icon_w, do: @icon_w
  def icon_h, do: @icon_h

  def icon(name), do: Map.fetch!(@icons, name)
end
