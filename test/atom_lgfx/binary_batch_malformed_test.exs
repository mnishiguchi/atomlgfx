# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatchMalformedTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.Cache
  alias AtomLGFX.Protocol

  test "empty binary submission is rejected before port call" do
    assert {:error, :empty_batch} = Protocol.submit_binary_batch(make_ref(), "")
  end

  test "non-binary submission is rejected before port call" do
    assert {:error, {:bad_binary_batch, [:not_binary]}} =
             Protocol.submit_binary_batch(make_ref(), [:not_binary])
  end

  test "binary payload larger than allowed is rejected before port call" do
    port = make_ref()
    Cache.put_max_binary_bytes(port, 4)
    large = <<0, 1, 2, 3, 4, 5, 6, 7, 8, 9>>

    assert {:error, {:binary_too_large, :submit_binary_batch, payload_size, 4}} =
             Protocol.submit_binary_batch(port, large)

    assert payload_size == byte_size(large)
  end

  test "binary payload exactly at the advertised limit is allowed past client validation" do
    port = make_ref()

    command =
      BinaryBatch.batch([
        BinaryBatch.target(0),
        BinaryBatch.fill_screen(0x0000)
      ])

    Cache.put_max_binary_bytes(port, byte_size(command))

    assert {:error, {:port_call_exit, _reason}} = Protocol.submit_binary_batch(port, command)
  end
end
