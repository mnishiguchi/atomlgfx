# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX.Simple do
  @moduledoc """
  画面に文字・図形・画像を描くための、ハンドル不要の API です。
  最初の LCD 描画にはこのモジュールを使ってください。

      alias AtomLGFX.Simple

      :ok = Simple.init()
      :ok = Simple.fill_screen(:black)
      :ok = Simple.set_text_color(:white)
      :ok = Simple.draw_string("Hello", 16, 16)
      :ok = Simple.display()

  `init/0` はファームウェアの既定設定を使います。配線やパネルを変更する場合は
  `init/1` に設定を渡してください。実行には AtomLGFX NIF を含む AtomVM が必要です。

  全関数は互換 API の `LGFX` へ委譲します。引数・戻り値・入力エラーは同じで、
  新しい NIF や LCD 状態は作りません。描画は通常 `:ok`、寸法は整数を返します。
  入力エラーなどは `{:error, reason}` を返し、NIF の引数検査では例外になる場合もあります。
  PC の BEAM で NIF へ到達した場合は `:nif_not_loaded` です。

  複数の描画は `batch/2`、同じ命令列の再利用は `encode_batch/2` と `submit_batch/1` を使います。
  色は `:red` などの名前付き色や RGB565 値を受け付けます。

  スプライト、タッチ、JPEG などにはハンドル付きの `AtomLGFX` を使います。
  ハンドルは作成したプロセスの設定・キャッシュを識別する値で、別の物理 LCD ではありません。
  どちらも同じ単一のネイティブ LCD を使うため、独立した機器として同時に初期化しないでください。
  内部の AtomLGFX.Native は NIF 境界です。
  """

  @doc "構築時の既定設定で LCD を初期化します。"
  defdelegate init(), to: LGFX

  @doc "パネルや GPIO などの実行時設定を指定して LCD を初期化します。"
  defdelegate init(options), to: LGFX

  @doc "LCD とスプライトを終了します。"
  defdelegate close(), to: LGFX

  @doc "現在の回転での画面の幅を返します。"
  defdelegate width(), to: LGFX

  @doc "現在の回転での画面の高さを返します。"
  defdelegate height(), to: LGFX

  @doc "複数の描画操作で使う書き込み区間を開始します。"
  defdelegate start_write(), to: LGFX

  @doc "書き込み区間を終了します。"
  defdelegate end_write(), to: LGFX

  @doc "描画を表示へ反映します。"
  defdelegate display(), to: LGFX

  @doc "画面の回転を設定します。"
  defdelegate set_rotation(rotation), to: LGFX

  @doc "対応する機器の明るさを設定します。"
  defdelegate set_brightness(brightness), to: LGFX

  @doc "画面を指定した色で塗りつぶします。"
  defdelegate fill_screen(color), to: LGFX

  @doc "指定した色で画面を消去します。"
  defdelegate clear(color), to: LGFX

  @doc "1画素を描きます。"
  defdelegate draw_pixel(x, y, color), to: LGFX

  @doc "垂直線を描きます。"
  defdelegate draw_fast_vline(x, y, height, color), to: LGFX

  @doc "水平線を描きます。"
  defdelegate draw_fast_hline(x, y, width, color), to: LGFX

  @doc "2点を結ぶ線を描きます。"
  defdelegate draw_line(x0, y0, x1, y1, color), to: LGFX

  @doc "長方形の輪郭を描きます。"
  defdelegate draw_rect(x, y, width, height, color), to: LGFX

  @doc "長方形を塗りつぶします。"
  defdelegate fill_rect(x, y, width, height, color), to: LGFX

  @doc "角丸長方形の輪郭を描きます。"
  defdelegate draw_round_rect(x, y, width, height, radius, color), to: LGFX

  @doc "角丸長方形を塗りつぶします。"
  defdelegate fill_round_rect(x, y, width, height, radius, color), to: LGFX

  @doc "円の輪郭を描きます。"
  defdelegate draw_circle(x, y, radius, color), to: LGFX

  @doc "円を塗りつぶします。"
  defdelegate fill_circle(x, y, radius, color), to: LGFX

  @doc "楕円の輪郭を描きます。"
  defdelegate draw_ellipse(x, y, radius_x, radius_y, color), to: LGFX

  @doc "楕円を塗りつぶします。"
  defdelegate fill_ellipse(x, y, radius_x, radius_y, color), to: LGFX

  @doc "円弧を描きます。"
  defdelegate draw_arc(x, y, radius0, radius1, angle0, angle1, color), to: LGFX

  @doc "円弧を塗りつぶします。"
  defdelegate fill_arc(x, y, radius0, radius1, angle0, angle1, color), to: LGFX

  @doc "三角形の輪郭を描きます。"
  defdelegate draw_triangle(x0, y0, x1, y1, x2, y2, color), to: LGFX

  @doc "三角形を塗りつぶします。"
  defdelegate fill_triangle(x0, y0, x1, y1, x2, y2, color), to: LGFX

  @doc "print / println の描画位置を設定します。"
  defdelegate set_cursor(x, y), to: LGFX

  @doc "文字の倍率を設定します。"
  defdelegate set_text_size(scale), to: LGFX

  @doc "文字配置の基準位置を設定します。"
  defdelegate set_text_datum(datum), to: LGFX

  @doc "文字色を設定します。"
  defdelegate set_text_color(foreground), to: LGFX

  @doc "文字色と背景色を設定します。"
  defdelegate set_text_color(foreground, background), to: LGFX

  @doc "指定位置に文字列を描きます。"
  defdelegate draw_string(text, x, y), to: LGFX

  @doc "カーソル位置に文字列を描きます。"
  defdelegate print(text), to: LGFX

  @doc "カーソル位置に文字列を描いて改行します。"
  defdelegate println(text), to: LGFX

  @doc "改行します。"
  defdelegate println(), to: LGFX

  @doc "RGB565 の画像データを転送します。"
  defdelegate push_image(x, y, width, height, pixels), to: LGFX

  @doc """
  描画命令を符号化し、1回の NIF 呼び出しで実行します。

      :ok = Simple.batch([{:fill_screen, :black}, {:draw_string, "Hello", 16, 16}], display: true)

  `opts` の `:target` は `:lcd` / `0`（既定）またはスプライト ID、
  `:display` は最後の表示反映の有無（既定 `false`）です。詳細は `LGFX.batch/2` と同じです。
  """
  defdelegate batch(commands, opts \\ []), to: LGFX

  @doc """
  繰り返し送信する描画命令をバイナリーへ符号化し、`{:ok, binary}` を返します。

  命令と設定は `batch/2` と同じです。この関数は NIF を呼ばず、PC でも使えます。
  """
  defdelegate encode_batch(commands, opts \\ []), to: LGFX

  @doc "encode_batch/2 で準備したバイナリーを送信します。"
  defdelegate submit_batch(command_binary), to: LGFX
end
