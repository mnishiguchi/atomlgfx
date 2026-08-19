# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule LGFX do
  @moduledoc """
  AtomVM から LovyanGFX を直接呼び出すための軽量な API です。

  LovyanGFX の既存例を Elixir へ置き換えやすいよう、よく使う関数名を
  `snake_case` でほぼそのまま提供します。

      :ok = LGFX.init()
      :ok = LGFX.fill_screen(:black)
      :ok = LGFX.draw_rect(20, 20, 100, 60, :red)
      :ok = LGFX.fill_circle(160, 120, 30, :blue)
      :ok = LGFX.draw_string("Hello", 20, 100)

  多数の描画命令をまとめる場合は `batch/2` を使用します。

      :ok =
        LGFX.batch([
          {:fill_screen, :black},
          {:fill_rect, 10, 10, 100, 40, :red},
          {:draw_string, "Hello", 20, 20}
        ])

  現時点の `init/0` は構築時の機器設定を使用します。実行時に詳細な機器設定を
  指定する既存の `AtomLGFX.open/1` ポート API は引き続き利用できます。
  """

  alias AtomLGFX.Color
  alias AtomLGFX.Command
  alias AtomLGFX.Native
  alias AtomLGFX.RenderBatch

  def init, do: Native.init()
  def close, do: Native.close()

  def width, do: Native.width()
  def height, do: Native.height()

  def start_write, do: Native.start_write()
  def end_write, do: Native.end_write()
  def display, do: Native.display()

  def set_rotation(rotation), do: Native.set_rotation(rotation)
  def set_brightness(brightness), do: Native.set_brightness(brightness)

  def fill_screen(color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_screen(color)
    end
  end

  def clear(color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.clear(color)
    end
  end

  def draw_pixel(x, y, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_pixel(x, y, color)
    end
  end

  def draw_fast_vline(x, y, height, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_fast_vline(x, y, height, color)
    end
  end

  def draw_fast_hline(x, y, width, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_fast_hline(x, y, width, color)
    end
  end

  def draw_line(x0, y0, x1, y1, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_line(x0, y0, x1, y1, color)
    end
  end

  def draw_rect(x, y, width, height, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_rect(x, y, width, height, color)
    end
  end

  def fill_rect(x, y, width, height, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_rect(x, y, width, height, color)
    end
  end

  def draw_round_rect(x, y, width, height, radius, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_round_rect(x, y, width, height, radius, color)
    end
  end

  def fill_round_rect(x, y, width, height, radius, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_round_rect(x, y, width, height, radius, color)
    end
  end

  def draw_circle(x, y, radius, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_circle(x, y, radius, color)
    end
  end

  def fill_circle(x, y, radius, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_circle(x, y, radius, color)
    end
  end

  def draw_ellipse(x, y, radius_x, radius_y, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_ellipse(x, y, radius_x, radius_y, color)
    end
  end

  def fill_ellipse(x, y, radius_x, radius_y, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_ellipse(x, y, radius_x, radius_y, color)
    end
  end

  def draw_arc(x, y, radius0, radius1, angle0, angle1, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_arc(x, y, radius0, radius1, angle0, angle1, color)
    end
  end

  def fill_arc(x, y, radius0, radius1, angle0, angle1, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_arc(x, y, radius0, radius1, angle0, angle1, color)
    end
  end

  def draw_triangle(x0, y0, x1, y1, x2, y2, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_triangle(x0, y0, x1, y1, x2, y2, color)
    end
  end

  def fill_triangle(x0, y0, x1, y1, x2, y2, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_triangle(x0, y0, x1, y1, x2, y2, color)
    end
  end

  def set_cursor(x, y), do: Native.set_cursor(x, y)
  def set_text_size(scale), do: Native.set_text_size(scale)

  def set_text_datum(datum) do
    with {:ok, datum} <- Command.normalize_text_datum(datum) do
      Native.set_text_datum(datum)
    end
  end

  def set_text_color(foreground) do
    with {:ok, foreground} <- Color.normalize_display(foreground) do
      Native.set_text_color(foreground)
    end
  end

  def set_text_color(foreground, background) do
    with {:ok, foreground} <- Color.normalize_display(foreground),
         {:ok, background} <- Color.normalize_display(background) do
      Native.set_text_color(foreground, background)
    end
  end

  def draw_string(text, x, y), do: Native.draw_string(text, x, y)
  def print(text), do: Native.print(text)
  def println(text), do: Native.println(text)
  def println, do: Native.println("")

  def push_image(x, y, width, height, pixels) do
    Native.push_image(x, y, width, height, pixels)
  end

  @doc """
  LovyanGFX 形式の描画命令一覧を1回の NIF 呼び出しで実行します。

  既存の描画命令形式を使用します。`opts` には `:target` と `:display` を指定できます。
  """
  def batch(commands, opts \\ []) do
    with {:ok, command_binary} <- RenderBatch.encode(commands, opts) do
      Native.batch(0, command_binary)
    end
  end
end
