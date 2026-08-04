# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.ColorTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.Color

  describe "normalize_display/1" do
    test "accepts named RGB565 colors" do
      assert Color.normalize_display(:red) == {:ok, 0xF800}
      assert Color.normalize_display(:light_gray) == {:ok, 0xC618}
    end

    test "accepts RGB tuple forms" do
      assert Color.normalize_display({:rgb, 255, 0, 0}) == {:ok, 0xF800}
      assert Color.normalize_display({:rgb565, 255, 255, 255}) == {:ok, 0xFFFF}
      assert Color.normalize_display({:rgb888, 0x00FF00}) == {:ok, 0x07E0}
    end

    test "rejects invalid display colors" do
      assert Color.normalize_display({:rgb, 256, 0, 0}) ==
               {:error, {:bad_display_color, {:rgb, 256, 0, 0}}}
    end
  end

  describe "normalize_palette/1" do
    test "accepts named colors as RGB888 palette colors" do
      assert Color.normalize_palette(:red) == {:ok, 0xFF0000}
      assert Color.normalize_palette(:white) == {:ok, 0xFFFFFF}
    end

    test "accepts RGB tuple forms" do
      assert Color.normalize_palette({:rgb, 255, 128, 0}) == {:ok, 0xFF8000}
      assert Color.normalize_palette({:rgb888, 0x0000FF}) == {:ok, 0x0000FF}
      assert Color.normalize_palette({:rgb565, 0x07E0}) == {:ok, 0x00FF00}
    end

    test "rejects invalid palette colors" do
      assert Color.normalize_palette({:rgb888, 0x01000000}) ==
               {:error, {:bad_palette_color, {:rgb888, 0x01000000}}}
    end
  end
end
