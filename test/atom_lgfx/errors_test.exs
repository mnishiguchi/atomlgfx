# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.ErrorsTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.Errors

  test "formats render batch command failures with command index and opcode" do
    assert Errors.format_error({:batch_failed, {2, 0xF0, :bad_args}}) ==
             "render batch command 2 opcode 240 failed: :bad_args"
  end
end
