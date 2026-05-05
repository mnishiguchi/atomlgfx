# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.ProtocolFreezeTest do
  use ExUnit.Case, async: true

  import Bitwise

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.Generated
  alias AtomLGFX.OpSchema
  alias AtomLGFX.Protocol

  @frozen_ops [
    {:ping, 0},
    {:get_caps, 1},
    {:get_last_error, 2},
    {:width, 3},
    {:height, 4},
    {:init, 5},
    {:close, 6},
    {:start_write, 7},
    {:end_write, 8},
    {:set_rotation, 9},
    {:set_brightness, 10},
    {:set_color_depth, 11},
    {:set_swap_bytes, 12},
    {:display, 13},
    {:fill_screen, 14},
    {:clear, 15},
    {:draw_pixel, 16},
    {:draw_fast_vline, 17},
    {:draw_fast_hline, 18},
    {:draw_line, 19},
    {:draw_rect, 20},
    {:fill_rect, 21},
    {:draw_round_rect, 22},
    {:fill_round_rect, 23},
    {:draw_circle, 24},
    {:fill_circle, 25},
    {:draw_ellipse, 26},
    {:fill_ellipse, 27},
    {:draw_arc, 28},
    {:fill_arc, 29},
    {:draw_bezier, 30},
    {:draw_triangle, 31},
    {:fill_triangle, 32},
    {:set_text_size, 33},
    {:set_text_datum, 34},
    {:set_text_wrap, 35},
    {:set_text_font_preset, 36},
    {:set_text_color, 37},
    {:set_cursor, 38},
    {:get_cursor, 39},
    {:draw_string, 40},
    {:print, 41},
    {:println, 42},
    {:draw_jpg, 43},
    {:push_image, 44},
    {:set_clip_rect, 45},
    {:clear_clip_rect, 46},
    {:create_sprite, 47},
    {:delete_sprite, 48},
    {:create_palette, 49},
    {:set_palette_color, 50},
    {:set_pivot, 51},
    {:push_sprite, 52},
    {:push_rotate_zoom, 53},
    {:get_touch, 54},
    {:get_touch_raw, 55},
    {:set_touch_calibrate, 56},
    {:calibrate_touch, 57},
    {:push_rotate_zoom_list, 58},
    {:submit_binary_batch, 59},
    {:get_presentation_strip_height, 60}
  ]

  @hidden_ops [:start_write, :end_write, :draw_pixel]
  @raw_ops [:start_write, :end_write, :draw_pixel]

  @binary_batch_public_ops %{
    display: 13,
    fill_screen: 14,
    clear: 15,
    draw_pixel: 16,
    draw_fast_vline: 17,
    draw_fast_hline: 18,
    draw_line: 19,
    draw_rect: 20,
    fill_rect: 21,
    draw_round_rect: 22,
    fill_round_rect: 23,
    draw_circle: 24,
    fill_circle: 25,
    draw_ellipse: 26,
    fill_ellipse: 27,
    draw_arc: 28,
    fill_arc: 29,
    draw_bezier: 30,
    draw_triangle: 31,
    fill_triangle: 32,
    set_text_size: 33,
    set_text_datum: 34,
    set_text_wrap: 35,
    set_text_font_preset: 36,
    set_text_color: 37,
    set_cursor: 38,
    draw_string: 40,
    print: 41,
    println: 42,
    draw_jpg: 43,
    push_image: 44,
    set_clip_rect: 45,
    clear_clip_rect: 46,
    set_palette_color: 50,
    set_pivot: 51,
    push_sprite: 52,
    push_rotate_zoom: 53,
    push_rotate_zoom_list: 58
  }

  @binary_batch_render_private_ops %{
    target: 0xF0,
    color_mode: 0xF1,
    push_sprite_transparent: 0xF2,
    push_sprite_list: 0xF3,
    push_sprite_region_list: 0xF4,
    begin_strip: 0xF5,
    present_strip: 0xF6,
    fill_rect_list: 0xF7,
    draw_line_list: 0xF8,
    draw_pixel_list: 0xF9,
    draw_rect_list: 0xFA,
    fill_circle_list: 0xFB,
    draw_circle_list: 0xFC,
    fill_triangle_list: 0xFD,
    draw_triangle_list: 0xFE,
    extended: 0xFF
  }

  @binary_batch_command_sizes %{
    target: 2,
    color_mode: 2,
    begin_strip: 3,
    present_strip: 1,
    display: 1,
    fill_screen: 3,
    clear: 3,
    draw_pixel: 7,
    draw_fast_vline: 9,
    draw_fast_hline: 9,
    draw_line: 11,
    draw_rect: 11,
    fill_rect: 11,
    draw_round_rect: 13,
    fill_round_rect: 13,
    draw_circle: 9,
    fill_circle: 9,
    draw_ellipse: 11,
    fill_ellipse: 11,
    draw_arc: 19,
    fill_arc: 19,
    draw_bezier_quadratic: 17,
    draw_bezier_cubic: 21,
    draw_triangle: 15,
    fill_triangle: 15,
    draw_pixel_list_header: 5,
    draw_pixel_list_record: 6,
    draw_rect_list_header: 5,
    draw_rect_list_record: 10,
    fill_rect_list_header: 5,
    fill_rect_list_record: 10,
    draw_circle_list_header: 5,
    draw_circle_list_record: 8,
    fill_circle_list_header: 5,
    fill_circle_list_record: 8,
    draw_ellipse_list_header: 8,
    draw_ellipse_list_record: 10,
    fill_ellipse_list_header: 8,
    fill_ellipse_list_record: 10,
    draw_line_list_header: 5,
    draw_line_list_record: 10,
    draw_triangle_list_header: 5,
    draw_triangle_list_record: 14,
    fill_triangle_list_header: 5,
    fill_triangle_list_record: 14,
    set_clip_rect: 9,
    clear_clip_rect: 1,
    set_text_font_preset: 2,
    set_text_size: 5,
    set_text_datum: 2,
    set_text_wrap: 3,
    set_cursor: 5,
    set_text_color_fg: 5,
    set_text_color_fg_bg: 7,
    draw_string_one_byte: 8,
    print_one_byte: 4,
    println_empty: 3,
    println_one_byte: 4,
    draw_jpg_header: 11,
    draw_jpg_scaled_header: 27,
    push_image_rgb565_header: 15,
    set_palette_color: 7,
    set_pivot: 5,
    push_sprite: 6,
    push_sprite_transparent: 10,
    push_sprite_list_header: 7,
    push_sprite_list_record: 6,
    push_sprite_region_list_header: 7,
    push_sprite_region_list_record: 14,
    push_rotate_zoom: 25,
    push_rotate_zoom_list_header: 15,
    push_rotate_zoom_list_record: 12
  }

  @capabilities %{
    sprite: 1 <<< 0,
    pushimage: 1 <<< 1,
    last_error: 1 <<< 2,
    touch: 1 <<< 3,
    palette: 1 <<< 4,
    batch: 1 <<< 5
  }

  test "freezes the v2 operation names and opcode order" do
    actual_ops =
      Enum.map(Generated.ops(), fn {name, meta} ->
        {name, Keyword.fetch!(meta, :opcode)}
      end)

    assert actual_ops == @frozen_ops
  end

  test "keeps canonical public names in snake_case" do
    for {name, _opcode} <- @frozen_ops do
      name_string = Atom.to_string(name)

      assert name_string == Macro.underscore(name_string)
      assert String.downcase(name_string) == name_string
    end
  end

  test "accepts only canonical operation names" do
    for {name, _opcode} <- @frozen_ops do
      assert OpSchema.canonical_name(name) == {:ok, name}
      assert OpSchema.elixir_name(name) == {:ok, name}
    end

    assert OpSchema.canonical_name(:fillRect) == :error
    assert OpSchema.elixir_name(:fillRect) == {:error, {:unknown_lgfx_op, :fillRect}}
  end

  test "freezes hidden and raw exposure policy" do
    hidden_ops =
      Generated.ops()
      |> Enum.reject(fn {_name, meta} -> Keyword.fetch!(meta, :public) end)
      |> Enum.map(fn {name, _meta} -> name end)

    raw_ops =
      Generated.ops()
      |> Enum.filter(fn {_name, meta} -> Keyword.fetch!(meta, :raw) end)
      |> Enum.map(fn {name, _meta} -> name end)

    assert hidden_ops == @hidden_ops
    assert raw_ops == @raw_ops
  end

  test "freezes the binary-batch public protocol opcode subset" do
    actual_opcodes =
      @binary_batch_public_ops
      |> Map.keys()
      |> Enum.map(fn name -> {name, OpSchema.opcode!(name)} end)
      |> Map.new()

    assert actual_opcodes == @binary_batch_public_ops
  end

  test "freezes the binary-batch render-private opcodes" do
    assert Map.new(BinaryBatch.__render_private_opcodes__()) == @binary_batch_render_private_ops
  end

  test "keeps binary-batch render-private opcodes aligned with protocol.h" do
    assert native_render_private_opcodes() == @binary_batch_render_private_ops
  end

  test "keeps binary-batch render-private opcodes contiguous" do
    actual_opcodes =
      BinaryBatch.__render_private_opcodes__()
      |> Keyword.values()
      |> Enum.sort()

    assert actual_opcodes == Enum.to_list(0xF0..0xFF)
  end

  test "keeps known binary-batch opcodes aligned with frozen opcode sets" do
    expected_opcodes =
      Enum.sort(
        Map.values(@binary_batch_render_private_ops) ++ Map.values(@binary_batch_public_ops)
      )

    actual_opcodes =
      BinaryBatch.__known_batch_opcodes__()
      |> Enum.sort()

    assert actual_opcodes == expected_opcodes
    assert actual_opcodes == Enum.uniq(actual_opcodes)
  end

  test "freezes fixed binary-batch command and record sizes" do
    actual_sizes = %{
      target: byte_size(BinaryBatch.target(0)),
      color_mode: byte_size(BinaryBatch.color_mode(:rgb565)),
      begin_strip: byte_size(BinaryBatch.begin_strip(0)),
      present_strip: byte_size(BinaryBatch.present_strip()),
      display: byte_size(BinaryBatch.display()),
      fill_screen: byte_size(BinaryBatch.fill_screen(0x0000)),
      clear: byte_size(BinaryBatch.clear(0x0000)),
      draw_pixel: byte_size(BinaryBatch.draw_pixel(0, 0, 0x0000)),
      draw_fast_vline: byte_size(BinaryBatch.draw_fast_vline(0, 0, 1, 0x0000)),
      draw_fast_hline: byte_size(BinaryBatch.draw_fast_hline(0, 0, 1, 0x0000)),
      draw_line: byte_size(BinaryBatch.draw_line(0, 0, 1, 1, 0x0000)),
      draw_rect: byte_size(BinaryBatch.draw_rect(0, 0, 1, 1, 0x0000)),
      fill_rect: byte_size(BinaryBatch.fill_rect(0, 0, 1, 1, 0x0000)),
      draw_round_rect: byte_size(BinaryBatch.draw_round_rect(0, 0, 1, 1, 1, 0x0000)),
      fill_round_rect: byte_size(BinaryBatch.fill_round_rect(0, 0, 1, 1, 1, 0x0000)),
      draw_circle: byte_size(BinaryBatch.draw_circle(0, 0, 1, 0x0000)),
      fill_circle: byte_size(BinaryBatch.fill_circle(0, 0, 1, 0x0000)),
      draw_ellipse: byte_size(BinaryBatch.draw_ellipse(0, 0, 1, 1, 0x0000)),
      fill_ellipse: byte_size(BinaryBatch.fill_ellipse(0, 0, 1, 1, 0x0000)),
      draw_arc: byte_size(BinaryBatch.draw_arc(0, 0, 1, 1, 0, 1, 0x0000)),
      fill_arc: byte_size(BinaryBatch.fill_arc(0, 0, 1, 1, 0, 1, 0x0000)),
      draw_bezier_quadratic: byte_size(BinaryBatch.draw_bezier(0, 0, 1, 1, 2, 2, 0x0000)),
      draw_bezier_cubic: byte_size(BinaryBatch.draw_bezier(0, 0, 1, 1, 2, 2, 3, 3, 0x0000)),
      draw_triangle: byte_size(BinaryBatch.draw_triangle(0, 0, 1, 0, 0, 1, 0x0000)),
      fill_triangle: byte_size(BinaryBatch.fill_triangle(0, 0, 1, 0, 0, 1, 0x0000)),
      draw_pixel_list_header:
        list_header_size(&BinaryBatch.draw_pixel_list/1, {0, 0, 0x0000}, {1, 1, 0xFFFF}),
      draw_pixel_list_record:
        list_record_size(&BinaryBatch.draw_pixel_list/1, {0, 0, 0x0000}, {1, 1, 0xFFFF}),
      draw_rect_list_header:
        list_header_size(
          &BinaryBatch.draw_rect_list/1,
          {0, 0, 1, 1, 0x0000},
          {1, 1, 2, 2, 0xFFFF}
        ),
      draw_rect_list_record:
        list_record_size(
          &BinaryBatch.draw_rect_list/1,
          {0, 0, 1, 1, 0x0000},
          {1, 1, 2, 2, 0xFFFF}
        ),
      fill_rect_list_header:
        list_header_size(
          &BinaryBatch.fill_rect_list/1,
          {0, 0, 1, 1, 0x0000},
          {1, 1, 2, 2, 0xFFFF}
        ),
      fill_rect_list_record:
        list_record_size(
          &BinaryBatch.fill_rect_list/1,
          {0, 0, 1, 1, 0x0000},
          {1, 1, 2, 2, 0xFFFF}
        ),
      draw_circle_list_header:
        list_header_size(&BinaryBatch.draw_circle_list/1, {0, 0, 1, 0x0000}, {1, 1, 2, 0xFFFF}),
      draw_circle_list_record:
        list_record_size(&BinaryBatch.draw_circle_list/1, {0, 0, 1, 0x0000}, {1, 1, 2, 0xFFFF}),
      fill_circle_list_header:
        list_header_size(&BinaryBatch.fill_circle_list/1, {0, 0, 1, 0x0000}, {1, 1, 2, 0xFFFF}),
      fill_circle_list_record:
        list_record_size(&BinaryBatch.fill_circle_list/1, {0, 0, 1, 0x0000}, {1, 1, 2, 0xFFFF}),
      draw_ellipse_list_header:
        list_header_size(
          &BinaryBatch.draw_ellipse_list/1,
          {0, 0, 1, 1, 0x0000},
          {1, 1, 2, 2, 0xFFFF}
        ),
      draw_ellipse_list_record:
        list_record_size(
          &BinaryBatch.draw_ellipse_list/1,
          {0, 0, 1, 1, 0x0000},
          {1, 1, 2, 2, 0xFFFF}
        ),
      fill_ellipse_list_header:
        list_header_size(
          &BinaryBatch.fill_ellipse_list/1,
          {0, 0, 1, 1, 0x0000},
          {1, 1, 2, 2, 0xFFFF}
        ),
      fill_ellipse_list_record:
        list_record_size(
          &BinaryBatch.fill_ellipse_list/1,
          {0, 0, 1, 1, 0x0000},
          {1, 1, 2, 2, 0xFFFF}
        ),
      draw_line_list_header:
        list_header_size(
          &BinaryBatch.draw_line_list/1,
          {0, 0, 1, 1, 0x0000},
          {1, 1, 2, 2, 0xFFFF}
        ),
      draw_line_list_record:
        list_record_size(
          &BinaryBatch.draw_line_list/1,
          {0, 0, 1, 1, 0x0000},
          {1, 1, 2, 2, 0xFFFF}
        ),
      draw_triangle_list_header:
        list_header_size(
          &BinaryBatch.draw_triangle_list/1,
          {0, 0, 1, 0, 0, 1, 0x0000},
          {1, 1, 2, 1, 1, 2, 0xFFFF}
        ),
      draw_triangle_list_record:
        list_record_size(
          &BinaryBatch.draw_triangle_list/1,
          {0, 0, 1, 0, 0, 1, 0x0000},
          {1, 1, 2, 1, 1, 2, 0xFFFF}
        ),
      fill_triangle_list_header:
        list_header_size(
          &BinaryBatch.fill_triangle_list/1,
          {0, 0, 1, 0, 0, 1, 0x0000},
          {1, 1, 2, 1, 1, 2, 0xFFFF}
        ),
      fill_triangle_list_record:
        list_record_size(
          &BinaryBatch.fill_triangle_list/1,
          {0, 0, 1, 0, 0, 1, 0x0000},
          {1, 1, 2, 1, 1, 2, 0xFFFF}
        ),
      set_clip_rect: byte_size(BinaryBatch.set_clip_rect(0, 0, 1, 1)),
      clear_clip_rect: byte_size(BinaryBatch.clear_clip_rect()),
      set_text_font_preset: byte_size(BinaryBatch.set_text_font_preset(:ascii)),
      set_text_size: byte_size(BinaryBatch.set_text_size(1)),
      set_text_datum: byte_size(BinaryBatch.set_text_datum(0)),
      set_text_wrap: byte_size(BinaryBatch.set_text_wrap(true)),
      set_cursor: byte_size(BinaryBatch.set_cursor(0, 0)),
      set_text_color_fg: byte_size(BinaryBatch.set_text_color(0xFFFF)),
      set_text_color_fg_bg: byte_size(BinaryBatch.set_text_color(0xFFFF, 0x0000)),
      draw_string_one_byte: byte_size(BinaryBatch.draw_string(0, 0, "a")),
      print_one_byte: byte_size(BinaryBatch.print("a")),
      println_empty: byte_size(BinaryBatch.println()),
      println_one_byte: byte_size(BinaryBatch.println("a")),
      draw_jpg_header: payload_header_size(BinaryBatch.draw_jpg(0, 0, <<1>>), 1),
      draw_jpg_scaled_header:
        payload_header_size(BinaryBatch.draw_jpg(0, 0, 0, 0, 0, 0, 1, 1, <<1>>), 1),
      push_image_rgb565_header:
        payload_header_size(BinaryBatch.push_image_rgb565(0, 0, 1, 1, <<0, 0>>), 2),
      set_palette_color: byte_size(BinaryBatch.set_palette_color(0, 0x000000)),
      set_pivot: byte_size(BinaryBatch.set_pivot(0, 0)),
      push_sprite: byte_size(BinaryBatch.push_sprite(1, 0, 0)),
      push_sprite_transparent: byte_size(BinaryBatch.push_sprite(1, 0, 0, 0x0000)),
      push_sprite_list_header: push_sprite_list_header_size(),
      push_sprite_list_record: push_sprite_list_record_size(),
      push_sprite_region_list_header: push_sprite_region_list_header_size(),
      push_sprite_region_list_record: push_sprite_region_list_record_size(),
      push_rotate_zoom: byte_size(BinaryBatch.push_rotate_zoom(1, 0, 0, 0, 1)),
      push_rotate_zoom_list_header: push_rotate_zoom_list_header_size(),
      push_rotate_zoom_list_record: push_rotate_zoom_list_record_size()
    }

    assert actual_sizes == @binary_batch_command_sizes
  end

  defp list_header_size(builder, first_record, second_record) do
    single = builder.([first_record])
    double = builder.([first_record, second_record])

    2 * byte_size(single) - byte_size(double)
  end

  defp list_record_size(builder, first_record, second_record) do
    single = builder.([first_record])
    double = builder.([first_record, second_record])

    byte_size(double) - byte_size(single)
  end

  defp payload_header_size(command, payload_size) do
    byte_size(command) - payload_size
  end

  defp native_render_private_opcodes do
    protocol_h_path =
      Path.expand("../../lgfx_port/include_internal/lgfx_port/protocol.h", __DIR__)

    ~r/#define LGFX_RENDER_OP_([A-Z0-9_]+)\s+\(\(uint8_t\)\s+0x([0-9A-Fa-f]+)u\)/
    |> Regex.scan(File.read!(protocol_h_path))
    |> Enum.map(fn [_match, name, value] ->
      {name |> String.downcase() |> String.to_atom(), String.to_integer(value, 16)}
    end)
    |> Map.new()
  end

  defp push_sprite_list_header_size do
    single = BinaryBatch.push_sprite_list([{1, 0, 0}])
    double = BinaryBatch.push_sprite_list([{1, 0, 0}, {2, 1, 1}])

    2 * byte_size(single) - byte_size(double)
  end

  defp push_sprite_list_record_size do
    single = BinaryBatch.push_sprite_list([{1, 0, 0}])
    double = BinaryBatch.push_sprite_list([{1, 0, 0}, {2, 1, 1}])

    byte_size(double) - byte_size(single)
  end

  defp push_sprite_region_list_header_size do
    single = BinaryBatch.push_sprite_region_list([{1, 0, 0, 1, 1, 0, 0}])

    double =
      BinaryBatch.push_sprite_region_list([
        {1, 0, 0, 1, 1, 0, 0},
        {2, 0, 0, 1, 1, 1, 1}
      ])

    2 * byte_size(single) - byte_size(double)
  end

  defp push_sprite_region_list_record_size do
    single = BinaryBatch.push_sprite_region_list([{1, 0, 0, 1, 1, 0, 0}])

    double =
      BinaryBatch.push_sprite_region_list([
        {1, 0, 0, 1, 1, 0, 0},
        {2, 0, 0, 1, 1, 1, 1}
      ])

    byte_size(double) - byte_size(single)
  end

  defp push_rotate_zoom_list_header_size do
    single = BinaryBatch.push_rotate_zoom_list([{1, 0, 0, 0, 1_024, 1_024}])

    double =
      BinaryBatch.push_rotate_zoom_list([
        {1, 0, 0, 0, 1_024, 1_024},
        {2, 1, 1, 0, 1_024, 1_024}
      ])

    2 * byte_size(single) - byte_size(double)
  end

  defp push_rotate_zoom_list_record_size do
    single = BinaryBatch.push_rotate_zoom_list([{1, 0, 0, 0, 1_024, 1_024}])

    double =
      BinaryBatch.push_rotate_zoom_list([
        {1, 0, 0, 0, 1_024, 1_024},
        {2, 1, 1, 0, 1_024, 1_024}
      ])

    byte_size(double) - byte_size(single)
  end

  test "freezes Elixir protocol capability bits" do
    actual_capabilities = %{
      sprite: Protocol.cap_sprite(),
      pushimage: Protocol.cap_pushimage(),
      last_error: Protocol.cap_last_error(),
      touch: Protocol.cap_touch(),
      palette: Protocol.cap_palette(),
      batch: Protocol.cap_batch()
    }

    assert actual_capabilities == @capabilities
  end
end
