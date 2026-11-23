# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.LGFX do
  @moduledoc """
  画面に文字・図形・画像を描くための、ハンドル不要の API です。

  LovyanGFX の既存例を Elixir へ置き換えやすいよう、よく使う関数名を
  `snake_case` でほぼそのまま提供します。

      :ok = AtomLGFX.LGFX.init()
      :ok = AtomLGFX.LGFX.fill_screen(:black)
      :ok = AtomLGFX.LGFX.draw_rect(20, 20, 100, 60, :red)
      :ok = AtomLGFX.LGFX.fill_circle(160, 120, 30, :blue)
      :ok = AtomLGFX.LGFX.draw_string("Hello", 20, 100)

  多数の描画命令をまとめる場合は `batch/2` を使用します。

      :ok =
        AtomLGFX.LGFX.batch([
          {:fill_screen, :black},
          {:fill_rect, 10, 10, 100, 40, :red},
          {:draw_string, "Hello", 20, 20}
        ])

  同じ命令列を繰り返す場合は、1回だけ符号化してから送信します。

      {:ok, batch} =
        AtomLGFX.LGFX.encode_batch([
          {:fill_screen, :black},
          {:fill_rect, 10, 10, 100, 40, :red}
        ])

      :ok = AtomLGFX.LGFX.submit_batch(batch)

  `init/0` は構築時の機器設定を使用します。実行時にパネルやバス設定を
  上書きする場合は `init/1` に設定を渡します。

  スプライト、タッチ、JPEG などにはハンドル付きの `AtomLGFX` を使います。
  両 API は単一のネイティブ LCD を共有します。
  描画は通常 `:ok`、寸法は整数、入力エラーなどは `{:error, reason}` を返します。
  NIF の引数検査では例外になる場合もあります。PC の BEAM では NIF 呼び出しが `:nif_not_loaded` を返します。
  """

  alias AtomLGFX.Color
  alias AtomLGFX.Command
  alias AtomLGFX.Native
  alias AtomLGFX.OpenConfig
  alias AtomLGFX.RenderBatch

  @doc "構築時の既定設定で LCD を初期化します。"
  def init, do: Native.init()

  @doc "パネルや GPIO などの実行時設定を指定して LCD を初期化します。"
  def init(options) when is_list(options) do
    with {:ok, config} <- OpenConfig.normalize_open_config(options) do
      Native.init(config)
    end
  end

  def init(options), do: {:error, {:bad_open_options_shape, options}}

  @doc "LCD とスプライトを終了します。"
  def close, do: Native.close()

  @doc "現在の回転での画面の幅を返します。"
  def width, do: Native.width()

  @doc "現在の回転での画面の高さを返します。"
  def height, do: Native.height()

  @doc "複数の描画操作で使う書き込み区間を開始します。"
  def start_write, do: Native.start_write()

  @doc "書き込み区間を終了します。"
  def end_write, do: Native.end_write()

  @doc "描画を表示へ反映します。"
  def display, do: Native.display()

  @doc "画面の回転を設定します。"
  def set_rotation(rotation), do: Native.set_rotation(rotation)

  @doc "対応する機器の明るさを設定します。"
  def set_brightness(brightness), do: Native.set_brightness(brightness)

  @doc "画面を指定した色で塗りつぶします。"
  def fill_screen(color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_screen(color)
    end
  end

  @doc "指定した色で画面を消去します。"
  def clear(color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.clear(color)
    end
  end

  @doc "1画素を描きます。"
  def draw_pixel(x, y, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_pixel(x, y, color)
    end
  end

  @doc "垂直線を描きます。"
  def draw_fast_vline(x, y, height, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_fast_vline(x, y, height, color)
    end
  end

  @doc "水平線を描きます。"
  def draw_fast_hline(x, y, width, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_fast_hline(x, y, width, color)
    end
  end

  @doc "2点を結ぶ線を描きます。"
  def draw_line(x0, y0, x1, y1, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_line(x0, y0, x1, y1, color)
    end
  end

  @doc "長方形の輪郭を描きます。"
  def draw_rect(x, y, width, height, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_rect(x, y, width, height, color)
    end
  end

  @doc "長方形を塗りつぶします。"
  def fill_rect(x, y, width, height, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_rect(x, y, width, height, color)
    end
  end

  @doc "角丸長方形の輪郭を描きます。"
  def draw_round_rect(x, y, width, height, radius, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_round_rect(x, y, width, height, radius, color)
    end
  end

  @doc "角丸長方形を塗りつぶします。"
  def fill_round_rect(x, y, width, height, radius, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_round_rect(x, y, width, height, radius, color)
    end
  end

  @doc "円の輪郭を描きます。"
  def draw_circle(x, y, radius, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_circle(x, y, radius, color)
    end
  end

  @doc "円を塗りつぶします。"
  def fill_circle(x, y, radius, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_circle(x, y, radius, color)
    end
  end

  @doc "楕円の輪郭を描きます。"
  def draw_ellipse(x, y, radius_x, radius_y, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_ellipse(x, y, radius_x, radius_y, color)
    end
  end

  @doc "楕円を塗りつぶします。"
  def fill_ellipse(x, y, radius_x, radius_y, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_ellipse(x, y, radius_x, radius_y, color)
    end
  end

  @doc "円弧を描きます。"
  def draw_arc(x, y, radius0, radius1, angle0, angle1, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_arc(x, y, radius0, radius1, angle0, angle1, color)
    end
  end

  @doc "円弧を塗りつぶします。"
  def fill_arc(x, y, radius0, radius1, angle0, angle1, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_arc(x, y, radius0, radius1, angle0, angle1, color)
    end
  end

  @doc "三角形の輪郭を描きます。"
  def draw_triangle(x0, y0, x1, y1, x2, y2, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.draw_triangle(x0, y0, x1, y1, x2, y2, color)
    end
  end

  @doc "三角形を塗りつぶします。"
  def fill_triangle(x0, y0, x1, y1, x2, y2, color) do
    with {:ok, color} <- Color.normalize_display(color) do
      Native.fill_triangle(x0, y0, x1, y1, x2, y2, color)
    end
  end

  @doc "print / println の描画位置を設定します。"
  def set_cursor(x, y), do: Native.set_cursor(x, y)

  @doc "文字の倍率を設定します。"
  def set_text_size(scale), do: Native.set_text_size(scale)

  @doc "文字配置の基準位置を設定します。"
  def set_text_datum(datum) do
    with {:ok, datum} <- Command.normalize_text_datum(datum) do
      Native.set_text_datum(datum)
    end
  end

  @doc "文字色を設定します。"
  def set_text_color(foreground) do
    with {:ok, foreground} <- Color.normalize_display(foreground) do
      Native.set_text_color(foreground)
    end
  end

  @doc "文字色と背景色を設定します。"
  def set_text_color(foreground, background) do
    with {:ok, foreground} <- Color.normalize_display(foreground),
         {:ok, background} <- Color.normalize_display(background) do
      Native.set_text_color(foreground, background)
    end
  end

  @doc "指定位置に文字列を描きます。"
  def draw_string(text, x, y), do: Native.draw_string(text, x, y)

  @doc "カーソル位置に文字列を描きます。"
  def print(text), do: Native.print(text)

  @doc "カーソル位置に文字列を描いて改行します。"
  def println(text), do: Native.println(text)

  @doc "改行します。"
  def println, do: Native.println("")

  @doc "RGB565 の画像データを転送します。"
  def push_image(x, y, width, height, pixels) do
    Native.push_image(x, y, width, height, pixels)
  end

  @doc """
  LovyanGFX 形式の描画命令一覧を1回の NIF 呼び出しで実行します。

  `opts` の `:target` は `:lcd` / `0`（既定）またはスプライト ID、
  `:display` は最後の表示反映の有無（既定 `false`）です。
  """
  def batch(commands, opts \\ []) do
    with {:ok, command_binary} <- encode_batch(commands, opts) do
      submit_batch(command_binary)
    end
  end

  @doc """
  描画命令一覧を符号化し、`{:ok, binary}` を返します。

  命令と設定は `batch/2` と同じです。NIF を呼ばず、PC でも使えます。
  """
  def encode_batch(commands, opts \\ []), do: RenderBatch.encode(commands, opts)

  @doc """
  `encode_batch/2` で準備したバイナリーバッチを1回の NIF 呼び出しで実行します。
  """
  def submit_batch(command_binary) when is_binary(command_binary) do
    Native.batch(0, command_binary)
  end

  def submit_batch(command_binary), do: {:error, {:bad_render_batch, command_binary}}
end
