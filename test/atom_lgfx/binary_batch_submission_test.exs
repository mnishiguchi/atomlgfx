# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatchSubmissionTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.BinaryBatch
  alias AtomLGFX.BinaryBatch.Submission

  describe "render_checked/2" do
    test "returns validation errors before crossing the port boundary" do
      assert Submission.render_checked(self(), <<>>) == {:error, :empty_batch}
      assert BinaryBatch.render_checked(self(), <<>>) == {:error, :empty_batch}
    end
  end
end
