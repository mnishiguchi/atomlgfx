# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatchDiagnosticsTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.BinaryBatch.Diagnostics

  describe "summary/1" do
    test "summarizes render-private, text, sprite, and display commands" do
      batch =
        BinaryBatch.batch([
          BinaryBatch.target(1),
          BinaryBatch.clear(0x0000),
          BinaryBatch.set_cursor(8, 8),
          BinaryBatch.println("sprite"),
          BinaryBatch.target(0),
          BinaryBatch.push_sprite(1, 24, 32),
          BinaryBatch.display()
        ])

      assert {:ok, summary} = Diagnostics.summary(batch)
      assert summary.command_count == 7
      assert summary.render_private_count == 2
      assert summary.scalar_count == 1
      assert summary.text_count == 2
      assert summary.sprite_push_count == 1
      assert summary.display_count == 1
      assert summary.target_count == 2
      assert summary.batch_bytes == byte_size(batch)
    end
  end

  describe "diagnose/1" do
    test "keeps partial context for malformed batches" do
      batch =
        BinaryBatch.batch([
          BinaryBatch.fill_screen(0x0000),
          <<AtomLGFX.OpSchema.opcode!(:draw_rect), 0, 0>>
        ])

      assert {:error, diagnosis} = Diagnostics.diagnose(batch)
      assert diagnosis.valid? == false
      assert diagnosis.failed_index == 1
      assert diagnosis.failed_op == :draw_rect
      assert diagnosis.decoded_command_count == 1
      assert diagnosis.last_decoded_command.op == :fill_screen
    end
  end

  describe "check_budget/2" do
    test "reports budget violations from summary metrics" do
      batch = BinaryBatch.batch([BinaryBatch.fill_screen(0x0000), BinaryBatch.display()])

      assert {:error, {:budget_exceeded, report}} =
               Diagnostics.check_budget(batch, max_command_count: 1)

      assert report.ok? == false
      assert [%{limit: :max_command_count, actual: 2, limit_value: 1}] = report.violations
    end
  end
end
