# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.RenderBatchEncoderTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.RenderBatch.Encoder

  describe "encode_normalized/1" do
    test "encodes normalized render commands" do
      expected =
        BinaryBatch.batch([
          BinaryBatch.clear(0x0000),
          BinaryBatch.set_cursor(4, 8),
          BinaryBatch.println("ok")
        ])

      assert Encoder.encode_normalized([
               {:clear, 0x0000},
               {:set_cursor, 4, 8},
               {:println, "ok"}
             ]) == {:ok, expected}
    end

    test "rejects non-list commands" do
      assert Encoder.encode_normalized(:bad_commands) ==
               {:error, {:bad_render_commands, :bad_commands}}
    end

    test "reports unexpected normalized command shapes as bad render batches" do
      assert {:error, {:bad_render_batch, _reason}} = Encoder.encode_normalized([{:clear}])
    end

    test "rejects unsupported normalized commands without relying on exceptions" do
      assert Encoder.encode_normalized([{:unknown_command, :value}]) ==
               {:error,
                {:bad_render_batch,
                 "unsupported normalized render command: {:unknown_command, :value}"}}
    end
  end
end
