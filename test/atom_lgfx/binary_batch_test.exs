# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatchTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.BinaryBatch

  describe "binary-batch control commands" do
    test "encodes target as a render-private command" do
      assert BinaryBatch.target(1) == <<0xF0, 1>>
    end

    test "encodes color mode as render-private commands" do
      assert BinaryBatch.color_mode(:rgb565) == <<0xF1, 0>>
      assert BinaryBatch.color_mode(:palette_index) == <<0xF1, 1>>
    end

    test "encodes native strip presentation commands as render-private commands" do
      assert BinaryBatch.begin_strip(160) == <<0xF5, 160::little-16>>
      assert BinaryBatch.present_strip() == <<0xF6>>
    end

    test "encodes display as the protocol display opcode" do
      assert BinaryBatch.display() == <<13>>
    end
  end

  describe "scalar command builders" do
    test "encodes draw pixel lists as compact render-private records" do
      assert BinaryBatch.draw_pixel_list([
               {1, 2, 0xF800},
               {-5, -6, 0x07E0}
             ]) ==
               <<0xF9, 0::little-16, 2::little-16, 1::little-signed-16, 2::little-signed-16,
                 0xF800::little-16, -5::little-signed-16, -6::little-signed-16,
                 0x07E0::little-16>>
    end

    test "encodes draw rectangle lists as compact render-private records" do
      assert BinaryBatch.draw_rect_list([
               {1, 2, 3, 4, 0xF800},
               {-5, -6, 7, 8, 0x07E0}
             ]) ==
               <<0xFA, 0::little-16, 2::little-16, 1::little-signed-16, 2::little-signed-16,
                 3::little-16, 4::little-16, 0xF800::little-16, -5::little-signed-16,
                 -6::little-signed-16, 7::little-16, 8::little-16, 0x07E0::little-16>>
    end

    test "encodes fill rectangle lists as compact render-private records" do
      assert BinaryBatch.fill_rect_list([
               {1, 2, 3, 4, 0xF800},
               {-5, -6, 7, 8, 0x07E0}
             ]) ==
               <<0xF7, 0::little-16, 2::little-16, 1::little-signed-16, 2::little-signed-16,
                 3::little-16, 4::little-16, 0xF800::little-16, -5::little-signed-16,
                 -6::little-signed-16, 7::little-16, 8::little-16, 0x07E0::little-16>>
    end

    test "encodes draw circle lists as compact render-private records" do
      assert BinaryBatch.draw_circle_list([
               {1, 2, 3, 0xF800},
               {-5, -6, 7, 0x07E0}
             ]) ==
               <<0xFC, 0::little-16, 2::little-16, 1::little-signed-16, 2::little-signed-16,
                 3::little-16, 0xF800::little-16, -5::little-signed-16, -6::little-signed-16,
                 7::little-16, 0x07E0::little-16>>
    end

    test "encodes fill circle lists as compact render-private records" do
      assert BinaryBatch.fill_circle_list([
               {1, 2, 3, 0xF800},
               {-5, -6, 7, 0x07E0}
             ]) ==
               <<0xFB, 0::little-16, 2::little-16, 1::little-signed-16, 2::little-signed-16,
                 3::little-16, 0xF800::little-16, -5::little-signed-16, -6::little-signed-16,
                 7::little-16, 0x07E0::little-16>>
    end

    test "encodes draw ellipse lists as compact render-private records" do
      assert BinaryBatch.draw_ellipse_list([
               {1, 2, 3, 4, 0xF800},
               {-5, -6, 7, 8, 0x07E0}
             ]) ==
               <<0xFF, 0, 0, 0, 0::little-16, 2::little-16, 1::little-signed-16,
                 2::little-signed-16, 3::little-16, 4::little-16, 0xF800::little-16,
                 -5::little-signed-16, -6::little-signed-16, 7::little-16, 8::little-16,
                 0x07E0::little-16>>
    end

    test "encodes fill ellipse lists as compact render-private records" do
      assert BinaryBatch.fill_ellipse_list([
               {1, 2, 3, 4, 0xF800},
               {-5, -6, 7, 8, 0x07E0}
             ]) ==
               <<0xFF, 0, 1, 0, 0::little-16, 2::little-16, 1::little-signed-16,
                 2::little-signed-16, 3::little-16, 4::little-16, 0xF800::little-16,
                 -5::little-signed-16, -6::little-signed-16, 7::little-16, 8::little-16,
                 0x07E0::little-16>>
    end

    test "encodes draw line lists as compact render-private records" do
      assert BinaryBatch.draw_line_list([
               {1, 2, 3, 4, 0xF800},
               {-5, -6, 7, 8, 0x07E0}
             ]) ==
               <<0xF8, 0::little-16, 2::little-16, 1::little-signed-16, 2::little-signed-16,
                 3::little-signed-16, 4::little-signed-16, 0xF800::little-16,
                 -5::little-signed-16, -6::little-signed-16, 7::little-signed-16,
                 8::little-signed-16, 0x07E0::little-16>>
    end

    test "encodes draw triangle lists as compact render-private records" do
      assert BinaryBatch.draw_triangle_list([
               {1, 2, 3, 4, 5, 6, 0xF800},
               {-5, -6, 7, 8, 9, 10, 0x07E0}
             ]) ==
               <<0xFE, 0::little-16, 2::little-16, 1::little-signed-16, 2::little-signed-16,
                 3::little-signed-16, 4::little-signed-16, 5::little-signed-16,
                 6::little-signed-16, 0xF800::little-16, -5::little-signed-16,
                 -6::little-signed-16, 7::little-signed-16, 8::little-signed-16,
                 9::little-signed-16, 10::little-signed-16, 0x07E0::little-16>>
    end

    test "encodes fill triangle lists as compact render-private records" do
      assert BinaryBatch.fill_triangle_list([
               {1, 2, 3, 4, 5, 6, 0xF800},
               {-5, -6, 7, 8, 9, 10, 0x07E0}
             ]) ==
               <<0xFD, 0::little-16, 2::little-16, 1::little-signed-16, 2::little-signed-16,
                 3::little-signed-16, 4::little-signed-16, 5::little-signed-16,
                 6::little-signed-16, 0xF800::little-16, -5::little-signed-16,
                 -6::little-signed-16, 7::little-signed-16, 8::little-signed-16,
                 9::little-signed-16, 10::little-signed-16, 0x07E0::little-16>>
    end

    test "encodes round rectangle commands as fixed-width little-endian records" do
      assert BinaryBatch.draw_round_rect(10, 20, 30, 40, 5, 0xFFFF) ==
               <<22, 10::little-signed-16, 20::little-signed-16, 30::little-16, 40::little-16,
                 5::little-16, 0xFFFF::little-16>>

      assert BinaryBatch.fill_round_rect(10, 20, 30, 40, 5, 0x07E0) ==
               <<23, 10::little-signed-16, 20::little-signed-16, 30::little-16, 40::little-16,
                 5::little-16, 0x07E0::little-16>>
    end

    test "encodes ellipse commands as fixed-width little-endian records" do
      assert BinaryBatch.draw_ellipse(160, 120, 60, 30, 0xFFFF) ==
               <<26, 160::little-signed-16, 120::little-signed-16, 60::little-16, 30::little-16,
                 0xFFFF::little-16>>

      assert BinaryBatch.fill_ellipse(160, 120, 60, 30, 0x07E0) ==
               <<27, 160::little-signed-16, 120::little-signed-16, 60::little-16, 30::little-16,
                 0x07E0::little-16>>
    end

    test "encodes arc commands as fixed-width little-endian records" do
      assert BinaryBatch.draw_arc(160, 120, 60, 40, 45, 180.5, 0xFFFF) ==
               <<28, 160::little-signed-16, 120::little-signed-16, 60::little-16, 40::little-16,
                 45.0::little-float-32, 180.5::little-float-32, 0xFFFF::little-16>>

      assert BinaryBatch.fill_arc(160, 120, 60, 40, -90, 90, 0x07E0) ==
               <<29, 160::little-signed-16, 120::little-signed-16, 60::little-16, 40::little-16,
                 -90.0::little-float-32, 90.0::little-float-32, 0x07E0::little-16>>
    end

    test "encodes quadratic and cubic bezier commands as fixed-width little-endian records" do
      assert BinaryBatch.draw_bezier(0, 1, 2, 3, 4, 5, 0xFFFF) ==
               <<30, 3, 0, 0::little-signed-16, 1::little-signed-16, 2::little-signed-16,
                 3::little-signed-16, 4::little-signed-16, 5::little-signed-16,
                 0xFFFF::little-16>>

      assert BinaryBatch.draw_bezier(0, 1, 2, 3, 4, 5, 6, 7, 0x07E0) ==
               <<30, 4, 0, 0::little-signed-16, 1::little-signed-16, 2::little-signed-16,
                 3::little-signed-16, 4::little-signed-16, 5::little-signed-16,
                 6::little-signed-16, 7::little-signed-16, 0x07E0::little-16>>
    end
  end

  describe "binary-batch clip commands" do
    test "encodes clip commands as fixed-width records" do
      assert BinaryBatch.set_clip_rect(-2, 4, 120, 80) ==
               <<45, -2::little-signed-16, 4::little-signed-16, 120::little-16, 80::little-16>>

      assert BinaryBatch.clear_clip_rect() == <<46>>
    end
  end

  describe "binary-batch text commands" do
    test "encodes text datum, wrap, and cursor commands" do
      assert BinaryBatch.set_text_datum(4) == <<34, 4>>
      assert BinaryBatch.set_text_wrap(true) == <<35, 1, 0>>
      assert BinaryBatch.set_text_wrap_xy(true, true) == <<35, 1, 1>>
      assert BinaryBatch.set_cursor(-8, 16) == <<38, -8::little-signed-16, 16::little-signed-16>>
    end

    test "encodes text font preset commands" do
      assert BinaryBatch.set_text_font_preset(:ascii) == <<36, 0>>
      assert BinaryBatch.set_text_font_preset(:jp) == <<36, 1>>
    end

    test "encodes text size as x1024 fixed-point values" do
      assert BinaryBatch.set_text_size(1.5) == <<33, 1536::little-16, 1536::little-16>>
      assert BinaryBatch.set_text_size_xy(1, 2) == <<33, 1024::little-16, 2048::little-16>>
    end

    test "encodes RGB565 text color commands" do
      assert BinaryBatch.set_text_color(0xFFFF) == <<37, 0::little-16, 0xFFFF::little-16>>

      assert BinaryBatch.set_text_color(0xFFFF, 0x0000) ==
               <<37, 1::little-16, 0xFFFF::little-16, 0x0000::little-16>>
    end

    test "encodes palette-index text color commands" do
      assert BinaryBatch.set_text_color({:index, 1}, {:index, 0}) ==
               <<37, 13::little-16, 1::little-16, 0::little-16>>
    end

    test "encodes draw string as a length-prefixed UTF-8 payload" do
      assert BinaryBatch.draw_string(8, 16, "fps:60") ==
               <<40, 8::little-signed-16, 16::little-signed-16, 6::little-16, "fps:60">>
    end

    test "encodes print and println as length-prefixed UTF-8 payloads" do
      assert BinaryBatch.print("fps:60") == <<41, 6::little-16, "fps:60">>
      assert BinaryBatch.println("ok") == <<42, 2::little-16, "ok">>
      assert BinaryBatch.println() == <<42, 0::little-16>>
    end
  end

  describe "binary-batch image commands" do
    test "encodes JPEG draw commands as length-prefixed payloads" do
      jpeg = <<0xFF, 0xD8, 0xFF, 0xD9>>

      assert BinaryBatch.draw_jpg(-2, 3, jpeg) ==
               <<43, 0, 0, -2::little-signed-16, 3::little-signed-16, 4::little-32, jpeg::binary>>

      assert BinaryBatch.draw_jpg(-2, 3, 32, 24, 1, 2, 1.5, 2, jpeg) ==
               <<43, 1, 0, -2::little-signed-16, 3::little-signed-16, 32::little-16,
                 24::little-16, 1::little-signed-16, 2::little-signed-16, 1.5::little-float-32,
                 2.0::little-float-32, 4::little-32, jpeg::binary>>
    end

    test "encodes RGB565 push image with implicit stride" do
      pixels = <<0x00, 0xF8, 0xE0, 0x07, 0x1F, 0x00, 0xFF, 0xFF>>

      assert BinaryBatch.push_image_rgb565(-2, 3, 2, 2, pixels) ==
               <<44, -2::little-signed-16, 3::little-signed-16, 2::little-16, 2::little-16,
                 0::little-16, 8::little-32, pixels::binary>>
    end

    test "encodes RGB565 push image with explicit stride" do
      pixels = <<0x00, 0xF8, 0xE0, 0x07, 0x00, 0x00, 0x1F, 0x00, 0xFF, 0xFF, 0x00, 0x00>>

      assert BinaryBatch.push_image_rgb565(0, 1, 2, 2, pixels, 3) ==
               <<44, 0::little-signed-16, 1::little-signed-16, 2::little-16, 2::little-16,
                 3::little-16, 12::little-32, pixels::binary>>
    end
  end

  describe "binary-batch sprite commands" do
    test "encodes sprite state commands" do
      assert BinaryBatch.set_palette_color(3, 0x112233) == <<50, 3, 0, 0x112233::little-32>>

      assert BinaryBatch.set_palette_color(4, 0x11, 0x22, 0x33) ==
               <<50, 4, 0, 0x112233::little-32>>

      assert BinaryBatch.set_pivot(-8, 16) == <<51, -8::little-signed-16, 16::little-signed-16>>
    end

    test "encodes push sprite to the current render target" do
      assert BinaryBatch.push_sprite(1, -2, 300) == <<52, 1, 0xFE, 0xFF, 0x2C, 0x01>>
    end

    test "encodes push sprite with RGB565 transparent key" do
      assert BinaryBatch.push_sprite(1, -2, 300, 0xF81F) ==
               <<0xF2, 0::little-16, 1, -2::little-signed-16, 300::little-signed-16,
                 0xF81F::little-16>>
    end

    test "encodes push sprite with palette-index transparent key" do
      assert BinaryBatch.push_sprite(1, -2, 300, {:index, 3}) ==
               <<0xF2, 16::little-16, 1, -2::little-signed-16, 300::little-signed-16,
                 3::little-16>>
    end

    test "encodes push rotate zoom to the current render target" do
      assert BinaryBatch.push_rotate_zoom(1, -2, 300, 45, 1.5) ==
               <<53, 0, 0, 0::little-16, 1, 0, -2::little-signed-16, 300::little-signed-16,
                 45.0::little-float-32, 1.5::little-float-32, 1.5::little-float-32, 0::little-16>>

      assert BinaryBatch.push_rotate_zoom(1, -2, 300, 45, 1.5, 2) ==
               <<53, 0, 0, 0::little-16, 1, 0, -2::little-signed-16, 300::little-signed-16,
                 45.0::little-float-32, 1.5::little-float-32, 2.0::little-float-32, 0::little-16>>
    end

    test "encodes push rotate zoom with transparent keys" do
      assert BinaryBatch.push_rotate_zoom(1, -2, 300, 45, 1.5, 2, 0xF81F) ==
               <<53, 1, 0, 0::little-16, 1, 0, -2::little-signed-16, 300::little-signed-16,
                 45.0::little-float-32, 1.5::little-float-32, 2.0::little-float-32,
                 0xF81F::little-16>>

      assert BinaryBatch.push_rotate_zoom(1, -2, 300, 45, 1.5, 2, {:index, 3}) ==
               <<53, 1, 0, 16::little-16, 1, 0, -2::little-signed-16, 300::little-signed-16,
                 45.0::little-float-32, 1.5::little-float-32, 2.0::little-float-32, 3::little-16>>
    end

    test "encodes push sprite list records" do
      assert BinaryBatch.push_sprite_list([{1, -2, 300}, {2, 4, 5}]) ==
               <<
                 0xF3,
                 0::little-16,
                 0::little-16,
                 2::little-16,
                 1,
                 0,
                 -2::little-signed-16,
                 300::little-signed-16,
                 2,
                 0,
                 4::little-signed-16,
                 5::little-signed-16
               >>
    end

    test "encodes push sprite list with palette-index transparent key" do
      assert BinaryBatch.push_sprite_list([{1, -2, 300}], transparent: {:index, 3}) ==
               <<
                 0xF3,
                 17::little-16,
                 3::little-16,
                 1::little-16,
                 1,
                 0,
                 -2::little-signed-16,
                 300::little-signed-16
               >>
    end

    test "encodes push sprite region list records" do
      assert BinaryBatch.push_sprite_region_list([{1, 2, 3, 32, 16, -4, 300}],
               transparent: 0x0000
             ) ==
               <<
                 0xF4,
                 1::little-16,
                 0::little-16,
                 1::little-16,
                 1,
                 0,
                 2::little-16,
                 3::little-16,
                 32::little-16,
                 16::little-16,
                 -4::little-signed-16,
                 300::little-signed-16
               >>
    end

    test "rejects indexed transparent key for push sprite region list" do
      assert_raise ArgumentError, fn ->
        BinaryBatch.push_sprite_region_list([{1, 0, 0, 32, 32, 0, 0}], transparent: {:index, 0})
      end
    end

    test "encodes push rotate zoom list with fixed-width little-endian records" do
      instances = [
        {1, 10, -20, 9_000, 1_024, 2_048}
      ]

      command =
        BinaryBatch.push_rotate_zoom_list(instances,
          transparent: {:index, 0},
          y_offset: 32
        )

      assert command ==
               <<
                 58,
                 16::little-16,
                 ?P,
                 ?R,
                 ?Z,
                 ?L,
                 1,
                 1,
                 0::little-16,
                 32::little-signed-16,
                 1::little-16,
                 1,
                 0,
                 10::little-signed-16,
                 -20::little-signed-16,
                 9_000::little-16,
                 1_024::little-16,
                 2_048::little-16
               >>
    end

    test "encodes push rotate zoom list with approximate culling" do
      instances = [
        {1, 10, -20, 9_000, 1_024, 2_048}
      ]

      command =
        BinaryBatch.push_rotate_zoom_list(instances,
          transparent: {:index, 0},
          y_offset: 32,
          approx_cull: true
        )

      assert command ==
               <<
                 58,
                 16::little-16,
                 ?P,
                 ?R,
                 ?Z,
                 ?L,
                 1,
                 3,
                 0::little-16,
                 32::little-signed-16,
                 1::little-16,
                 1,
                 0,
                 10::little-signed-16,
                 -20::little-signed-16,
                 9_000::little-16,
                 1_024::little-16,
                 2_048::little-16
               >>
    end

    test "encodes native push rotate zoom frame strips" do
      instances = [
        {1, 10, -20, 9_000, 1_024, 2_048}
      ]

      command =
        BinaryBatch.push_rotate_zoom_frame_strips(instances,
          frame_height: 320,
          background: 0x0000,
          transparent: {:index, 0},
          approx_cull: true
        )

      assert command ==
               <<
                 0xFF,
                 0x01,
                 16::little-16,
                 ?P,
                 ?R,
                 ?Z,
                 ?F,
                 1,
                 3,
                 0::little-16,
                 320::little-16,
                 0::little-16,
                 1::little-16,
                 1,
                 0,
                 10::little-signed-16,
                 -20::little-signed-16,
                 9_000::little-16,
                 1_024::little-16,
                 2_048::little-16
               >>
    end
  end

  describe "decode/1" do
    test "decodes round rectangle and text state commands" do
      frame = [
        BinaryBatch.draw_round_rect(10, 20, 30, 40, 5, 0xFFFF),
        BinaryBatch.fill_round_rect(11, 21, 31, 41, 6, 0x07E0),
        BinaryBatch.set_text_datum(4),
        BinaryBatch.set_text_wrap_xy(true, false),
        BinaryBatch.set_cursor(-8, 16)
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 22,
                    op: :draw_round_rect,
                    x: 10,
                    y: 20,
                    width: 30,
                    height: 40,
                    radius: 5,
                    color: 0xFFFF
                  },
                  %{
                    index: 1,
                    opcode: 23,
                    op: :fill_round_rect,
                    x: 11,
                    y: 21,
                    width: 31,
                    height: 41,
                    radius: 6,
                    color: 0x07E0
                  },
                  %{index: 2, opcode: 34, op: :set_text_datum, datum: 4},
                  %{index: 3, opcode: 35, op: :set_text_wrap, wrap_x: true, wrap_y: false},
                  %{index: 4, opcode: 38, op: :set_cursor, x: -8, y: 16}
                ]}
    end

    test "decodes draw pixel list commands" do
      frame = [
        BinaryBatch.draw_pixel_list([
          {1, 2, 0xF800},
          {-5, -6, 0x07E0}
        ])
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 0xF9,
                    op: :draw_pixel_list,
                    flags: 0,
                    pixels: [
                      %{x: 1, y: 2, color: 0xF800},
                      %{x: -5, y: -6, color: 0x07E0}
                    ]
                  }
                ]}
    end

    test "decodes draw rectangle list commands" do
      frame = [
        BinaryBatch.draw_rect_list([
          {1, 2, 3, 4, 0xF800},
          {-5, -6, 7, 8, 0x07E0}
        ])
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 0xFA,
                    op: :draw_rect_list,
                    flags: 0,
                    rectangles: [
                      %{x: 1, y: 2, width: 3, height: 4, color: 0xF800},
                      %{x: -5, y: -6, width: 7, height: 8, color: 0x07E0}
                    ]
                  }
                ]}
    end

    test "decodes fill rectangle list commands" do
      frame = [
        BinaryBatch.fill_rect_list([
          {1, 2, 3, 4, 0xF800},
          {-5, -6, 7, 8, 0x07E0}
        ])
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 0xF7,
                    op: :fill_rect_list,
                    flags: 0,
                    rectangles: [
                      %{x: 1, y: 2, width: 3, height: 4, color: 0xF800},
                      %{x: -5, y: -6, width: 7, height: 8, color: 0x07E0}
                    ]
                  }
                ]}
    end

    test "decodes draw circle list commands" do
      frame = [
        BinaryBatch.draw_circle_list([
          {1, 2, 3, 0xF800},
          {-5, -6, 7, 0x07E0}
        ])
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 0xFC,
                    op: :draw_circle_list,
                    flags: 0,
                    circles: [
                      %{x: 1, y: 2, radius: 3, color: 0xF800},
                      %{x: -5, y: -6, radius: 7, color: 0x07E0}
                    ]
                  }
                ]}
    end

    test "decodes fill circle list commands" do
      frame = [
        BinaryBatch.fill_circle_list([
          {1, 2, 3, 0xF800},
          {-5, -6, 7, 0x07E0}
        ])
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 0xFB,
                    op: :fill_circle_list,
                    flags: 0,
                    circles: [
                      %{x: 1, y: 2, radius: 3, color: 0xF800},
                      %{x: -5, y: -6, radius: 7, color: 0x07E0}
                    ]
                  }
                ]}
    end

    test "decodes draw ellipse list commands" do
      frame = [
        BinaryBatch.draw_ellipse_list([
          {1, 2, 3, 4, 0xF800},
          {-5, -6, 7, 8, 0x07E0}
        ])
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 0xFF,
                    op: :draw_ellipse_list,
                    kind: 0,
                    flags: 0,
                    ellipses: [
                      %{x: 1, y: 2, radius_x: 3, radius_y: 4, color: 0xF800},
                      %{x: -5, y: -6, radius_x: 7, radius_y: 8, color: 0x07E0}
                    ]
                  }
                ]}
    end

    test "decodes fill ellipse list commands" do
      frame = [
        BinaryBatch.fill_ellipse_list([
          {1, 2, 3, 4, 0xF800},
          {-5, -6, 7, 8, 0x07E0}
        ])
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 0xFF,
                    op: :fill_ellipse_list,
                    kind: 1,
                    flags: 0,
                    ellipses: [
                      %{x: 1, y: 2, radius_x: 3, radius_y: 4, color: 0xF800},
                      %{x: -5, y: -6, radius_x: 7, radius_y: 8, color: 0x07E0}
                    ]
                  }
                ]}
    end

    test "decodes draw line list commands" do
      frame = [
        BinaryBatch.draw_line_list([
          {1, 2, 3, 4, 0xF800},
          {-5, -6, 7, 8, 0x07E0}
        ])
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 0xF8,
                    op: :draw_line_list,
                    flags: 0,
                    lines: [
                      %{x0: 1, y0: 2, x1: 3, y1: 4, color: 0xF800},
                      %{x0: -5, y0: -6, x1: 7, y1: 8, color: 0x07E0}
                    ]
                  }
                ]}
    end

    test "decodes draw triangle list commands" do
      frame = [
        BinaryBatch.draw_triangle_list([
          {1, 2, 3, 4, 5, 6, 0xF800},
          {-5, -6, 7, 8, 9, 10, 0x07E0}
        ])
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 0xFE,
                    op: :draw_triangle_list,
                    flags: 0,
                    triangles: [
                      %{x0: 1, y0: 2, x1: 3, y1: 4, x2: 5, y2: 6, color: 0xF800},
                      %{x0: -5, y0: -6, x1: 7, y1: 8, x2: 9, y2: 10, color: 0x07E0}
                    ]
                  }
                ]}
    end

    test "decodes fill triangle list commands" do
      frame = [
        BinaryBatch.fill_triangle_list([
          {1, 2, 3, 4, 5, 6, 0xF800},
          {-5, -6, 7, 8, 9, 10, 0x07E0}
        ])
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 0xFD,
                    op: :fill_triangle_list,
                    flags: 0,
                    triangles: [
                      %{x0: 1, y0: 2, x1: 3, y1: 4, x2: 5, y2: 6, color: 0xF800},
                      %{x0: -5, y0: -6, x1: 7, y1: 8, x2: 9, y2: 10, color: 0x07E0}
                    ]
                  }
                ]}
    end

    test "decodes arc and bezier commands" do
      frame = [
        BinaryBatch.draw_arc(160, 120, 60, 40, 45, 180.5, 0xFFFF),
        BinaryBatch.fill_arc(161, 121, 61, 41, -90, 90, 0x07E0),
        BinaryBatch.draw_bezier(0, 1, 2, 3, 4, 5, 0xFFFF),
        BinaryBatch.draw_bezier(0, 1, 2, 3, 4, 5, 6, 7, 0x07E0)
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 28,
                    op: :draw_arc,
                    x: 160,
                    y: 120,
                    radius0: 60,
                    radius1: 40,
                    angle0: 45.0,
                    angle1: 180.5,
                    color: 0xFFFF
                  },
                  %{
                    index: 1,
                    opcode: 29,
                    op: :fill_arc,
                    x: 161,
                    y: 121,
                    radius0: 61,
                    radius1: 41,
                    angle0: -90.0,
                    angle1: 90.0,
                    color: 0x07E0
                  },
                  %{
                    index: 2,
                    opcode: 30,
                    op: :draw_bezier,
                    point_count: 3,
                    x0: 0,
                    y0: 1,
                    x1: 2,
                    y1: 3,
                    x2: 4,
                    y2: 5,
                    color: 0xFFFF
                  },
                  %{
                    index: 3,
                    opcode: 30,
                    op: :draw_bezier,
                    point_count: 4,
                    x0: 0,
                    y0: 1,
                    x1: 2,
                    y1: 3,
                    x2: 4,
                    y2: 5,
                    x3: 6,
                    y3: 7,
                    color: 0x07E0
                  }
                ]}
    end

    test "decodes JPEG draw payloads" do
      jpeg = <<0xFF, 0xD8, 0xFF, 0xD9>>

      frame = [
        BinaryBatch.draw_jpg(-2, 3, jpeg),
        BinaryBatch.draw_jpg(4, 5, 32, 24, 1, 2, 1.5, 2, jpeg)
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 43,
                    op: :draw_jpg,
                    x: -2,
                    y: 3,
                    variant: :basic,
                    jpeg_len: 4
                  },
                  %{
                    index: 1,
                    opcode: 43,
                    op: :draw_jpg,
                    x: 4,
                    y: 5,
                    variant: :scaled,
                    max_width: 32,
                    max_height: 24,
                    off_x: 1,
                    off_y: 2,
                    scale_x: 1.5,
                    scale_y: 2.0,
                    jpeg_len: 4
                  }
                ]}
    end

    test "decodes RGB565 push image payloads" do
      pixels = <<0x00, 0xF8, 0xE0, 0x07, 0x1F, 0x00, 0xFF, 0xFF>>
      frame = BinaryBatch.push_image_rgb565(-2, 3, 2, 2, pixels)

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 44,
                    op: :push_image,
                    x: -2,
                    y: 3,
                    width: 2,
                    height: 2,
                    stride_pixels: 0,
                    pixels_len: 8
                  }
                ]}
    end

    test "decodes sprite state commands" do
      frame = [
        BinaryBatch.set_palette_color(3, 0x112233),
        BinaryBatch.set_pivot(-8, 16)
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 50,
                    op: :set_palette_color,
                    palette_index: 3,
                    rgb888: 0x112233
                  },
                  %{index: 1, opcode: 51, op: :set_pivot, x: -8, y: 16}
                ]}
    end

    test "decodes push rotate zoom payloads" do
      frame = [
        BinaryBatch.push_rotate_zoom(1, -2, 300, 45, 1.5, 2),
        BinaryBatch.push_rotate_zoom(1, -2, 300, 45, 1.5, 2, {:index, 3})
      ]

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 53,
                    op: :push_rotate_zoom,
                    flags: 0,
                    source_target: 1,
                    x: -2,
                    y: 300,
                    angle_deg: 45.0,
                    zoom_x: 1.5,
                    zoom_y: 2.0,
                    transparent: nil
                  },
                  %{
                    index: 1,
                    opcode: 53,
                    op: :push_rotate_zoom,
                    flags: 16,
                    source_target: 1,
                    x: -2,
                    y: 300,
                    angle_deg: 45.0,
                    zoom_x: 1.5,
                    zoom_y: 2.0,
                    transparent: {:index, 3}
                  }
                ]}
    end

    test "decodes native push rotate zoom frame strips" do
      frame =
        BinaryBatch.push_rotate_zoom_frame_strips(
          [{1, 10, -20, 9_000, 1_024, 2_048}],
          frame_height: 320,
          background: 0x1234,
          transparent: {:index, 0},
          approx_cull: true
        )

      assert BinaryBatch.decode(frame) ==
               {:ok,
                [
                  %{
                    index: 0,
                    opcode: 0xFF,
                    subop: 0x01,
                    op: :push_rotate_zoom_frame_strips,
                    flags: 16,
                    options: 3,
                    transparent: {:index, 0},
                    frame_height: 320,
                    background: 0x1234,
                    instances: [
                      %{
                        source_target: 1,
                        x: 10,
                        y: -20,
                        angle_cdeg: 9_000,
                        zoom_x1024: 1_024,
                        zoom_y1024: 2_048
                      }
                    ]
                  }
                ]}
    end

    test "decodes binary-batch frame scripts into readable command maps" do
      frame = [
        BinaryBatch.target(1),
        BinaryBatch.set_palette_color(3, 0x112233),
        BinaryBatch.set_pivot(16, 16),
        BinaryBatch.color_mode(:palette_index),
        BinaryBatch.fill_rect(2, 3, 4, 5, 1),
        BinaryBatch.set_text_color({:index, 1}, {:index, 0}),
        BinaryBatch.draw_string(8, 16, "fps:60"),
        BinaryBatch.print(" cursor"),
        BinaryBatch.println(" line"),
        BinaryBatch.begin_strip(160),
        BinaryBatch.push_sprite(2, -1, 32, {:index, 0}),
        BinaryBatch.push_rotate_zoom(2, 8, 9, 45, 1.5, 2, {:index, 0}),
        BinaryBatch.present_strip(),
        BinaryBatch.draw_jpg(1, 2, <<0xFF, 0xD8, 0xFF, 0xD9>>),
        BinaryBatch.push_image_rgb565(0, 0, 1, 1, <<0xFF, 0xFF>>),
        BinaryBatch.push_sprite_list([{4, 1, 2}, {5, 3, 4}], transparent: {:index, 0}),
        BinaryBatch.push_sprite_region_list([{4, 0, 0, 32, 16, 8, 9}], transparent: 0x0000),
        BinaryBatch.push_rotate_zoom_list(
          [{3, 10, -20, 9_000, 1_024, 2_048}],
          transparent: {:index, 0},
          y_offset: 32,
          approx_cull: true
        ),
        BinaryBatch.display()
      ]

      assert {:ok, decoded} = BinaryBatch.decode(frame)

      assert decoded == [
               %{index: 0, opcode: 0xF0, op: :target, target: 1},
               %{
                 index: 1,
                 opcode: 50,
                 op: :set_palette_color,
                 palette_index: 3,
                 rgb888: 0x112233
               },
               %{index: 2, opcode: 51, op: :set_pivot, x: 16, y: 16},
               %{index: 3, opcode: 0xF1, op: :color_mode, color_mode: :palette_index},
               %{
                 index: 4,
                 opcode: 21,
                 op: :fill_rect,
                 x: 2,
                 y: 3,
                 width: 4,
                 height: 5,
                 color: 1
               },
               %{
                 index: 5,
                 opcode: 37,
                 op: :set_text_color,
                 flags: 13,
                 fg: {:index, 1},
                 bg: {:index, 0}
               },
               %{
                 index: 6,
                 opcode: 40,
                 op: :draw_string,
                 x: 8,
                 y: 16,
                 text: "fps:60",
                 text_len: 6
               },
               %{index: 7, opcode: 41, op: :print, text: " cursor", text_len: 7},
               %{index: 8, opcode: 42, op: :println, text: " line", text_len: 5},
               %{index: 9, opcode: 0xF5, op: :begin_strip, y0: 160},
               %{
                 index: 10,
                 opcode: 0xF2,
                 op: :push_sprite,
                 source_target: 2,
                 x: -1,
                 y: 32,
                 transparent: {:index, 0},
                 flags: 16
               },
               %{
                 index: 11,
                 opcode: 53,
                 op: :push_rotate_zoom,
                 flags: 16,
                 source_target: 2,
                 x: 8,
                 y: 9,
                 angle_deg: 45.0,
                 zoom_x: 1.5,
                 zoom_y: 2.0,
                 transparent: {:index, 0}
               },
               %{index: 12, opcode: 0xF6, op: :present_strip},
               %{
                 index: 13,
                 opcode: 43,
                 op: :draw_jpg,
                 x: 1,
                 y: 2,
                 variant: :basic,
                 jpeg_len: 4
               },
               %{
                 index: 14,
                 opcode: 44,
                 op: :push_image,
                 x: 0,
                 y: 0,
                 width: 1,
                 height: 1,
                 stride_pixels: 0,
                 pixels_len: 2
               },
               %{
                 index: 15,
                 opcode: 0xF3,
                 op: :push_sprite_list,
                 flags: 17,
                 transparent: {:index, 0},
                 instances: [
                   %{source_target: 4, x: 1, y: 2},
                   %{source_target: 5, x: 3, y: 4}
                 ]
               },
               %{
                 index: 16,
                 opcode: 0xF4,
                 op: :push_sprite_region_list,
                 flags: 1,
                 transparent: {:rgb565, 0},
                 instances: [
                   %{
                     source_target: 4,
                     src_x: 0,
                     src_y: 0,
                     src_w: 32,
                     src_h: 16,
                     dst_x: 8,
                     dst_y: 9
                   }
                 ]
               },
               %{
                 index: 17,
                 opcode: 58,
                 op: :push_rotate_zoom_list,
                 flags: 16,
                 options: 3,
                 transparent: {:index, 0},
                 y_offset: 32,
                 approx_cull: true,
                 instances: [
                   %{
                     source_target: 3,
                     x: 10,
                     y: -20,
                     angle_cdeg: 9_000,
                     zoom_x1024: 1_024,
                     zoom_y1024: 2_048
                   }
                 ]
               },
               %{index: 18, opcode: 13, op: :display}
             ]
    end

    test "reports failed command index and opcode for malformed streams" do
      assert BinaryBatch.decode(<<0xF0>>) == {:error, {:batch_failed, 0, 0xF0, :truncated}}

      assert BinaryBatch.decode(<<0xF0, 1, 0xEF>>) ==
               {:error, {:batch_failed, 1, 0xEF, :unsupported_command}}
    end

    test "rejects malformed native presentation strip lifecycle" do
      assert BinaryBatch.decode([BinaryBatch.begin_strip(0), BinaryBatch.begin_strip(16)]) ==
               {:error, {:batch_failed, 1, 0xF5, :strip_already_active}}

      assert BinaryBatch.decode([BinaryBatch.present_strip()]) ==
               {:error, {:batch_failed, 0, 0xF6, :strip_not_active}}

      assert BinaryBatch.decode([BinaryBatch.begin_strip(0), BinaryBatch.clear(0)]) ==
               {:error, {:batch_failed, :end_of_batch, 0, :strip_not_presented}}

      assert BinaryBatch.decode([BinaryBatch.begin_strip(0), BinaryBatch.display()]) ==
               {:error, {:batch_failed, 1, 13, :strip_not_presented}}
    end

    test "rejects malformed scalar and control wire fields" do
      assert BinaryBatch.decode(<<0xF0, 0xFF>>) ==
               {:error, {:batch_failed, 0, 0xF0, {:bad_target, 0xFF}}}

      assert BinaryBatch.decode(
               <<17, 0::little-signed-16, 0::little-signed-16, 0::little-16, 0::little-16>>
             ) ==
               {:error, {:batch_failed, 0, 17, {:bad_zero_value, :height}}}

      assert BinaryBatch.decode(
               <<21, 0::little-signed-16, 0::little-signed-16, 0::little-16, 1::little-16,
                 0::little-16>>
             ) ==
               {:error, {:batch_failed, 0, 21, {:bad_zero_value, :width}}}

      assert BinaryBatch.decode(
               <<22, 0::little-signed-16, 0::little-signed-16, 1::little-16, 1::little-16,
                 0::little-16, 0::little-16>>
             ) ==
               {:error, {:batch_failed, 0, 22, {:bad_zero_value, :radius}}}

      assert BinaryBatch.decode(
               <<28, 0::little-signed-16, 0::little-signed-16, 0::little-16, 1::little-16,
                 0.0::little-float-32, 90.0::little-float-32, 0::little-16>>
             ) ==
               {:error, {:batch_failed, 0, 28, {:bad_zero_value, :radius0}}}

      assert BinaryBatch.decode(<<30, 5, 0>>) ==
               {:error, {:batch_failed, 0, 30, {:bad_bezier_point_count, 5}}}

      assert BinaryBatch.decode(<<30, 3, 1>>) ==
               {:error, {:batch_failed, 0, 30, {:bad_reserved, 1}}}

      assert BinaryBatch.decode(<<0xFA, 1::little-16, 1::little-16, 0::size(80)>>) ==
               {:error, {:batch_failed, 0, 0xFA, {:bad_flags, 1}}}

      assert BinaryBatch.decode(<<0xFA, 0::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFA, :empty_batch}}

      assert BinaryBatch.decode(
               <<0xFA, 0::little-16, 1::little-16, 0::little-signed-16, 0::little-signed-16,
                 0::little-16, 1::little-16, 0::little-16>>
             ) ==
               {:error, {:batch_failed, 0, 0xFA, {:bad_zero_value, :width}}}

      assert BinaryBatch.decode(
               <<0xFA, 0::little-16, 1::little-16, 0::little-signed-16, 0::little-signed-16,
                 1::little-16, 0::little-16, 0::little-16>>
             ) ==
               {:error, {:batch_failed, 0, 0xFA, {:bad_zero_value, :height}}}

      assert BinaryBatch.decode(<<35, 2, 0>>) ==
               {:error, {:batch_failed, 0, 35, :bad_text_wrap}}

      assert BinaryBatch.decode(<<0xF9, 1::little-16, 1::little-16>>) ==
               {:error, {:batch_failed, 0, 0xF9, {:bad_flags, 1}}}

      assert BinaryBatch.decode(<<0xF9, 0::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xF9, :empty_batch}}

      assert BinaryBatch.decode(
               <<0xF9, 0::little-16, 1::little-16, 0::little-signed-16, 0::little-signed-16>>
             ) == {:error, {:batch_failed, 0, 0xF9, :truncated}}

      assert BinaryBatch.decode(<<0xF7, 1::little-16, 1::little-16>>) ==
               {:error, {:batch_failed, 0, 0xF7, {:bad_flags, 1}}}

      assert BinaryBatch.decode(<<0xF7, 0::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xF7, :empty_batch}}

      assert BinaryBatch.decode(
               <<0xF7, 0::little-16, 1::little-16, 0::little-signed-16, 0::little-signed-16,
                 0::little-16, 1::little-16, 0::little-16>>
             ) == {:error, {:batch_failed, 0, 0xF7, {:bad_zero_value, :width}}}

      assert BinaryBatch.decode(<<0xFC, 1::little-16, 1::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFC, {:bad_flags, 1}}}

      assert BinaryBatch.decode(<<0xFC, 0::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFC, :empty_batch}}

      assert BinaryBatch.decode(
               <<0xFC, 0::little-16, 1::little-16, 0::little-signed-16, 0::little-signed-16,
                 0::little-16, 0::little-16>>
             ) == {:error, {:batch_failed, 0, 0xFC, {:bad_zero_value, :radius}}}

      assert BinaryBatch.decode(<<0xFB, 1::little-16, 1::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFB, {:bad_flags, 1}}}

      assert BinaryBatch.decode(<<0xFB, 0::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFB, :empty_batch}}

      assert BinaryBatch.decode(
               <<0xFB, 0::little-16, 1::little-16, 0::little-signed-16, 0::little-signed-16,
                 0::little-16, 0::little-16>>
             ) == {:error, {:batch_failed, 0, 0xFB, {:bad_zero_value, :radius}}}

      assert BinaryBatch.decode(<<0xFF, 0, 2, 0, 0::little-16, 1::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFF, {:bad_ellipse_list_kind, 2}}}

      assert BinaryBatch.decode(<<0xFF, 0, 0, 1, 0::little-16, 1::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFF, {:bad_reserved, 1}}}

      assert BinaryBatch.decode(<<0xFF, 0, 0, 0, 1::little-16, 1::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFF, {:bad_flags, 1}}}

      assert BinaryBatch.decode(<<0xFF, 0, 0, 0, 0::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFF, :empty_batch}}

      assert BinaryBatch.decode(
               <<0xFF, 0, 0, 0, 0::little-16, 1::little-16, 0::little-signed-16,
                 0::little-signed-16, 0::little-16, 1::little-16, 0::little-16>>
             ) == {:error, {:batch_failed, 0, 0xFF, {:bad_zero_value, :radius_x}}}

      assert BinaryBatch.decode(
               <<0xFF, 0, 1, 0, 0::little-16, 1::little-16, 0::little-signed-16,
                 0::little-signed-16, 1::little-16, 0::little-16, 0::little-16>>
             ) == {:error, {:batch_failed, 0, 0xFF, {:bad_zero_value, :radius_y}}}

      assert BinaryBatch.decode(
               <<0xFF, 0, 0, 0, 0::little-16, 1::little-16, 0::little-signed-16,
                 0::little-signed-16>>
             ) == {:error, {:batch_failed, 0, 0xFF, :truncated}}

      assert BinaryBatch.decode(<<0xF8, 1::little-16, 1::little-16>>) ==
               {:error, {:batch_failed, 0, 0xF8, {:bad_flags, 1}}}

      assert BinaryBatch.decode(<<0xF8, 0::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xF8, :empty_batch}}

      assert BinaryBatch.decode(<<0xFE, 1::little-16, 1::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFE, {:bad_flags, 1}}}

      assert BinaryBatch.decode(<<0xFE, 0::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFE, :empty_batch}}

      assert BinaryBatch.decode(
               <<0xFE, 0::little-16, 1::little-16, 0::little-signed-16, 0::little-signed-16>>
             ) == {:error, {:batch_failed, 0, 0xFE, :truncated}}

      assert BinaryBatch.decode(<<0xFD, 1::little-16, 1::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFD, {:bad_flags, 1}}}

      assert BinaryBatch.decode(<<0xFD, 0::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xFD, :empty_batch}}

      assert BinaryBatch.decode(
               <<0xFD, 0::little-16, 1::little-16, 0::little-signed-16, 0::little-signed-16,
                 1::little-signed-16>>
             ) == {:error, {:batch_failed, 0, 0xFD, :truncated}}

      assert BinaryBatch.decode(<<41, 3::little-16, "a", 0, "b">>) ==
               {:error, {:batch_failed, 0, 41, :text_contains_nul}}

      assert BinaryBatch.decode(<<42, 2::little-16, "a">>) ==
               {:error, {:batch_failed, 0, 42, :truncated}}

      assert BinaryBatch.decode(<<43, 0, 1>>) ==
               {:error, {:batch_failed, 0, 43, {:bad_reserved, 1}}}

      assert BinaryBatch.decode(<<43, 2, 0>>) ==
               {:error, {:batch_failed, 0, 43, {:bad_jpeg_variant, 2}}}

      assert BinaryBatch.decode(
               <<43, 0, 0, 0::little-signed-16, 0::little-signed-16, 0::little-32>>
             ) ==
               {:error, {:batch_failed, 0, 43, :empty_jpeg}}

      assert BinaryBatch.decode(
               <<43, 1, 0, 0::little-signed-16, 0::little-signed-16, 0::little-16, 0::little-16,
                 0::little-signed-16, 0::little-signed-16, 0.0::little-float-32,
                 1.0::little-float-32, 1::little-32, "x">>
             ) == {:error, {:batch_failed, 0, 43, {:bad_image_scale, 0.0}}}

      assert BinaryBatch.decode(<<33, 0::little-16, 1024::little-16>>) ==
               {:error, {:batch_failed, 0, 33, {:bad_zero_value, :scale_x1024}}}

      assert BinaryBatch.decode(
               <<0xF2, 0::little-16, 0, 0::little-signed-16, 0::little-signed-16, 0::little-16>>
             ) ==
               {:error, {:batch_failed, 0, 0xF2, {:bad_sprite_target, 0}}}
    end

    test "rejects malformed sprite state wire fields" do
      assert BinaryBatch.decode(<<50, 1, 7, 0x112233::little-32>>) ==
               {:error, {:batch_failed, 0, 50, {:bad_reserved, 7}}}

      assert BinaryBatch.decode(<<50, 1, 0, 0x01000000::little-32>>) ==
               {:error, {:batch_failed, 0, 50, {:bad_palette_color, 0x01000000}}}
    end

    test "rejects malformed push rotate zoom wire fields" do
      assert BinaryBatch.decode(<<53, 0, 1>>) ==
               {:error, {:batch_failed, 0, 53, {:bad_reserved, 1}}}

      assert BinaryBatch.decode(
               <<53, 0, 0, 0::little-16, 1, 7, 0::little-signed-16, 0::little-signed-16,
                 0.0::little-float-32, 1.0::little-float-32, 1.0::little-float-32, 0::little-16>>
             ) == {:error, {:batch_failed, 0, 53, {:bad_reserved, 7}}}

      assert BinaryBatch.decode(
               <<53, 0, 0, 16::little-16, 1, 0, 0::little-signed-16, 0::little-signed-16,
                 0.0::little-float-32, 1.0::little-float-32, 1.0::little-float-32, 0::little-16>>
             ) == {:error, {:batch_failed, 0, 53, {:bad_flags, 16}}}

      assert BinaryBatch.decode(
               <<53, 2, 0, 0::little-16, 1, 0, 0::little-signed-16, 0::little-signed-16,
                 0.0::little-float-32, 1.0::little-float-32, 1.0::little-float-32, 0::little-16>>
             ) == {:error, {:batch_failed, 0, 53, {:bad_options, 2}}}

      assert BinaryBatch.decode(
               <<53, 0, 0, 0::little-16, 1, 0, 0::little-signed-16, 0::little-signed-16,
                 0.0::little-float-32, 0.0::little-float-32, 1.0::little-float-32, 0::little-16>>
             ) == {:error, {:batch_failed, 0, 53, {:bad_image_scale, 0.0}}}
    end

    test "rejects malformed image payload fields" do
      assert BinaryBatch.decode(
               <<44, 0::little-signed-16, 0::little-signed-16, 0::little-16, 1::little-16,
                 0::little-16, 0::little-32>>
             ) ==
               {:error, {:batch_failed, 0, 44, {:bad_image_dimensions, 0, 1}}}

      assert BinaryBatch.decode(
               <<44, 0::little-signed-16, 0::little-signed-16, 2::little-16, 1::little-16,
                 1::little-16, 2::little-32, 0xFF, 0xFF>>
             ) ==
               {:error, {:batch_failed, 0, 44, {:bad_stride, 1, 2}}}

      assert BinaryBatch.decode(
               <<44, 0::little-signed-16, 0::little-signed-16, 1::little-16, 1::little-16,
                 0::little-16, 1::little-32, 0xFF>>
             ) ==
               {:error, {:batch_failed, 0, 44, {:pixels_size_not_even, 1}}}

      assert BinaryBatch.decode(
               <<44, 0::little-signed-16, 0::little-signed-16, 2::little-16, 2::little-16,
                 0::little-16, 2::little-32, 0xFF, 0xFF>>
             ) ==
               {:error, {:batch_failed, 0, 44, {:pixels_size_too_small, 8, 2}}}
    end

    test "rejects malformed sprite list wire fields" do
      assert BinaryBatch.decode(<<0xF3, 0::little-16, 1::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xF3, {:bad_transparent_color, 1}}}

      assert BinaryBatch.decode(
               <<0xF3, 0::little-16, 0::little-16, 1::little-16, 1, 7, 0::little-signed-16,
                 0::little-signed-16>>
             ) ==
               {:error, {:batch_failed, 0, 0xF3, {:bad_reserved, 7}}}

      assert BinaryBatch.decode(<<0xF4, 0::little-16, 1::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xF4, {:bad_transparent_color, 1}}}

      assert BinaryBatch.decode(<<0xF3, 0::little-16, 0::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xF3, :empty_batch}}

      assert BinaryBatch.decode(<<0xF4, 0::little-16, 0::little-16, 0::little-16>>) ==
               {:error, {:batch_failed, 0, 0xF4, :empty_batch}}
    end

    test "rejects malformed transform list wire fields" do
      assert BinaryBatch.decode(
               <<58, 16::little-16, ?P, ?R, ?Z, ?L, 1, 0, 0::little-16, 0::little-signed-16,
                 0::little-16>>
             ) ==
               {:error, {:batch_failed, 0, 58, {:bad_flags, 16}}}

      assert BinaryBatch.decode(
               <<58, 0::little-16, ?P, ?R, ?Z, ?L, 1, 0, 1::little-16, 0::little-signed-16,
                 0::little-16>>
             ) ==
               {:error, {:batch_failed, 0, 58, {:bad_transparent_color, 1}}}

      assert BinaryBatch.decode(
               <<58, 0::little-16, ?P, ?R, ?Z, ?L, 1, 4, 0::little-16, 0::little-signed-16,
                 0::little-16>>
             ) ==
               {:error, {:batch_failed, 0, 58, {:bad_options, 4}}}

      assert BinaryBatch.decode(
               <<58, 0::little-16, ?P, ?R, ?Z, ?L, 1, 0, 0::little-16, 0::little-signed-16,
                 0::little-16>>
             ) ==
               {:error, {:batch_failed, 0, 58, :empty_batch}}
    end
  end

  describe "validate/1" do
    test "returns ok for a structurally valid command stream" do
      frame = [
        BinaryBatch.target(1),
        BinaryBatch.clear(0x0000),
        BinaryBatch.display()
      ]

      assert BinaryBatch.validate(frame) == :ok
      assert BinaryBatch.validate!(frame) == :ok
    end

    test "returns decode errors without producing decoded command maps" do
      assert BinaryBatch.validate(<<0xF0>>) == {:error, {:batch_failed, 0, 0xF0, :truncated}}

      assert_raise ArgumentError,
                   "render batch command 0 opcode 240 failed: :truncated",
                   fn ->
                     BinaryBatch.validate!(<<0xF0>>)
                   end
    end

    test "checks strip lifecycle before native rendering" do
      frame = [
        BinaryBatch.begin_strip(160),
        BinaryBatch.display()
      ]

      assert BinaryBatch.validate(frame) == {:error, {:batch_failed, 1, 13, :strip_not_presented}}

      assert BinaryBatch.render_checked(:not_a_port, frame) ==
               {:error, {:batch_failed, 1, 13, :strip_not_presented}}
    end

    test "rejects native frame commands inside an active strip" do
      frame = [
        BinaryBatch.begin_strip(160),
        BinaryBatch.push_rotate_zoom_frame_strips([{1, 0, 0, 0, 1_024, 1_024}],
          frame_height: 320
        )
      ]

      assert BinaryBatch.validate(frame) == {:error, {:batch_failed, 1, 0xFF, :strip_already_active}}
    end

    test "rejects empty command streams before native rendering" do
      assert BinaryBatch.validate(<<>>) == {:error, :empty_batch}
      assert BinaryBatch.render_checked(:not_a_port, <<>>) == {:error, :empty_batch}
    end
  end

  describe "summary/1" do
    test "returns compact diagnostic counts for binary-batch frame scripts" do
      frame = [
        BinaryBatch.target(1),
        BinaryBatch.set_palette_color(3, 0x112233),
        BinaryBatch.set_pivot(16, 16),
        BinaryBatch.color_mode(:palette_index),
        BinaryBatch.clear(0),
        BinaryBatch.draw_pixel_list([{0, 0, 1}, {1, 1, 2}, {2, 2, 3}]),
        BinaryBatch.draw_rect_list([{0, 0, 10, 4, 1}, {12, 0, 8, 4, 2}]),
        BinaryBatch.fill_rect_list([{0, 0, 10, 4, 1}, {12, 0, 8, 4, 2}]),
        BinaryBatch.draw_circle_list([{4, 4, 2, 1}, {12, 12, 3, 2}]),
        BinaryBatch.fill_circle_list([{4, 4, 2, 1}, {12, 12, 3, 2}]),
        BinaryBatch.draw_ellipse_list([{4, 4, 2, 3, 1}, {12, 12, 3, 4, 2}]),
        BinaryBatch.fill_ellipse_list([{4, 4, 2, 3, 1}, {12, 12, 3, 4, 2}]),
        BinaryBatch.draw_line_list([{0, 0, 10, 10, 1}, {12, 0, 20, 8, 2}]),
        BinaryBatch.draw_triangle_list([{0, 0, 6, 0, 3, 5, 1}, {8, 0, 14, 0, 11, 5, 2}]),
        BinaryBatch.fill_triangle_list([{0, 0, 6, 0, 3, 5, 1}, {8, 0, 14, 0, 11, 5, 2}]),
        BinaryBatch.set_clip_rect(0, 0, 120, 80),
        BinaryBatch.fill_circle(24, 24, 6, 1),
        BinaryBatch.clear_clip_rect(),
        BinaryBatch.set_text_color({:index, 1}, {:index, 0}),
        BinaryBatch.draw_string(0, 0, "ok"),
        BinaryBatch.print(" cursor"),
        BinaryBatch.println(" line"),
        BinaryBatch.begin_strip(160),
        BinaryBatch.push_sprite(2, 0, 0, {:index, 0}),
        BinaryBatch.push_rotate_zoom(2, 8, 9, 45, 1.5, 2, {:index, 0}),
        BinaryBatch.present_strip(),
        BinaryBatch.draw_jpg(1, 2, <<0xFF, 0xD8, 0xFF, 0xD9>>),
        BinaryBatch.push_image_rgb565(0, 0, 1, 1, <<0xFF, 0xFF>>),
        BinaryBatch.push_sprite_list([{4, 1, 2}, {5, 3, 4}], transparent: {:index, 0}),
        BinaryBatch.push_sprite_region_list([{4, 0, 0, 32, 16, 8, 9}], transparent: 0x0000),
        BinaryBatch.push_rotate_zoom_list(
          [
            {3, 10, -20, 9_000, 1_024, 2_048},
            {4, 20, -10, 18_000, 1_024, 1_024}
          ],
          transparent: {:index, 0},
          approx_cull: true
        ),
        BinaryBatch.display()
      ]

      assert {:ok, summary} = BinaryBatch.summary(frame)

      assert summary.batch_bytes == byte_size(BinaryBatch.batch(frame))
      assert summary.command_count == 32
      assert summary.scalar_count == 23
      assert summary.render_private_count == 17
      assert summary.dynamic_payload_bytes == 276
      assert summary.packed_list_record_bytes == 256
      assert summary.fixed_overhead_bytes == summary.batch_bytes - summary.dynamic_payload_bytes

      assert summary.bytes_per_command_x1000 ==
               div(summary.batch_bytes * 1000, summary.command_count)

      assert summary.bytes_per_logical_scalar_x1000 ==
               div(summary.batch_bytes * 1000, summary.scalar_count)

      assert summary.dynamic_payload_ratio_x1000 ==
               div(summary.dynamic_payload_bytes * 1000, summary.batch_bytes)

      assert summary.packed_list_record_ratio_x1000 ==
               div(summary.packed_list_record_bytes * 1000, summary.batch_bytes)

      assert summary.packed_list_instances_per_command_x1000 ==
               div(summary.packed_list_instance_count * 1000, summary.packed_list_count)

      assert summary.packed_list_count == 13
      assert summary.packed_list_instance_count == 26
      assert summary.draw_pixel_list_count == 1
      assert summary.draw_pixel_list_instance_count == 3
      assert summary.draw_rect_list_count == 1
      assert summary.draw_rect_list_instance_count == 2
      assert summary.fill_rect_list_count == 1
      assert summary.fill_rect_list_instance_count == 2
      assert summary.draw_circle_list_count == 1
      assert summary.draw_circle_list_instance_count == 2
      assert summary.fill_circle_list_count == 1
      assert summary.fill_circle_list_instance_count == 2
      assert summary.draw_ellipse_list_count == 1
      assert summary.draw_ellipse_list_instance_count == 2
      assert summary.fill_ellipse_list_count == 1
      assert summary.fill_ellipse_list_instance_count == 2
      assert summary.draw_line_list_count == 1
      assert summary.draw_line_list_instance_count == 2
      assert summary.draw_triangle_list_count == 1
      assert summary.draw_triangle_list_instance_count == 2
      assert summary.fill_triangle_list_count == 1
      assert summary.fill_triangle_list_instance_count == 2
      assert summary.clip_count == 2
      assert summary.text_count == 4
      assert summary.image_count == 2
      assert summary.sprite_state_count == 2
      assert summary.sprite_push_count == 5
      assert summary.push_rotate_zoom_count == 1
      assert summary.sprite_push_list_count == 1
      assert summary.sprite_push_list_instance_count == 2
      assert summary.sprite_region_list_count == 1
      assert summary.sprite_region_list_instance_count == 1
      assert summary.push_rotate_zoom_list_count == 1
      assert summary.push_rotate_zoom_instance_count == 2
      assert summary.strip_begin_count == 1
      assert summary.strip_present_count == 1
      assert summary.display_count == 1
      assert summary.target_count == 1

      assert summary.ops == %{
               target: 1,
               set_palette_color: 1,
               set_pivot: 1,
               color_mode: 1,
               clear: 1,
               draw_pixel_list: 1,
               draw_rect_list: 1,
               fill_rect_list: 1,
               draw_circle_list: 1,
               fill_circle_list: 1,
               draw_ellipse_list: 1,
               fill_ellipse_list: 1,
               draw_line_list: 1,
               draw_triangle_list: 1,
               fill_triangle_list: 1,
               set_clip_rect: 1,
               fill_circle: 1,
               clear_clip_rect: 1,
               set_text_color: 1,
               draw_string: 1,
               print: 1,
               println: 1,
               begin_strip: 1,
               push_sprite: 1,
               push_rotate_zoom: 1,
               present_strip: 1,
               draw_jpg: 1,
               push_image: 1,
               push_sprite_list: 1,
               push_sprite_region_list: 1,
               push_rotate_zoom_list: 1,
               display: 1
             }
    end

    test "counts native frame commands as packed sprite work" do
      frame =
        BinaryBatch.push_rotate_zoom_frame_strips(
          [{1, 10, -20, 9_000, 1_024, 2_048}],
          frame_height: 320,
          background: 0x1234,
          transparent: {:index, 0},
          approx_cull: true
        )

      assert {:ok, summary} = BinaryBatch.summary(frame)

      assert summary.batch_bytes == byte_size(frame)
      assert summary.command_count == 1
      assert summary.render_private_count == 1
      assert summary.packed_list_count == 1
      assert summary.packed_list_instance_count == 1
      assert summary.packed_list_record_bytes == 12
      assert summary.dynamic_payload_bytes == 12
      assert summary.sprite_push_count == 1
      assert summary.push_rotate_zoom_frame_count == 1
      assert summary.push_rotate_zoom_frame_instance_count == 1
      assert summary.ops == %{push_rotate_zoom_frame_strips: 1}
    end

    test "returns decode errors for malformed streams" do
      assert BinaryBatch.summary(<<0xF0>>) == {:error, {:batch_failed, 0, 0xF0, :truncated}}
    end
  end

  describe "compare/2" do
    test "compares baseline and candidate batch summaries" do
      rectangles =
        for x <- 0..9 do
          {x * 2, 0, 1, 1, 0xFFFF}
        end

      baseline = [
        BinaryBatch.target(1),
        Enum.map(rectangles, fn {x, y, width, height, color} ->
          BinaryBatch.fill_rect(x, y, width, height, color)
        end),
        BinaryBatch.display()
      ]

      candidate = [
        BinaryBatch.target(1),
        BinaryBatch.fill_rect_list(rectangles),
        BinaryBatch.display()
      ]

      assert {:ok, comparison} = BinaryBatch.compare(baseline, candidate)

      assert comparison.baseline.batch_bytes == byte_size(BinaryBatch.batch(baseline))
      assert comparison.candidate.batch_bytes == byte_size(BinaryBatch.batch(candidate))
      assert comparison.baseline.command_count == 12
      assert comparison.candidate.command_count == 3
      assert comparison.baseline.scalar_count == 10
      assert comparison.candidate.scalar_count == 10

      assert comparison.delta.batch_bytes ==
               comparison.candidate.batch_bytes - comparison.baseline.batch_bytes

      assert comparison.delta.batch_bytes_savings ==
               comparison.baseline.batch_bytes - comparison.candidate.batch_bytes

      assert comparison.delta.batch_bytes_savings > 0
      assert comparison.delta.command_count == -9
      assert comparison.delta.scalar_count == 0
      assert comparison.delta.packed_list_count == 1
      assert comparison.delta.packed_list_instance_count == 10

      assert comparison.delta.candidate_batch_bytes_ratio_x1000 ==
               div(comparison.candidate.batch_bytes * 1000, comparison.baseline.batch_bytes)

      assert comparison.delta.batch_bytes_savings_ratio_x1000 ==
               div(comparison.delta.batch_bytes_savings * 1000, comparison.baseline.batch_bytes)

      assert BinaryBatch.compare!(baseline, candidate).delta == comparison.delta
    end

    test "reports which side failed validation" do
      valid_frame = [BinaryBatch.clear(0), BinaryBatch.display()]

      assert BinaryBatch.compare(<<0xF0>>, valid_frame) ==
               {:error, {:baseline, {:batch_failed, 0, 0xF0, :truncated}}}

      assert BinaryBatch.compare(valid_frame, <<0xF0>>) ==
               {:error, {:candidate, {:batch_failed, 0, 0xF0, :truncated}}}

      assert_raise ArgumentError,
                   "candidate batch invalid: render batch command 0 opcode 240 failed: :truncated",
                   fn -> BinaryBatch.compare!(valid_frame, <<0xF0>>) end
    end
  end

  describe "check_budget/2" do
    test "returns an ok report when a frame stays within limits" do
      rectangles = [
        {0, 0, 2, 2, 0xFFFF},
        {4, 0, 2, 2, 0x07E0}
      ]

      frame = [
        BinaryBatch.target(1),
        BinaryBatch.fill_rect_list(rectangles),
        BinaryBatch.display()
      ]

      batch_bytes = byte_size(BinaryBatch.batch(frame))

      assert {:ok, report} =
               BinaryBatch.check_budget(frame,
                 max_batch_bytes: batch_bytes,
                 max_command_count: 3,
                 min_packed_list_count: 1,
                 min_packed_list_instance_count: 2
               )

      assert report.ok? == true
      assert report.violations == []

      assert report.limits == %{
               max_batch_bytes: batch_bytes,
               max_command_count: 3,
               min_packed_list_count: 1,
               min_packed_list_instance_count: 2
             }

      assert report.summary.batch_bytes == batch_bytes
      assert report.summary.command_count == 3
      assert report.summary.packed_list_count == 1
      assert report.summary.packed_list_instance_count == 2

      assert BinaryBatch.check_budget!(frame, max_batch_bytes: batch_bytes).ok? == true
    end

    test "returns a budget report when a frame exceeds limits" do
      frame = [
        BinaryBatch.target(1),
        BinaryBatch.clear(0),
        BinaryBatch.display()
      ]

      assert {:error, {:budget_exceeded, report}} =
               BinaryBatch.check_budget(frame,
                 max_batch_bytes: 1,
                 min_packed_list_count: 1
               )

      assert report.ok? == false
      assert report.summary.batch_bytes == 6

      assert report.violations == [
               %{
                 limit: :max_batch_bytes,
                 metric: :batch_bytes,
                 direction: :max,
                 actual: 6,
                 limit_value: 1,
                 over_by: 5
               },
               %{
                 limit: :min_packed_list_count,
                 metric: :packed_list_count,
                 direction: :min,
                 actual: 0,
                 limit_value: 1,
                 under_by: 1
               }
             ]

      assert_raise ArgumentError,
                   "binary batch exceeds budget: batch_bytes 6 > max_batch_bytes 1, packed_list_count 0 < min_packed_list_count 1",
                   fn ->
                     BinaryBatch.check_budget!(frame,
                       max_batch_bytes: 1,
                       min_packed_list_count: 1
                     )
                   end
    end

    test "reports unavailable ratio metrics as budget violations" do
      frame = [
        BinaryBatch.target(1),
        BinaryBatch.display()
      ]

      assert {:error, {:budget_exceeded, report}} =
               BinaryBatch.check_budget(frame, max_bytes_per_logical_scalar_x1000: 1000)

      assert report.violations == [
               %{
                 limit: :max_bytes_per_logical_scalar_x1000,
                 metric: :bytes_per_logical_scalar_x1000,
                 direction: :max,
                 actual: nil,
                 limit_value: 1000,
                 reason: :metric_unavailable
               }
             ]
    end

    test "returns validation and limit errors" do
      assert BinaryBatch.check_budget(<<0xF0>>, max_batch_bytes: 1) ==
               {:error, {:batch_failed, 0, 0xF0, :truncated}}

      assert BinaryBatch.check_budget([BinaryBatch.display()], max_batch_bytes: -1) ==
               {:error, {:invalid_budget_limit, :max_batch_bytes, -1}}

      assert BinaryBatch.check_budget([BinaryBatch.display()], max_unknown: 1) ==
               {:error, {:invalid_budget_limit, :max_unknown, 1}}

      assert BinaryBatch.check_budget([BinaryBatch.display()], "bad") ==
               {:error, {:invalid_budget_limits, "bad"}}

      assert_raise ArgumentError,
                   "render batch command 0 opcode 240 failed: :truncated",
                   fn -> BinaryBatch.check_budget!(<<0xF0>>, max_batch_bytes: 1) end
    end
  end

  describe "diagnose/1" do
    test "returns a valid diagnostic report with summary fields" do
      frame = [
        BinaryBatch.target(1),
        BinaryBatch.clear(0x0000),
        BinaryBatch.display()
      ]

      assert {:ok, diagnosis} = BinaryBatch.diagnose(frame)

      assert diagnosis.valid? == true
      assert diagnosis.message == "binary batch is valid"
      assert diagnosis.error == nil
      assert diagnosis.failed_index == nil
      assert diagnosis.failed_opcode == nil
      assert diagnosis.failed_op == nil
      assert diagnosis.decoded_command_count == 3
      assert diagnosis.command_count == 3
      assert diagnosis.batch_bytes == byte_size(BinaryBatch.batch(frame))
      assert diagnosis.fixed_overhead_bytes == diagnosis.batch_bytes
      assert diagnosis.dynamic_payload_ratio_x1000 == 0
      assert diagnosis.packed_list_record_bytes == 0
      assert diagnosis.packed_list_record_ratio_x1000 == 0
      assert diagnosis.last_decoded_command.op == :display
      assert BinaryBatch.diagnose!(frame).valid? == true
    end

    test "returns partial decode context for malformed command streams" do
      frame = [
        BinaryBatch.target(1),
        <<0xF0>>
      ]

      assert {:error, diagnosis} = BinaryBatch.diagnose(frame)

      assert diagnosis.valid? == false
      assert diagnosis.error == {:batch_failed, 1, 0xF0, :truncated}
      assert diagnosis.message == "render batch command 1 opcode 240 failed: :truncated"
      assert diagnosis.failed_index == 1
      assert diagnosis.failed_opcode == 0xF0
      assert diagnosis.failed_op == :target
      assert diagnosis.decoded_command_count == 1
      assert diagnosis.command_count == 1
      assert diagnosis.target_count == 1
      assert diagnosis.last_decoded_command == %{index: 0, op: :target, opcode: 0xF0, target: 1}

      assert_raise ArgumentError,
                   "render batch command 1 opcode 240 failed: :truncated",
                   fn -> BinaryBatch.diagnose!(frame) end
    end

    test "returns lifecycle diagnostics after successful structural decode" do
      frame = [
        BinaryBatch.begin_strip(160),
        BinaryBatch.display()
      ]

      assert {:error, diagnosis} = BinaryBatch.diagnose(frame)

      assert diagnosis.valid? == false
      assert diagnosis.error == {:batch_failed, 1, 13, :strip_not_presented}
      assert diagnosis.failed_index == 1
      assert diagnosis.failed_opcode == 13
      assert diagnosis.failed_op == :display
      assert diagnosis.decoded_command_count == 2
      assert diagnosis.command_count == 2
      assert diagnosis.strip_begin_count == 1
      assert diagnosis.display_count == 1
      assert diagnosis.last_decoded_command.op == :display
    end

    test "returns an empty-batch diagnostic without command location" do
      assert {:error, diagnosis} = BinaryBatch.diagnose(<<>>)

      assert diagnosis.valid? == false
      assert diagnosis.error == :empty_batch
      assert diagnosis.message == "batch must not be empty"
      assert diagnosis.failed_index == nil
      assert diagnosis.failed_opcode == nil
      assert diagnosis.failed_op == nil
      assert diagnosis.decoded_command_count == 0
      assert diagnosis.command_count == 0
      assert diagnosis.batch_bytes == 0
      assert diagnosis.last_decoded_command == nil
    end
  end

  describe "batch/1" do
    test "accepts prebuilt binary command streams" do
      command_binary = <<1, 2, 3>>

      assert BinaryBatch.batch(command_binary) == command_binary
    end

    test "accepts iodata fragments" do
      batch =
        BinaryBatch.batch([
          BinaryBatch.target(1),
          [BinaryBatch.clear(0x0000)],
          BinaryBatch.display()
        ])

      assert batch == <<0xF0, 1, 15, 0x00, 0x00, 13>>
    end
  end
end
