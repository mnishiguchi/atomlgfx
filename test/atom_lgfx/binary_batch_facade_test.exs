# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatchFacadeTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.BinaryBatch.Codec
  alias AtomLGFX.BinaryBatch.Diagnostics
  alias AtomLGFX.BinaryBatch.Validation

  describe "public facade" do
    test "keeps command builders compatible with the internal codec" do
      assert BinaryBatch.fill_rect(-1, 2, 3, 4, 0x1234) ==
               Codec.fill_rect(-1, 2, 3, 4, 0x1234)

      assert BinaryBatch.push_sprite(1, 10, 20, {:index, 0}) ==
               Codec.push_sprite(1, 10, 20, {:index, 0})
    end

    test "keeps diagnostics and validation routed through focused modules" do
      batch =
        BinaryBatch.batch([
          BinaryBatch.target(1),
          BinaryBatch.clear(0x0000),
          BinaryBatch.target(0),
          BinaryBatch.push_sprite(1, 0, 0),
          BinaryBatch.display()
        ])

      assert BinaryBatch.decode(batch) == Codec.decode(batch)
      assert BinaryBatch.validate(batch) == Validation.validate(batch)
      assert BinaryBatch.summary(batch) == Diagnostics.summary(batch)
      assert BinaryBatch.diagnose(batch) == Diagnostics.diagnose(batch)
    end
  end
end
