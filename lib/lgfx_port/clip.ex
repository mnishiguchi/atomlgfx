defmodule LGFXPort.Clip do
  @moduledoc false

  import LGFXPort.Guards

  alias LGFXPort.Protocol

  def set_clip_rect(port, x, y, width, height, target \\ 0)
      when i16(x) and i16(y) and
             u16(width) and width >= 1 and
             u16(height) and height >= 1 and
             target_any(target) do
    Protocol.call_ok(
      port,
      :setClipRect,
      target,
      0,
      [x, y, width, height],
      Protocol.long_timeout()
    )
  end

  def clear_clip_rect(port, target \\ 0)
      when target_any(target) do
    Protocol.call_ok(port, :clearClipRect, target, 0, [], Protocol.long_timeout())
  end
end
