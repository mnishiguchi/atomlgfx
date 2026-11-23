# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.NativeMovingIconsTest do
  use ExUnit.Case, async: true

  alias AtomLGFX.Native

  test "NIF benchmark functions have host fallbacks" do
    assert Native.moving_icons_start(<<>>, <<>>, <<>>, 1) == :nif_not_loaded
    assert Native.moving_icons_stats() == :nif_not_loaded
    assert Native.moving_icons_stop() == :nif_not_loaded
  end
end
