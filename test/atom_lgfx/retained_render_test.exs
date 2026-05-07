# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.RetainedRenderTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.InstanceBuffer
  alias AtomLGFX.ObjectBuffers
  alias AtomLGFX.RenderProgram
  alias AtomLGFX.RenderScene

  test "encodes sprite_transform_2d object records from natural values" do
    assert {:ok, encoded} =
             ObjectBuffers.encode_sprite_transform_2d_objects([
               {2, 10, -20, 3, -4, 90.0, 1.5, -1.25, 0.25}
             ])

    assert encoded ==
             <<2, 0, 10::little-signed-16, -20::little-signed-16, 3::little-signed-16,
               -4::little-signed-16, 9000::little-16, 1536::little-16, -125::little-signed-16,
               256::little-signed-16>>
  end

  test "encodes empty retained object lists as an empty binary" do
    assert {:ok, <<>>} = ObjectBuffers.encode_sprite_transform_2d_objects([])
  end

  test "decodes retained render-program stats payloads" do
    assert {:ok, stats} =
             RenderProgram.decode_stats(
               {:render_program_stats, true, 12, 50, 44, 6, 160, 410_000, 1_200, 125_000, 284_000}
             )

    assert stats.running == true
    assert stats.frame_count == 12
    assert stats.object_count == 50
    assert stats.drawn_count == 44
    assert stats.culled_count == 6
    assert stats.strip_height == 160
    assert stats.last_frame_us == 410_000
    assert stats.last_update_us == 1_200
    assert stats.last_draw_us == 125_000
    assert stats.last_present_us == 284_000
  end

  test "exposes friendlier retained render-scene helpers" do
    opts = [
      renderer: :sprite_transform,
      instance_buffer: 1,
      sprites: [1, 2, 3],
      strip_height: 160,
      background_color: 0x0000,
      transparent_color: 0x0000,
      motion: :bounce,
      zoom_min: 0.5,
      zoom_max: 2.0
    ]

    assert {:module, RenderScene} = Code.ensure_loaded(RenderScene)
    assert {:module, InstanceBuffer} = Code.ensure_loaded(InstanceBuffer)

    assert {:create, 2} in RenderScene.__info__(:functions)
    assert {:start, 2} in RenderScene.__info__(:functions)
    assert {:stop, 2} in RenderScene.__info__(:functions)
    assert {:stats, 2} in RenderScene.__info__(:functions)

    assert {:create, 2} in InstanceBuffer.__info__(:functions)
    assert {:write, 3} in InstanceBuffer.__info__(:functions)
    assert {:delete, 2} in InstanceBuffer.__info__(:functions)

    assert is_list(opts)
  end
end
