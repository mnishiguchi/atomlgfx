# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Color do
  @moduledoc """
  AtomLGFX の色値を扱うための補助関数群です。
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

  @named_rgb565_colors %{
    black: 0x0000,
    navy: 0x000F,
    dark_green: 0x03E0,
    dark_cyan: 0x03EF,
    maroon: 0x7800,
    purple: 0x780F,
    olive: 0x7BE0,
    light_grey: 0xC618,
    light_gray: 0xC618,
    dark_grey: 0x7BEF,
    dark_gray: 0x7BEF,
    blue: 0x001F,
    green: 0x07E0,
    cyan: 0x07FF,
    red: 0xF800,
    magenta: 0xF81F,
    yellow: 0xFFE0,
    white: 0xFFFF,
    orange: 0xFD20,
    green_yellow: 0xAFE5,
    pink: 0xFE19,
    brown: 0x9A60,
    gold: 0xFEA0,
    silver: 0xC618,
    sky_blue: 0x867D,
    violet: 0x915C,
    transparent: 0x0120
  }

  @doc """
  名前付き色に対応する RGB565 値を返します。
  """
  @spec named_rgb565(atom()) :: {:ok, rgb565_value()} | {:error, term()}
  def named_rgb565(name) when is_atom(name) do
    case Map.fetch(@named_rgb565_colors, name) do
      {:ok, color} -> {:ok, color}
      :error -> {:error, {:unknown_color_name, name}}
    end
  end

  @doc """
  名前付き色に対応する圧縮 RGB888 値を返します。
  """
  @spec named_rgb888(atom()) :: {:ok, rgb888_value()} | {:error, term()}
  def named_rgb888(name) when is_atom(name) do
    with {:ok, color} <- named_rgb565(name) do
      {:ok, color16to24(color)}
    end
  end

  @doc """
  利用者向けの表示色を RGB565 へ正規化します。

  受け付ける値:

  - RGB565 整数
  - `:black`、`:white`、`:red` などの名前付き色
  - `{:rgb565, value}`
  - `{:rgb565, r, g, b}`
  - `{:rgb888, value}`
  - `{:rgb888, r, g, b}`
  - `{:rgb, r, g, b}`
  """
  @spec normalize_display(term()) :: {:ok, rgb565_value()} | {:error, term()}
  def normalize_display(color) when rgb565(color), do: {:ok, color}
  def normalize_display({:rgb565, color}) when rgb565(color), do: {:ok, color}

  def normalize_display({:rgb565, r, g, b}) when u8(r) and u8(g) and u8(b) do
    {:ok, color565(r, g, b)}
  end

  def normalize_display({:rgb888, color}) when color888(color) do
    {:ok, color24to16(color)}
  end

  def normalize_display({:rgb888, r, g, b}) when u8(r) and u8(g) and u8(b) do
    {:ok, color565(r, g, b)}
  end

  def normalize_display({:rgb, r, g, b}) when u8(r) and u8(g) and u8(b) do
    {:ok, color565(r, g, b)}
  end

  def normalize_display(name) when is_atom(name), do: named_rgb565(name)
  def normalize_display(color), do: {:error, {:bad_display_color, color}}

  @doc """
  利用者向けの配色表の色を圧縮 RGB888 へ正規化します。

  受け付ける値:

  - 圧縮 RGB888 整数
  - `:black`、`:white`、`:red` などの名前付き色
  - `{:rgb888, value}`
  - `{:rgb888, r, g, b}`
  - `{:rgb, r, g, b}`
  - `{:rgb565, value}`
  - `{:rgb565, r, g, b}`
  """
  @spec normalize_palette(term()) :: {:ok, rgb888_value()} | {:error, term()}
  def normalize_palette(color) when color888(color), do: {:ok, color}
  def normalize_palette({:rgb888, color}) when color888(color), do: {:ok, color}

  def normalize_palette({:rgb888, r, g, b}) when u8(r) and u8(g) and u8(b) do
    {:ok, color888(r, g, b)}
  end

  def normalize_palette({:rgb, r, g, b}) when u8(r) and u8(g) and u8(b) do
    {:ok, color888(r, g, b)}
  end

  def normalize_palette({:rgb565, color}) when rgb565(color) do
    {:ok, color16to24(color)}
  end

  def normalize_palette({:rgb565, r, g, b}) when u8(r) and u8(g) and u8(b) do
    {:ok, color888(r, g, b)}
  end

  def normalize_palette(name) when is_atom(name), do: named_rgb888(name)
  def normalize_palette(color), do: {:error, {:bad_palette_color, color}}

  @doc """
  8ビットの RGB 各成分を RGB332 色へまとめます。
  """
  @spec color332(0..255, 0..255, 0..255) :: rgb332_value
  def color332(r, g, b) when u8(r) and u8(g) and u8(b) do
    (r &&& 0xE0) ||| (g &&& 0xE0) >>> 3 ||| b >>> 6
  end

  @doc """
  8ビットの RGB 各成分を RGB565 色へまとめます。
  """
  @spec color565(0..255, 0..255, 0..255) :: rgb565_value
  def color565(r, g, b) when u8(r) and u8(g) and u8(b) do
    (r &&& 0xF8) <<< 8 ||| (g &&& 0xFC) <<< 3 ||| b >>> 3
  end

  @doc """
  8ビットの RGB 各成分を圧縮 RGB888 色へまとめます。
  """
  @spec color888(0..255, 0..255, 0..255) :: rgb888_value
  def color888(r, g, b) when u8(r) and u8(g) and u8(b) do
    r <<< 16 ||| g <<< 8 ||| b
  end

  @doc """
  1つの RGB565 色値のバイト順を変換します。
  """
  @spec swap565(rgb565_value) :: rgb565_value
  def swap565(color) when rgb565(color) do
    (color &&& 0x00FF) <<< 8 ||| (color &&& 0xFF00) >>> 8
  end

  @doc """
  RGB 各成分を RGB565 色へまとめた後、バイト順を変換します。
  """
  @spec swap565(0..255, 0..255, 0..255) :: rgb565_value
  def swap565(r, g, b) when u8(r) and u8(g) and u8(b) do
    r
    |> color565(g, b)
    |> swap565()
  end

  @doc """
  1つの RGB888 色値のバイト順を変換します。
  """
  @spec swap888(rgb888_value) :: rgb888_value
  def swap888(color) when color888(color) do
    (color &&& 0x0000FF) <<< 16 ||| (color &&& 0x00FF00) ||| (color &&& 0xFF0000) >>> 16
  end

  @doc """
  RGB 各成分を RGB888 色へまとめた後、バイト順を変換します。
  """
  @spec swap888(0..255, 0..255, 0..255) :: rgb888_value
  def swap888(r, g, b) when u8(r) and u8(g) and u8(b) do
    r
    |> color888(g, b)
    |> swap888()
  end

  @doc """
  RGB565 を RGB332 へ変換します。
  """
  @spec color16to8(rgb565_value) :: rgb332_value
  def color16to8(color565_value) when rgb565(color565_value) do
    color565_value
    |> color16to24()
    |> color24to8()
  end

  @doc """
  RGB332 を RGB565 へ変換します。
  """
  @spec color8to16(rgb332_value) :: rgb565_value
  def color8to16(color332_value) when u8(color332_value) do
    r3 = color332_value >>> 5 &&& 0x07
    g3 = color332_value >>> 2 &&& 0x07
    b2 = color332_value &&& 0x03

    r8 = r3 <<< 5 ||| r3 <<< 2 ||| r3 >>> 1
    g8 = g3 <<< 5 ||| g3 <<< 2 ||| g3 >>> 1
    b8 = b2 <<< 6 ||| b2 <<< 4 ||| b2 <<< 2 ||| b2

    color565(r8, g8, b8)
  end

  @doc """
  RGB565 を圧縮 RGB888 へ変換します。
  """
  @spec color16to24(rgb565_value) :: rgb888_value
  def color16to24(color565_value) when rgb565(color565_value) do
    r5 = color565_value >>> 11 &&& 0x1F
    g6 = color565_value >>> 5 &&& 0x3F
    b5 = color565_value &&& 0x1F

    r8 = r5 <<< 3 ||| r5 >>> 2
    g8 = g6 <<< 2 ||| g6 >>> 4
    b8 = b5 <<< 3 ||| b5 >>> 2

    color888(r8, g8, b8)
  end

  @doc """
  圧縮 RGB888 を RGB565 へ変換します。
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
  RGB565 表示色を `{:rgb565, value}` として明示します。
  """
  @spec display(rgb565_value) :: display_descriptor
  def display(color) when rgb565(color), do: {:rgb565, color}

  @doc """
  圧縮 RGB888 の配色表色を `{:rgb888, value}` として明示します。
  """
  @spec palette(rgb888_value) :: palette_descriptor
  def palette(color) when color888(color), do: {:rgb888, color}

  @doc """
  配色表番号を `{:index, value}` として明示します。
  """
  @spec index(palette_index_value) :: index_descriptor
  def index(value) when palette_index(value), do: {:index, value}

  @doc """
  1つの RGB565 値を通常のリトルエンディアン16ビット語へ符号化します。
  """
  @spec rgb565_le(rgb565_value) :: binary
  def rgb565_le(color) when rgb565(color) do
    <<color::16-little>>
  end

  @doc """
  1つの RGB565 値をビッグエンディアン16ビット語へ符号化します。
  """
  @spec rgb565_be(rgb565_value) :: binary
  def rgb565_be(color) when rgb565(color) do
    <<color::16-big>>
  end

  @doc """
  RGB565 値の一覧を通常のリトルエンディアン語へ符号化します。
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
  RGB565 値の一覧をビッグエンディアン語へ符号化します。
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
  RGB565 値の一覧を、上流の `swap565_t` 形式に相当する事前入れ替え済みの生バイト列へ符号化します。
  """
  @spec pixels_swap565([rgb565_value]) :: binary
  def pixels_swap565(colors) when is_list(colors) do
    pixels_be(colors)
  end

  @doc """
  RGB565 の黒色です。
  """
  @spec black() :: rgb565_value
  def black, do: 0x0000

  @doc """
  RGB565 の紺色です。
  """
  @spec navy() :: rgb565_value
  def navy, do: 0x000F

  @doc """
  RGB565 の濃い緑色です。
  """
  @spec dark_green() :: rgb565_value
  def dark_green, do: 0x03E0

  @doc """
  RGB565 の濃いシアン色です。
  """
  @spec dark_cyan() :: rgb565_value
  def dark_cyan, do: 0x03EF

  @doc """
  RGB565 のえび茶色です。
  """
  @spec maroon() :: rgb565_value
  def maroon, do: 0x7800

  @doc """
  RGB565 の紫色です。
  """
  @spec purple() :: rgb565_value
  def purple, do: 0x780F

  @doc """
  RGB565 のオリーブ色です。
  """
  @spec olive() :: rgb565_value
  def olive, do: 0x7BE0

  @doc """
  RGB565 の薄い灰色です。
  """
  @spec light_grey() :: rgb565_value
  def light_grey, do: 0xC618

  @doc """
  RGB565 の濃い灰色です。
  """
  @spec dark_grey() :: rgb565_value
  def dark_grey, do: 0x7BEF

  @doc """
  RGB565 の青色です。
  """
  @spec blue() :: rgb565_value
  def blue, do: 0x001F

  @doc """
  RGB565 の緑色です。
  """
  @spec green() :: rgb565_value
  def green, do: 0x07E0

  @doc """
  RGB565 のシアン色です。
  """
  @spec cyan() :: rgb565_value
  def cyan, do: 0x07FF

  @doc """
  RGB565 の赤色です。
  """
  @spec red() :: rgb565_value
  def red, do: 0xF800

  @doc """
  RGB565 のマゼンタ色です。
  """
  @spec magenta() :: rgb565_value
  def magenta, do: 0xF81F

  @doc """
  RGB565 の黄色です。
  """
  @spec yellow() :: rgb565_value
  def yellow, do: 0xFFE0

  @doc """
  RGB565 の白色です。
  """
  @spec white() :: rgb565_value
  def white, do: 0xFFFF

  @doc """
  RGB565 の橙色です。
  """
  @spec orange() :: rgb565_value
  def orange, do: 0xFD20

  @doc """
  RGB565 の黄緑色です。
  """
  @spec green_yellow() :: rgb565_value
  def green_yellow, do: 0xAFE5

  @doc """
  RGB565 の桃色です。
  """
  @spec pink() :: rgb565_value
  def pink, do: 0xFE19

  @doc """
  RGB565 の茶色です。
  """
  @spec brown() :: rgb565_value
  def brown, do: 0x9A60

  @doc """
  RGB565 の金色です。
  """
  @spec gold() :: rgb565_value
  def gold, do: 0xFEA0

  @doc """
  RGB565 の銀色です。
  """
  @spec silver() :: rgb565_value
  def silver, do: 0xC618

  @doc """
  RGB565 の空色色です。
  """
  @spec sky_blue() :: rgb565_value
  def sky_blue, do: 0x867D

  @doc """
  RGB565 のすみれ色色です。
  """
  @spec violet() :: rgb565_value
  def violet, do: 0x915C

  @doc """
  上流と同じ形式の特殊な透明色定数です。
  """
  @spec transparent() :: rgb565_value
  def transparent, do: 0x0120

  @doc """
  `light_grey/0` の別名です。
  """
  @spec light_gray() :: rgb565_value
  def light_gray, do: light_grey()

  @doc """
  `dark_grey/0` の別名です。
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

  defp red8(color888_value), do: color888_value >>> 16 &&& 0xFF
  defp green8(color888_value), do: color888_value >>> 8 &&& 0xFF
  defp blue8(color888_value), do: color888_value &&& 0xFF
end
