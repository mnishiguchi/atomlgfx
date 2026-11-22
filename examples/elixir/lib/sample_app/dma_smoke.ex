# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule SampleApp.DmaSmoke do
  @moduledoc false

  alias AtomLGFX.BinaryBatch, as: Batch

  # RGB565 で 38,400 bytes。SCT の 32 KB 境界も越える。
  @width 160
  @height 120
  @target 1
  @rounds 40

  def run(handle, _width, _height) do
    IO.puts("dma_smoke start rounds=#{@rounds} source_bytes=#{@width * @height * 2}")
    :ok = check(:start_write, AtomLGFX.Raw.call(handle, :start_write))
    :ok = check(:start_write, AtomLGFX.Raw.call(handle, :start_write))

    try do
      :ok = loop(handle, @rounds)
      IO.puts("dma_smoke ok direct=batch source_reuse=delete=recreate=depth_change")
      :ok
    after
      _ = AtomLGFX.delete_sprite(handle, @target)
      _ = AtomLGFX.Raw.call(handle, :end_write)
      _ = AtomLGFX.Raw.call(handle, :end_write)
    end
  end

  defp check(_step, :ok), do: :ok
  defp check(_step, {:ok, :ok}), do: :ok

  defp check(step, result) do
    IO.puts("dma_smoke failed step=#{step} result=#{inspect(result)}")
    :erlang.error(:dma_smoke_failed)
  end

  defp loop(_handle, 0), do: :ok

  defp loop(handle, remaining) do
    :ok = check(:create_sprite, AtomLGFX.create_sprite(handle, @width, @height, 16, @target))
    :ok = check(:fill_screen, AtomLGFX.fill_screen(handle, 0xF800, @target))
    :ok = check(:push_sprite, AtomLGFX.push_sprite(handle, @target, 0, 0))
    # 外側の書き込み区間を閉じずに、転送元を直ちに変更する。
    :ok = check(:fill_screen, AtomLGFX.fill_screen(handle, 0x001F, @target))
    :ok = check(:push_sprite, AtomLGFX.push_sprite(handle, @target, 160, 0))
    :ok = check(:set_color_depth, AtomLGFX.set_color_depth(handle, 8, @target))
    :ok = check(:delete_sprite, AtomLGFX.delete_sprite(handle, @target))
    :ok = check(:create_sprite, AtomLGFX.create_sprite(handle, @width, @height, 16, @target))

    :ok =
      Batch.render(handle, [
        Batch.target(@target),
        Batch.fill_screen(0x07E0),
        Batch.target(0),
        Batch.push_sprite(@target, 0, 120),
        Batch.target(@target),
        Batch.fill_screen(0xFFFF),
        Batch.target(0),
        Batch.push_sprite(@target, 160, 120)
      ])

    :ok = check(:delete_sprite, AtomLGFX.delete_sprite(handle, @target))
    loop(handle, remaining - 1)
  end
end
