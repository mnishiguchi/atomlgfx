# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.BinaryBatch do
  @moduledoc """
  圧縮バイナリーバッチ命令を構築する互換窓口です。

  `AtomLGFX.BinaryBatch` は、通信形式を細かく制御する必要がある利用者向けの公開低水準 API として維持します。実装は小さな内部モジュールへ分割し、命令の符号化、復号、送信、検証、診断を内部で簡潔に保ちながら、公開モジュールを安定させます。
  """

  alias AtomLGFX.BinaryBatch.Codec
  alias AtomLGFX.BinaryBatch.Diagnostics
  alias AtomLGFX.BinaryBatch.Submission
  alias AtomLGFX.BinaryBatch.Validation

  @doc false
  @spec __render_private_opcodes__() :: [{atom(), byte()}]
  defdelegate __render_private_opcodes__(), to: Codec

  @doc false
  @spec __render_extended_opcodes__() :: [{atom(), byte()}]
  defdelegate __render_extended_opcodes__(), to: Codec

  @doc false
  @spec __known_batch_opcodes__() :: [byte()]
  defdelegate __known_batch_opcodes__(), to: Codec

  @doc """
  圧縮バイナリー命令の断片を1つの命令列へまとめます。
  """
  @spec batch(iodata()) :: binary()
  defdelegate batch(commands), to: Codec

  @doc """
  バイナリーバッチ命令列を送信します。
  """
  @spec render(reference(), iodata()) :: :ok | {:error, term()}
  defdelegate render(handle, commands), to: Submission

  @doc """
  バイナリーバッチ命令列を検証してから送信します。
  """
  @spec render_checked(reference(), iodata()) :: :ok | {:error, term()}
  defdelegate render_checked(handle, commands), to: Submission

  @doc """
  バイナリーバッチ命令列を送信せずに検証します。
  """
  @spec validate(iodata()) :: :ok | {:error, term()}
  defdelegate validate(commands), to: Validation

  @doc """
  バイナリーバッチ命令列を検証し、不正な場合は `ArgumentError` を送出します。
  """
  @spec validate!(iodata()) :: :ok
  defdelegate validate!(commands), to: Validation

  @doc """
  後続のバイナリーバッチ命令で使用する描画対象を選択します。
  """
  @spec target(integer()) :: binary()
  defdelegate target(target), to: Codec

  @doc """
  後続の色値をどの形式として解釈するかを選択します。
  """
  @spec color_mode(:rgb565 | :palette_index) :: binary()
  defdelegate color_mode(mode), to: Codec

  @doc """
  現在のフレームを表示へ反映します。
  """
  @spec display() :: binary()
  defdelegate display(), to: Codec

  @doc "現在の対象を色で塗りつぶす命令を符号化します。"
  @spec fill_screen(integer()) :: binary()
  defdelegate fill_screen(color), to: Codec

  @doc "現在の対象を色で消去する命令を符号化します。"
  @spec clear(integer()) :: binary()
  defdelegate clear(color), to: Codec

  @doc "現在の対象に1画素を描画する命令を符号化します。"
  @spec draw_pixel(integer(), integer(), integer()) :: binary()
  defdelegate draw_pixel(x, y, color), to: Codec

  @doc "高速な垂直線を描画する命令を符号化します。"
  @spec draw_fast_vline(integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_fast_vline(x, y, height, color), to: Codec

  @doc "高速な水平線を描画する命令を符号化します。"
  @spec draw_fast_hline(integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_fast_hline(x, y, width, color), to: Codec

  @doc "線を描画する命令を符号化します。"
  @spec draw_line(integer(), integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_line(x0, y0, x1, y1, color), to: Codec

  @doc "長方形の輪郭を描画する命令を符号化します。"
  @spec draw_rect(integer(), integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_rect(x, y, width, height, color), to: Codec

  @doc "長方形を塗りつぶす命令を符号化します。"
  @spec fill_rect(integer(), integer(), integer(), integer(), integer()) :: binary()
  defdelegate fill_rect(x, y, width, height, color), to: Codec

  @doc "角丸長方形の輪郭を描画する命令を符号化します。"
  @spec draw_round_rect(integer(), integer(), integer(), integer(), integer(), integer()) ::
          binary()
  defdelegate draw_round_rect(x, y, width, height, radius, color), to: Codec

  @doc "角丸長方形を塗りつぶす命令を符号化します。"
  @spec fill_round_rect(integer(), integer(), integer(), integer(), integer(), integer()) ::
          binary()
  defdelegate fill_round_rect(x, y, width, height, radius, color), to: Codec

  @doc "円の輪郭を描画する命令を符号化します。"
  @spec draw_circle(integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_circle(x, y, radius, color), to: Codec

  @doc "円を塗りつぶす命令を符号化します。"
  @spec fill_circle(integer(), integer(), integer(), integer()) :: binary()
  defdelegate fill_circle(x, y, radius, color), to: Codec

  @doc "楕円の輪郭を描画する命令を符号化します。"
  @spec draw_ellipse(integer(), integer(), integer(), integer(), integer()) :: binary()
  defdelegate draw_ellipse(x, y, radius_x, radius_y, color), to: Codec

  @doc "楕円を塗りつぶす命令を符号化します。"
  @spec fill_ellipse(integer(), integer(), integer(), integer(), integer()) :: binary()
  defdelegate fill_ellipse(x, y, radius_x, radius_y, color), to: Codec

  @doc "円弧の輪郭を描画する命令を符号化します。"
  @spec draw_arc(integer(), integer(), integer(), integer(), number(), number(), integer()) ::
          binary()
  defdelegate draw_arc(x, y, radius0, radius1, angle0, angle1, color), to: Codec

  @doc "円弧を塗りつぶす命令を符号化します。"
  @spec fill_arc(integer(), integer(), integer(), integer(), number(), number(), integer()) ::
          binary()
  defdelegate fill_arc(x, y, radius0, radius1, angle0, angle1, color), to: Codec

  @doc "2次ベジェ曲線を描画する命令を符号化します。"
  @spec draw_bezier(
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer()
        ) :: binary()
  defdelegate draw_bezier(x0, y0, x1, y1, x2, y2, color), to: Codec

  @doc "3次ベジェ曲線を描画する命令を符号化します。"
  @spec draw_bezier(
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer(),
          integer()
        ) :: binary()
  defdelegate draw_bezier(x0, y0, x1, y1, x2, y2, x3, y3, color), to: Codec

  @doc "三角形の輪郭を描画する命令を符号化します。"
  @spec draw_triangle(integer(), integer(), integer(), integer(), integer(), integer(), integer()) ::
          binary()
  defdelegate draw_triangle(x0, y0, x1, y1, x2, y2, color), to: Codec

  @doc "三角形を塗りつぶす命令を符号化します。"
  @spec fill_triangle(integer(), integer(), integer(), integer(), integer(), integer(), integer()) ::
          binary()
  defdelegate fill_triangle(x0, y0, x1, y1, x2, y2, color), to: Codec

  @doc """
  現在の描画対象に切り抜き長方形を設定します。
  """
  @spec set_clip_rect(integer(), integer(), integer(), integer()) :: binary()
  defdelegate set_clip_rect(x, y, width, height), to: Codec

  @doc """
  現在の描画対象の切り抜き長方形を解除します。
  """
  @spec clear_clip_rect() :: binary()
  defdelegate clear_clip_rect(), to: Codec

  @doc """
  現在の描画対象に文字書体のプリセットを選択します。
  """
  @spec set_text_font_preset(:ascii | :jp) :: binary()
  defdelegate set_text_font_preset(preset), to: Codec

  @doc """
  現在の描画対象の文字倍率を設定します。
  """
  @spec set_text_size(number()) :: binary()
  defdelegate set_text_size(scale), to: Codec

  @doc """
  現在の描画対象の文字倍率を X 軸と Y 軸で個別に設定します。
  """
  @spec set_text_size_xy(number(), number()) :: binary()
  defdelegate set_text_size_xy(scale_x, scale_y), to: Codec

  @doc """
  現在の描画対象の文字基準位置を設定します。
  """
  @spec set_text_datum(non_neg_integer()) :: binary()
  defdelegate set_text_datum(datum), to: Codec

  @doc """
  現在の描画対象の文字折り返しを設定します。
  """
  @spec set_text_wrap(boolean()) :: binary()
  defdelegate set_text_wrap(wrap), to: Codec

  @doc """
  現在の描画対象の文字折り返しを X 軸と Y 軸で個別に設定します。
  """
  @spec set_text_wrap_xy(boolean(), boolean()) :: binary()
  defdelegate set_text_wrap_xy(wrap_x, wrap_y), to: Codec

  @doc """
  現在の描画対象の文字カーソル位置を設定します。
  """
  @spec set_cursor(integer(), integer()) :: binary()
  defdelegate set_cursor(x, y), to: Codec

  @doc """
  文字の前景色と、任意の背景色を設定します。
  """
  @spec set_text_color(integer() | {:rgb565, integer()} | {:index, integer()}) :: binary()
  defdelegate set_text_color(fg_color), to: Codec

  @doc "現在の対象に文字の前景色と任意の背景色を設定します。"
  @spec set_text_color(
          integer() | {:rgb565, integer()} | {:index, integer()},
          nil | integer() | {:rgb565, integer()} | {:index, integer()}
        ) :: binary()
  defdelegate set_text_color(fg_color, bg_color), to: Codec

  @doc """
  現在の描画対象の絶対位置へ文字列を描画します。
  """
  @spec draw_string(integer(), integer(), binary()) :: binary()
  defdelegate draw_string(x, y, text), to: Codec

  @doc """
  現在のカーソル位置へ文字列を出力します。
  """
  @spec print(binary()) :: binary()
  defdelegate print(text), to: Codec

  @doc """
  現在のカーソル位置へ文字列と改行を出力します。
  """
  @spec println() :: binary()
  defdelegate println(), to: Codec

  @doc "現在のカーソル位置へ文字列と改行を出力します。"
  @spec println(binary()) :: binary()
  defdelegate println(text), to: Codec

  @doc """
  現在の配色表式スプライト対象に配色表の1項目を設定します。
  """
  @spec set_palette_color(integer(), integer()) :: binary()
  defdelegate set_palette_color(palette_index, rgb888), to: Codec

  @doc """
  現在の描画対象の基準点を設定します。
  """
  @spec set_pivot(integer(), integer()) :: binary()
  defdelegate set_pivot(x, y), to: Codec

  @doc """
  スプライト対象を現在の描画対象へ転送します。
  """
  @spec push_sprite(integer(), integer(), integer()) :: binary()
  defdelegate push_sprite(source_target, x, y), to: Codec

  @doc """
  透明色を使用してスプライト対象を現在の描画対象へ転送します。
  """
  @spec push_sprite(
          integer(),
          integer(),
          integer(),
          integer() | {:rgb565, integer()} | {:index, integer()}
        ) ::
          binary()
  defdelegate push_sprite(source_target, x, y, transparent), to: Codec

  @doc """
  回転・倍率の選択肢を使用してスプライト対象を転送します。
  """
  @spec push_rotate_zoom(integer(), integer(), integer(), number(), number()) :: binary()
  defdelegate push_rotate_zoom(source_target, x, y, angle_deg, zoom), to: Codec

  @doc "X 軸と Y 軸の倍率を個別に指定してスプライト対象を転送します。"
  @spec push_rotate_zoom(integer(), integer(), integer(), number(), number(), number()) ::
          binary()
  defdelegate push_rotate_zoom(source_target, x, y, angle_deg, zoom_x, zoom_y), to: Codec

  @doc "回転・倍率値と透明色を使用してスプライト対象を転送します。"
  @spec push_rotate_zoom(
          integer(),
          integer(),
          integer(),
          number(),
          number(),
          number(),
          integer() | {:rgb565, integer()} | {:index, integer()}
        ) :: binary()
  defdelegate push_rotate_zoom(source_target, x, y, angle_deg, zoom_x, zoom_y, transparent),
    to: Codec

  @doc """
  1つの圧縮回転・倍率命令で多数のスプライトを転送します。
  """
  @spec push_rotate_zoom_list(list()) :: binary()
  defdelegate push_rotate_zoom_list(instances), to: Codec

  @doc "1つの圧縮回転・倍率命令と選択肢で多数のスプライトを転送します。"
  @spec push_rotate_zoom_list(list(), keyword()) :: binary()
  defdelegate push_rotate_zoom_list(instances, opts), to: Codec

  @doc """
  ネイティブドライバーを呼び出さずに、バイナリーバッチ命令列を復号します。
  """
  @spec decode(iodata()) :: {:ok, [map()]} | {:error, term()}
  defdelegate decode(commands), to: Codec

  @doc """
  バイナリーバッチ命令列を復号し、不正な場合は `ArgumentError` を送出します。
  """
  @spec decode!(iodata()) :: [map()]
  defdelegate decode!(commands), to: Codec

  @doc """
  バイナリーバッチの診断概要を返します。
  """
  @spec summary(iodata()) :: {:ok, map()} | {:error, term()}
  defdelegate summary(commands), to: Diagnostics

  @doc """
  バイナリーバッチの診断概要を返し、不正な場合は `ArgumentError` を送出します。
  """
  @spec summary!(iodata()) :: map()
  defdelegate summary!(commands), to: Diagnostics

  @doc """
  バイナリーバッチ命令列の構造化された診断報告を返します。
  """
  @spec diagnose(iodata()) :: {:ok, map()} | {:error, map()}
  defdelegate diagnose(commands), to: Diagnostics

  @doc """
  正常なバイナリーバッチ診断報告を返し、不正な場合は `ArgumentError` を送出します。
  """
  @spec diagnose!(iodata()) :: map()
  defdelegate diagnose!(commands), to: Diagnostics

  @doc """
  `summary/1` の測定値を使用して2つのバイナリーバッチ命令列を比較します。
  """
  @spec compare(iodata(), iodata()) ::
          {:ok, map()} | {:error, {:baseline, term()}} | {:error, {:candidate, term()}}
  defdelegate compare(baseline_commands, candidate_commands), to: Diagnostics

  @doc """
  2つのバイナリーバッチ命令列を比較し、不正な場合は `ArgumentError` を送出します。
  """
  @spec compare!(iodata(), iodata()) :: map()
  defdelegate compare!(baseline_commands, candidate_commands), to: Diagnostics

  @doc """
  呼び出し側が指定した診断上限に対して、バイナリーバッチ命令列を検査します。
  """
  @spec check_budget(iodata(), map() | keyword()) ::
          {:ok, map()} | {:error, term()} | {:error, {:budget_exceeded, map()}}
  defdelegate check_budget(commands, limits), to: Diagnostics

  @doc """
  診断上限に対してバイナリーバッチ命令列を検査し、不正な場合は `ArgumentError` を送出します。
  """
  @spec check_budget!(iodata(), map() | keyword()) :: map()
  defdelegate check_budget!(commands, limits), to: Diagnostics
end
