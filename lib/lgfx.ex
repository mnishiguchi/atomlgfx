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

  defdelegate init(), to: Native
  defdelegate close(), to: Native

  defdelegate width(), to: Native
  defdelegate height(), to: Native

  defdelegate start_write(), to: Native
  defdelegate end_write(), to: Native
  defdelegate display(), to: Native

  defdelegate set_rotation(rotation), to: Native
  defdelegate set_brightness(brightness), to: Native

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

  defdelegate set_cursor(x, y), to: Native
  defdelegate set_text_size(scale), to: Native

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

  defdelegate draw_string(text, x, y), to: Native
  defdelegate print(text), to: Native
  defdelegate println(text), to: Native

  def println, do: Native.println("")

  defdelegate push_image(x, y, width, height, pixels), to: Native

  @doc """
  LovyanGFX 形式の描画命令一覧を1回の NIF 呼び出しで実行します。

  `AtomLGFX.RenderBatch` と同じ命令形式を使用します。`opts` には既存の
  `:target` と `:display` を指定できます。
  """
  def batch(commands, opts \\ []) do
    with {:ok, command_binary} <- RenderBatch.encode(commands, opts) do
      Native.batch(0, command_binary)
    end
  end
end
