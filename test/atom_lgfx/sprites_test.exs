# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.SpritesTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.Protocol
  alias AtomLGFX.Sprites

  test "encodes push rotate zoom list payload as fixed-width little-endian records" do
    instances = [
      {1, -2, 300, 1234, 1024, 2048},
      {4, 0, -1, 35_999, 512, 512}
    ]

    assert {:ok, 0, payload} =
             Sprites.encode_push_rotate_zoom_list_payload(
               instances,
               transparent: 0x0000,
               y_offset: 64
             )

    assert payload ==
             <<?P, ?R, ?Z, ?L, 1, 1, 0x0000::little-16, 64::little-signed-16, 2::little-16, 1, 0,
               -2::little-signed-16, 300::little-signed-16, 1234::little-16, 1024::little-16,
               2048::little-16, 4, 0, 0::little-signed-16, -1::little-signed-16,
               35_999::little-16, 512::little-16, 512::little-16>>
  end

  test "encodes indexed transparent key with protocol flag" do
    assert {:ok, flags, <<?P, ?R, ?Z, ?L, 1, 1, 7::little-16, _rest::binary>>} =
             Sprites.encode_push_rotate_zoom_list_payload(
               [{1, 0, 0, 0, 1024, 1024}],
               transparent: {:index, 7}
             )

    assert flags == Protocol.transparent_index_flag()
  end

  test "rejects malformed push rotate zoom list instances" do
    assert {:error, :empty_batch} =
             Sprites.encode_push_rotate_zoom_list_payload([])

    assert {:error, {:bad_sprite_transform_instance, {0, 0, 0, 0, 1024, 1024}}} =
             Sprites.encode_push_rotate_zoom_list_payload([{0, 0, 0, 0, 1024, 1024}])

    assert {:error, {:bad_sprite_transform_instance, {1, 0, 0, 36_000, 1024, 1024}}} =
             Sprites.encode_push_rotate_zoom_list_payload([{1, 0, 0, 36_000, 1024, 1024}])

    assert {:error, {:bad_sprite_transform_instance, {1, 0, 0, 0, 0, 1024}}} =
             Sprites.encode_push_rotate_zoom_list_payload([{1, 0, 0, 0, 0, 1024}])
  end
end
