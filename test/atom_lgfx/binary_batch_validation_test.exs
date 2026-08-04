# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatchValidationTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.BinaryBatch.Validation

  describe "validate/1" do
    test "accepts a valid command stream" do
      batch =
        BinaryBatch.batch([
          BinaryBatch.fill_screen(0x0000),
          BinaryBatch.set_cursor(8, 8),
          BinaryBatch.println("ok"),
          BinaryBatch.display()
        ])

      assert Validation.validate(batch) == :ok
      assert BinaryBatch.validate(batch) == :ok
    end

    test "rejects an empty command stream" do
      assert Validation.validate(<<>>) == {:error, :empty_batch}
      assert BinaryBatch.validate(<<>>) == {:error, :empty_batch}
    end

    test "rejects a malformed command stream" do
      malformed = <<AtomLGFX.OpSchema.opcode!(:draw_rect), 0, 0>>

      assert {:error, {:batch_failed, 0, _opcode, :truncated}} = Validation.validate(malformed)
      assert BinaryBatch.validate(malformed) == Validation.validate(malformed)
    end
  end

  describe "validate!/1" do
    test "raises a formatted error for malformed batches" do
      assert_raise ArgumentError, ~r/batch must not be empty/, fn ->
        Validation.validate!(<<>>)
      end
    end
  end
end
