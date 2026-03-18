# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule LGFXPort.Guards do
  @moduledoc false

  defguard i16(v) when is_integer(v) and v >= -0x8000 and v <= 0x7FFF
  defguard u16(v) when is_integer(v) and v >= 0 and v <= 0xFFFF
  defguard i32(v) when is_integer(v) and v >= -0x8000_0000 and v <= 0x7FFF_FFFF
  defguard u8(v) when is_integer(v) and v >= 0 and v <= 0xFF
  defguard positive_i32(v) when is_integer(v) and v >= 1 and v <= 0x7FFF_FFFF

  defguard target_any(v) when is_integer(v) and v >= 0 and v <= 254
  defguard sprite_handle(v) when is_integer(v) and v >= 1 and v <= 254

  defguard color888(v) when is_integer(v) and v >= 0 and v <= 0xFFFFFF
  defguard rgb565(v) when is_integer(v) and v >= 0 and v <= 0xFFFF
  defguard palette_index(v) when is_integer(v) and v >= 0 and v <= 0xFF
end
