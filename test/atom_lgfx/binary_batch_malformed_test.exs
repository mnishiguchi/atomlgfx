# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatchMalformedTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.Cache
  alias AtomLGFX.Protocol

  test "empty binary submission is rejected before NIF call" do
    assert {:error, :empty_batch} = Protocol.submit_binary_batch(make_ref(), "")
  end

  test "non-binary submission is rejected before NIF call" do
    assert {:error, {:bad_binary_batch, [:not_binary]}} =
             Protocol.submit_binary_batch(make_ref(), [:not_binary])
  end

  test "binary payload larger than allowed is rejected before NIF call" do
    handle = make_ref()
    Cache.put_max_binary_bytes(handle, 4)
    large = <<0, 1, 2, 3, 4, 5, 6, 7, 8, 9>>

    assert {:error, {:binary_too_large, :submit_binary_batch, payload_size, 4}} =
             Protocol.submit_binary_batch(handle, large)

    assert payload_size == byte_size(large)
  end

  test "binary payload exactly at the advertised limit is allowed past client validation" do
    handle = make_ref()

    command =
      BinaryBatch.batch([
        BinaryBatch.target(0),
        BinaryBatch.fill_screen(0x0000)
      ])

    Cache.put_max_binary_bytes(handle, byte_size(command))

    assert {:error, {:unexpected_reply, :nif_not_loaded}} =
             Protocol.submit_binary_batch(handle, command)
  end
end
