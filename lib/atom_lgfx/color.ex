# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Color do
  @moduledoc """
  Helpers for working with AtomLGFX color values.
  """

  import Bitwise
  import AtomLGFX.Guards

  @type rgb332_value :: 0..0xFF
  @type rgb565_value :: 0..0xFFFF
  @type rgb888_value :: 0..0xFFFFFF
  @type palette_index_value :: 0..0xFF

  @type display_descriptor :: {:rgb565, rgb565_value}
  @type palette_descriptor :: {:rgb888, rgb888_value}
  @type index_descriptor :: {:index, palette_index_value}

  @doc """
  Packs 8-bit RGB channels into an RGB332 color.
  """
  @spec color332(0..255, 0..255, 0..255) :: rgb332_value
  def color332(r, g, b) when u8(r) and u8(g) and u8(b) do
    (r &&& 0xE0) ||| ((g &&& 0xE0) >>> 3) ||| (b >>> 6)
  end

  @doc """
  Packs 8-bit RGB channels into an RGB565 color.
  """
  @spec color565(0..255, 0..255, 0..255) :: rgb565_value
  def color565(r, g, b) when u8(r) and u8(g) and u8(b) do
    (r &&& 0xF8) <<< 8 ||| (g &&& 0xFC) <<< 3 ||| (b >>> 3)
  end

  @doc """
  Packs 8-bit RGB channels into a packed RGB888 color.
  """
  @spec color888(0..255, 0..255, 0..255) :: rgb888_value
  def color888(r, g, b) when u8(r) and u8(g) and u8(b) do
    (r <<< 16) ||| (g <<< 8) ||| b
  end

  @doc """
  Endian-converts one RGB565 scalar color value.
  """
  @spec swap565(rgb565_value) :: rgb565_value
  def swap565(color) when rgb565(color) do
    ((color &&& 0x00FF) <<< 8) ||| ((color &&& 0xFF00) >>> 8)
  end

  @doc """
  Packs RGB channels, then endian-converts the RGB565 scalar color value.
  """
  @spec swap565(0..255, 0..255, 0..255) :: rgb565_value
  def swap565(r, g, b) when u8(r) and u8(g) and u8(b) do
    r
    |> color565(g, b)
    |> swap565()
  end

  @doc """
  Endian-converts one RGB888 scalar color value.
  """
  @spec swap888(rgb888_value) :: rgb888_value
  def swap888(color) when color888(color) do
    ((color &&& 0x0000FF) <<< 16) ||| (color &&& 0x00FF00) ||| ((color &&& 0xFF0000) >>> 16)
  end

  @doc """
  Packs RGB channels, then endian-converts the RGB888 scalar color value.
  """
  @spec swap888(0..255, 0..255, 0..255) :: rgb888_value
  def swap888(r, g, b) when u8(r) and u8(g) and u8(b) do
    r
    |> color888(g, b)
    |> swap888()
  end

  @doc """
  Converts RGB565 to RGB332.
  """
  @spec color16to8(rgb565_value) :: rgb332_value
  def color16to8(color565_value) when rgb565(color565_value) do
    color565_value
    |> color16to24()
    |> color24to8()
  end

  @doc """
  Converts RGB332 to RGB565.
  """
  @spec color8to16(rgb332_value) :: rgb565_value
  def color8to16(color332_value) when u8(color332_value) do
    r3 = (color332_value >>> 5) &&& 0x07
    g3 = (color332_value >>> 2) &&& 0x07
    b2 = color332_value &&& 0x03

    r8 = (r3 <<< 5) ||| (r3 <<< 2) ||| (r3 >>> 1)
    g8 = (g3 <<< 5) ||| (g3 <<< 2) ||| (g3 >>> 1)
    b8 = (b2 <<< 6) ||| (b2 <<< 4) ||| (b2 <<< 2) ||| b2

    color565(r8, g8, b8)
  end

  @doc """
  Converts RGB565 to packed RGB888.
  """
  @spec color16to24(rgb565_value) :: rgb888_value
  def color16to24(color565_value) when rgb565(color565_value) do
    r5 = (color565_value >>> 11) &&& 0x1F
    g6 = (color565_value >>> 5) &&& 0x3F
    b5 = color565_value &&& 0x1F

    r8 = (r5 <<< 3) ||| (r5 >>> 2)
    g8 = (g6 <<< 2) ||| (g6 >>> 4)
    b8 = (b5 <<< 3) ||| (b5 >>> 2)

    color888(r8, g8, b8)
  end

  @doc """
  Converts packed RGB888 to RGB565.
  """
  @spec color24to16(rgb888_value) :: rgb565_value
  def color24to16(color888_value) when color888(color888_value) do
    color565(
      red8(color888_value),
      green8(color888_value),
      blue8(color888_value)
    )
  end

  @doc """
  Wraps an RGB565 display color explicitly as `{:rgb565, value}`.
  """
  @spec display(rgb565_value) :: display_descriptor
  def display(color) when rgb565(color), do: {:rgb565, color}

  @doc """
  Wraps a packed RGB888 palette color explicitly as `{:rgb888, value}`.
  """
  @spec palette(rgb888_value) :: palette_descriptor
  def palette(color) when color888(color), do: {:rgb888, color}

  @doc """
  Wraps a palette index explicitly as `{:index, value}`.
  """
  @spec index(palette_index_value) :: index_descriptor
  def index(value) when palette_index(value), do: {:index, value}

  @doc """
  Encodes one RGB565 value as an ordinary little-endian 16-bit binary word.
  """
  @spec rgb565_le(rgb565_value) :: binary
  def rgb565_le(color) when rgb565(color) do
    <<color::16-little>>
  end

  @doc """
  Encodes one RGB565 value as a big-endian 16-bit binary word.
  """
  @spec rgb565_be(rgb565_value) :: binary
  def rgb565_be(color) when rgb565(color) do
    <<color::16-big>>
  end

  @doc """
  Encodes a list of RGB565 values as ordinary little-endian words.
  """
  @spec pixels_le([rgb565_value]) :: binary
  def pixels_le(colors) when is_list(colors) do
    pixels_le(colors, [])
  end

  defp pixels_le([], acc) do
    acc
    |> :lists.reverse()
    |> :erlang.iolist_to_binary()
  end

  defp pixels_le([color | rest], acc) when rgb565(color) do
    pixels_le(rest, [<<color::16-little>> | acc])
  end

  @doc """
  Encodes a list of RGB565 values as big-endian words.
  """
  @spec pixels_be([rgb565_value]) :: binary
  def pixels_be(colors) when is_list(colors) do
    pixels_be(colors, [])
  end

  defp pixels_be([], acc) do
    acc
    |> :lists.reverse()
    |> :erlang.iolist_to_binary()
  end

  defp pixels_be([color | rest], acc) when rgb565(color) do
    pixels_be(rest, [<<color::16-big>> | acc])
  end

  @doc """
  Encodes a list of RGB565 values as pre-swapped raw bytes, analogous to upstream
  `swap565_t`-style data in memory.
  """
  @spec pixels_swap565([rgb565_value]) :: binary
  def pixels_swap565(colors) when is_list(colors) do
    pixels_be(colors)
  end

  @doc """
  RGB565 black.
  """
  @spec black() :: rgb565_value
  def black, do: 0x0000

  @doc """
  RGB565 navy.
  """
  @spec navy() :: rgb565_value
  def navy, do: 0x000F

  @doc """
  RGB565 dark green.
  """
  @spec dark_green() :: rgb565_value
  def dark_green, do: 0x03E0

  @doc """
  RGB565 dark cyan.
  """
  @spec dark_cyan() :: rgb565_value
  def dark_cyan, do: 0x03EF

  @doc """
  RGB565 maroon.
  """
  @spec maroon() :: rgb565_value
  def maroon, do: 0x7800

  @doc """
  RGB565 purple.
  """
  @spec purple() :: rgb565_value
  def purple, do: 0x780F

  @doc """
  RGB565 olive.
  """
  @spec olive() :: rgb565_value
  def olive, do: 0x7BE0

  @doc """
  RGB565 light grey.
  """
  @spec light_grey() :: rgb565_value
  def light_grey, do: 0xC618

  @doc """
  RGB565 dark grey.
  """
  @spec dark_grey() :: rgb565_value
  def dark_grey, do: 0x7BEF

  @doc """
  RGB565 blue.
  """
  @spec blue() :: rgb565_value
  def blue, do: 0x001F

  @doc """
  RGB565 green.
  """
  @spec green() :: rgb565_value
  def green, do: 0x07E0

  @doc """
  RGB565 cyan.
  """
  @spec cyan() :: rgb565_value
  def cyan, do: 0x07FF

  @doc """
  RGB565 red.
  """
  @spec red() :: rgb565_value
  def red, do: 0xF800

  @doc """
  RGB565 magenta.
  """
  @spec magenta() :: rgb565_value
  def magenta, do: 0xF81F

  @doc """
  RGB565 yellow.
  """
  @spec yellow() :: rgb565_value
  def yellow, do: 0xFFE0

  @doc """
  RGB565 white.
  """
  @spec white() :: rgb565_value
  def white, do: 0xFFFF

  @doc """
  RGB565 orange.
  """
  @spec orange() :: rgb565_value
  def orange, do: 0xFD20

  @doc """
  RGB565 green-yellow.
  """
  @spec green_yellow() :: rgb565_value
  def green_yellow, do: 0xAFE5

  @doc """
  RGB565 pink.
  """
  @spec pink() :: rgb565_value
  def pink, do: 0xFE19

  @doc """
  RGB565 brown.
  """
  @spec brown() :: rgb565_value
  def brown, do: 0x9A60

  @doc """
  RGB565 gold.
  """
  @spec gold() :: rgb565_value
  def gold, do: 0xFEA0

  @doc """
  RGB565 silver.
  """
  @spec silver() :: rgb565_value
  def silver, do: 0xC618

  @doc """
  RGB565 sky blue.
  """
  @spec sky_blue() :: rgb565_value
  def sky_blue, do: 0x867D

  @doc """
  RGB565 violet.
  """
  @spec violet() :: rgb565_value
  def violet, do: 0x915C

  @doc """
  Special upstream-style transparent color constant.
  """
  @spec transparent() :: rgb565_value
  def transparent, do: 0x0120

  @doc """
  Alias for `light_grey/0`.
  """
  @spec light_gray() :: rgb565_value
  def light_gray, do: light_grey()

  @doc """
  Alias for `dark_grey/0`.
  """
  @spec dark_gray() :: rgb565_value
  def dark_gray, do: dark_grey()

  defp color24to8(color888_value) do
    color332(
      red8(color888_value),
      green8(color888_value),
      blue8(color888_value)
    )
  end

  defp red8(color888_value), do: (color888_value >>> 16) &&& 0xFF
  defp green8(color888_value), do: (color888_value >>> 8) &&& 0xFF
  defp blue8(color888_value), do: color888_value &&& 0xFF
end
