# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Clip do
  @moduledoc false

  import AtomLGFX.Guards

  alias AtomLGFX.Protocol

  def set_clip_rect(handle, x, y, width, height, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             target_any(target) do
    Protocol.call_ok(
      handle,
      :set_clip_rect,
      target,
      0,
      [x, y, width, height]
    )
  end

  def clear_clip_rect(handle, target \\ 0)
      when target_any(target) do
    Protocol.call_ok(handle, :clear_clip_rect, target, 0, [])
  end
end
