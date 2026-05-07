# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.RenderScene do
  @moduledoc """
  Retained native render-scene lifecycle helpers.

  A render scene is configured from Elixir and then rendered by native code.
  It is intended for hot display loops where sending one frame from Elixir each
  time would be too expensive.
  """

  alias AtomLGFX.RenderProgram

  @doc """
  Creates a retained native render scene.

  User-facing option names describe the scene:

  - `:renderer` selects what the scene draws, currently `:sprite_transform`
  - `:instance_buffer` is the retained native instance buffer handle
  - `:sprites` is the list of source sprite handles
  - `:motion` selects native per-frame motion, currently `:none` or `:bounce`

  The older lower-level names `:type`, `:object_buffer`, `:sources`, and
  `:update` are also accepted for compatibility with the protocol helpers.
  """
  def create(port, opts) when is_list(opts) do
    RenderProgram.create(port, normalize_create_opts(opts))
  end

  def start(port, scene, opts \\ []), do: RenderProgram.start(port, scene, opts)
  def stop(port, scene), do: RenderProgram.stop(port, scene)
  def destroy(port, scene), do: RenderProgram.destroy(port, scene)
  def stats(port, scene), do: RenderProgram.stats(port, scene)

  defp normalize_create_opts(opts) do
    opts
    |> put_if_present(:type, normalize_renderer(Keyword.get(opts, :renderer)))
    |> put_if_present(:object_buffer, Keyword.get(opts, :instance_buffer))
    |> put_if_present(:sources, Keyword.get(opts, :sprites))
    |> put_if_present(:update, Keyword.get(opts, :motion))
  end

  defp normalize_renderer(nil), do: nil
  defp normalize_renderer(:sprite_transform), do: :striped_sprite_transform
  defp normalize_renderer(other), do: other

  defp put_if_present(opts, _key, nil), do: opts
  defp put_if_present(opts, key, value), do: Keyword.put_new(opts, key, value)
end
