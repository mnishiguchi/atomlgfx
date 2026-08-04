# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

defmodule AtomLGFX do
  @moduledoc """
  `lgfx_port` AtomVM ポートドライバーを扱う Elixir 側の窓口です。

  公開 API は、小さく分かりやすい LovyanGFX 形式のラッパーとして設計されています。

  - 文字倍率には `1`、`2`、`1.5` などの自然な値を使用します
  - 回転角には度数をそのまま使用します
  - 拡大縮小率には `1.0`、`2.0`、`0.5` などの自然な値を使用します

  回転と倍率の API は、LovyanGFX と同じ数値の意味をそのまま使用します。

  基本的な起動例:

      {:ok, port} =
        AtomLGFX.open(
          panel_driver: :ili9341_2,
          width: 240,
          height: 320,
          offset_rotation: 4,
          invert: true,
          rgb_order: false,
          lcd_spi_host: :spi2_host,
          touch_cs_gpio: 44
        )

      :ok = AtomLGFX.ping(port)
      :ok = AtomLGFX.init(port)
      :ok = AtomLGFX.display(port)

  ライフサイクル:

  - ネイティブドライバーは単一機器モデルです
  - `open/1` はポートごとに開始時設定を保存します
  - `init/1` は同じポートに保存された設定を適用します
  - 実行中のネイティブ機器を所有できるポートは一度に1つだけです
  - `close/1` は機器を完全に終了し、このモジュールの実行時キャッシュを消去します
  - `close/1` は BEAM のポートハンドルを閉じず、保存した開始時設定も削除しません
  - `close/1` は終了時に、所有ポートに残ったネイティブ書き込みの入れ子を強制的に解消します

  データと応答:

  - 省略した開始時設定には構築時の既定値を使用します
  - `get_touch/1` と `get_touch_raw/1` は `{:ok, :none}` または `{:ok, {x, y, size}}` を返します
  - 基本図形と文字の表示色には、RGB565 整数または `{:index, 3}` 形式の配色表番号を使用できます
  - スプライト転送の透明色には、RGB565 `u16` 値または `{:index, 0}` 形式の配色表番号を使用できます
  - `AtomLGFX.Color` は RGB565 表示色、RGB888 配色表色、配色表番号を扱う補助関数群です
  - `set_text_datum/3` は `0..255` の値を固定したネイティブドライバーへそのまま渡します
  - `set_text_size/3` と `set_text_size_xy/4` は LovyanGFX 形式の倍率値を直接受け取ります
  - `set_text_wrap/3` は X 軸だけを指定し、Y 軸の折り返しを無効にします。両軸を指定する場合は `set_text_wrap_xy/4` を使用します
  - `set_cursor/4` と `get_cursor/2` は LCD 対象 `0` またはスプライト対象 `1..254` を扱います
  - `print/3` と `println/3` は対象が保持する現在のカーソル位置を使用します
  - `draw_jpg/5` は JPEG バイナリーを選択対象の `{x, y}` へ描画します
  - `draw_jpg/11` は LovyanGFX 形式の倍率値を直接受け取ります
  - LCD とスプライトの切り抜き状態は個別に保持されます
  - `create_palette/2` と `set_palette_color/4` は配色表式スプライトの配色表を管理します
  - `set_text_font_preset/3` は `:ascii` と `:jp` の安定した書体選択を提供します
  - 書体の選択と文字倍率は独立しており、倍率は `set_text_size/3` または `set_text_size_xy/4` で指定します
  - `push_rotate_zoom_to/7`、`/8`、`/9` は角度と倍率を直接受け取ります
  - `push_rotate_zoom_list_to/4` は、多数の変形スプライトを扱う固定小数点形式の高速経路です
  - `push_image_rgb565/8` は、リトルエンディアンの16ビット語で符号化した通常の RGB565 画素データを受け取ります
  - `set_swap_bytes/3` は、生画像転送時の対象別バイト入れ替えを制御します
  """

  alias AtomLGFX.Cache
  alias AtomLGFX.Color
  alias AtomLGFX.Command
  alias AtomLGFX.Clip
  alias AtomLGFX.Device
  alias AtomLGFX.Errors
  alias AtomLGFX.OpSchema
  alias AtomLGFX.Images
  alias AtomLGFX.OpenConfig
  alias AtomLGFX.Primitives
  alias AtomLGFX.Protocol
  alias AtomLGFX.RenderBatch
  alias AtomLGFX.Sprites
  alias AtomLGFX.Text
  alias AtomLGFX.Touch

  @port_name "lgfx_port"

  @typedoc "ネイティブ `lgfx_port` ドライバーを開いた AtomVM ポートハンドルです。"
  @type port_handle :: port()

  @typedoc "LCD 対象 `0`、または `1..254` のスプライトハンドルです。"
  @type target :: 0..254

  @typedoc "RGB565 表示色、または配色表番号の記述子です。"
  @type display_color :: Color.rgb565_value() | Color.index_descriptor()

  @doc """
  任意の開始時設定を指定して `lgfx_port` ドライバーを開きます。
  """
  def open(options \\ [])

  def open(options) when is_list(options) do
    open_with(options, &:erlang.open_port/2)
  end

  def open(other) do
    raise ArgumentError,
          "AtomLGFX.open/1 expects a keyword list or proplist, got: #{inspect(other)}"
  end

  @doc false
  def open_with(options, open_port_fun)
      when is_list(options) and is_function(open_port_fun, 2) do
    normalized_open_config = OpenConfig.normalize_open_options!(options)
    port = open_port_fun.({:spawn_driver, @port_name}, normalized_open_config)
    Cache.remember_open_config(port, normalized_open_config)
    {:ok, port}
  end

  @doc """
  ドライバーを開かずに開始時設定を正規化します。
  """
  def normalize_open_config(options), do: OpenConfig.normalize_open_config(options)

  @doc """
  低水準の v3 呼び出し要求をドライバーへ送信します。
  """
  def raw_call(port, op, target, flags, args, timeout \\ Protocol.short_timeout())
      when is_integer(target) and
             is_integer(flags) and flags >= 0 and
             is_list(args) do
    Protocol.raw_call(port, op, target, flags, args, timeout)
  end

  @doc """
  v3 プロトコルを通じて、公開対象として選定された LovyanGFX 操作を呼び出します。
  """
  def call(port, op_name, args \\ [], opts \\ [])

  def call(port, op_name, args, opts) when is_atom(op_name) and is_list(args) and is_list(opts) do
    with {:ok, canonical_name} <- OpSchema.elixir_name(op_name),
         :ok <- require_public_op(canonical_name) do
      target = Keyword.get(opts, :target, 0)
      flags = Keyword.get(opts, :flags, 0)
      timeout = Keyword.get(opts, :timeout, Protocol.long_timeout())

      Protocol.call(port, canonical_name, target, flags, args, timeout)
    else
      {:error, _reason} = error -> error
    end
  end

  @doc """
  複数対象を扱うバイナリーバッチのフレーム処理を送信します。
  """
  def submit_binary_batch(port, command_binary) when is_binary(command_binary) do
    Protocol.submit_binary_batch(port, command_binary, Protocol.long_timeout())
  end

  @doc """
  既定の描画対象を明示してバイナリーバッチを送信します。
  """
  def submit_binary_batch(port, target, command_binary)
      when is_integer(target) and target >= 0 and target <= 254 and is_binary(command_binary) do
    Protocol.submit_binary_batch(port, target, command_binary, Protocol.long_timeout())
  end

  @doc """
  LovyanGFX 形式の命令一覧をバイナリーバッチ経路で描画します。

  多数の小さな基本図形・文字操作を1回の処理として AtomVM のポート境界を越えられるため、通常の描画ではこの API を推奨します。

      AtomLGFX.render(port, [
        {:fill_screen, :black},
        {:set_text_color, :white},
        {:set_cursor, 10, 10},
        {:println, "こんにちは AtomLGFX"},
        {:draw_line, 0, 40, 200, 40, :red},
        :display
      ])

  選択肢:

  - `:target` - 既定の描画対象。既定値は LCD を示す `0`
  - `:display` - 表示命令がない場合に `:display` を末尾へ追加する
  - `:validate` - ネイティブ側へ送信する前に符号化済みバッチを検証する

  JPEG 描画や生画像転送などデータ量の多い操作は、メモリーの所有関係を明確にするため、現時点では個別 API として維持します。
  """
  @spec render(port(), [Command.command()], keyword()) :: :ok | {:error, term()}
  def render(port, commands, opts \\ [])

  def render(port, commands, opts) when is_list(commands) and is_list(opts) do
    RenderBatch.render(port, commands, opts)
  end

  def render(_port, commands, opts) when is_list(commands) do
    {:error, {:bad_render_options, opts}}
  end

  def render(_port, commands, _opts) do
    {:error, {:bad_render_commands, commands}}
  end

  @doc """
  明示した対象へ命令一覧を描画します。

  対象の指定を呼び出し箇所に限定し、命令一覧内の `{:target, id}` を拒否します。対象 `0` は LCD、`1..254` はスプライトです。意図的に複数対象を扱う場合は `render/3` を直接使用します。
  """
  @spec render_to(port(), :lcd | 0..254, [Command.command()], keyword()) :: :ok | {:error, term()}
  def render_to(port, target, commands, opts \\ [])

  def render_to(port, :lcd, commands, opts) when is_list(commands) and is_list(opts) do
    with :ok <- reject_render_target_overrides(commands) do
      render(port, commands, Keyword.put(opts, :target, :lcd))
    end
  end

  def render_to(port, target, commands, opts)
      when is_integer(target) and target >= 0 and target <= 254 and is_list(commands) and
             is_list(opts) do
    with :ok <- reject_render_target_overrides(commands) do
      render(port, commands, Keyword.put(opts, :target, target))
    end
  end

  def render_to(_port, target, commands, _opts)
      when (target == :lcd or (is_integer(target) and target >= 0 and target <= 254)) and
             not is_list(commands) do
    {:error, {:bad_render_commands, commands}}
  end

  def render_to(_port, target, _commands, opts)
      when target == :lcd or (is_integer(target) and target >= 0 and target <= 254) do
    {:error, {:bad_render_options, opts}}
  end

  def render_to(_port, target, _commands, _opts) do
    {:error, {:bad_render_target, target}}
  end

  @doc """
  LCD 対象へ命令一覧を描画します。
  """
  @spec render_lcd(port(), [Command.command()], keyword()) :: :ok | {:error, term()}
  def render_lcd(port, commands, opts \\ [])

  def render_lcd(port, commands, opts) when is_list(commands) and is_list(opts) do
    render_to(port, 0, commands, opts)
  end

  def render_lcd(_port, commands, opts) when is_list(commands) do
    {:error, {:bad_render_options, opts}}
  end

  def render_lcd(_port, commands, _opts) do
    {:error, {:bad_render_commands, commands}}
  end

  @doc """
  スプライト対象へ命令一覧を描画します。

  この補助関数はスプライトハンドル `1..254` だけを受け付け、命令一覧内の対象切り替えを拒否します。これにより、誤って LCD へ描画する問題を公開 API の境界で検出します。
  """
  @spec render_sprite(port(), 1..254, [Command.command()], keyword()) :: :ok | {:error, term()}
  def render_sprite(port, sprite_target, commands, opts \\ [])

  def render_sprite(port, sprite_target, commands, opts)
      when is_integer(sprite_target) and sprite_target >= 1 and sprite_target <= 254 do
    render_to(port, sprite_target, commands, opts)
  end

  def render_sprite(_port, sprite_target, _commands, _opts) do
    {:error, {:bad_sprite_target, sprite_target}}
  end

  defp reject_render_target_overrides([]), do: :ok

  defp reject_render_target_overrides([{:target, target} | _commands]) do
    {:error, {:render_target_override, target}}
  end

  defp reject_render_target_overrides([_command | commands]) do
    reject_render_target_overrides(commands)
  end

  @doc """
  基本的なプロトコル疎通を確認します。
  """
  def ping(port), do: Protocol.ping(port)

  @doc """
  ドライバーが公開するプロトコル機能を返します。
  """
  def get_caps(port), do: Protocol.get_caps(port)

  @doc """
  このポートに保存された開始時設定を返します。
  """
  def get_open_config(port), do: Cache.get_open_config(port)

  @doc """
  ドライバーが保持する直近のプロトコルエラー情報を返します。
  """
  def get_last_error(port), do: Protocol.get_last_error(port)

  @doc """
  選択した対象の幅を返します。
  """
  def width(port, target \\ 0), do: Protocol.width(port, target)

  @doc """
  選択した対象の高さを返します。
  """
  def height(port, target \\ 0), do: Protocol.height(port, target)

  @doc """
  ドライバーがスプライト操作に対応しているかを返します。
  """
  def supports_sprite?(port), do: Protocol.supports_sprite?(port)

  @doc """
  ドライバーが `pushImage` に対応しているかを返します。
  """
  def supports_pushimage?(port), do: Protocol.supports_pushimage?(port)

  @doc """
  ドライバーが `getLastError` に対応しているかを返します。
  """
  def supports_last_error?(port), do: Protocol.supports_last_error?(port)

  @doc """
  ドライバーがタッチ操作に対応しているかを返します。
  """
  def supports_touch?(port), do: Protocol.supports_touch?(port)

  @doc """
  ドライバーが配色表のライフサイクル操作に対応しているかを返します。
  """
  def supports_palette?(port), do: Protocol.supports_palette?(port)

  @doc """
  ドライバーが複数対象のバイナリーバッチ処理に対応しているかを返します。
  """
  def supports_batch?(port), do: Protocol.supports_batch?(port)

  @doc """
  このドライバー実体が受け付けるバイナリーデータの最大容量を返します。
  """
  def max_binary_bytes(port), do: Protocol.max_binary_bytes(port)

  @doc """
  このポートに保存された開始時設定を使用してネイティブ機器を初期化します。
  """
  def init(port), do: Device.init(port)

  @doc """
  ネイティブドライバーの動作に従って LCD 表示を反映します。
  """
  def display(port), do: Device.display(port)

  @doc """
  LCD の回転方向を設定します。
  """
  def set_rotation(port, rotation), do: Device.set_rotation(port, rotation)

  @doc """
  ドライバーの生の `u8` 値で LCD の明るさを設定します。
  """
  def set_brightness(port, brightness), do: Device.set_brightness(port, brightness)

  @doc """
  選択した対象の色深度を設定します。
  """
  def set_color_depth(port, depth, target \\ 0), do: Device.set_color_depth(port, depth, target)

  @doc """
  選択した対象で LovyanGFX のバイト入れ替えを有効または無効にします。
  """
  def set_swap_bytes(port, enabled, target \\ 0), do: Device.set_swap_bytes(port, enabled, target)

  @doc """
  このポートが所有するネイティブ機器を終了し、実行時キャッシュを消去します。
  """
  def close(port) do
    with :ok <- Device.close(port) do
      Cache.reset_runtime_cache(port)
      :ok
    end
  end

  @doc """
  選択した対象に切り抜き長方形を設定します。
  """
  def set_clip_rect(port, x, y, width, height, target \\ 0) do
    Clip.set_clip_rect(port, x, y, width, height, target)
  end

  @doc """
  選択した対象の有効な切り抜き長方形を解除します。
  """
  def clear_clip_rect(port, target \\ 0), do: Clip.clear_clip_rect(port, target)

  @doc """
  選択した対象を指定した表示色で塗りつぶします。
  """
  def fill_screen(port, color, target \\ 0), do: Primitives.fill_screen(port, color, target)

  @doc """
  選択した対象を指定した表示色で消去します。
  """
  def clear(port, color, target \\ 0), do: Primitives.clear(port, color, target)

  @doc """
  指定した表示色で1画素を描画します。

  LovyanGFX の `drawPixel` に対応します。画素を繰り返し描画する場合や画像を生成する場合は、多数の画素を1回で AtomVM のポート境界へ送れる `render/3`、`render_lcd/3`、`render_sprite/4` を推奨します。
  """
  @spec draw_pixel(port_handle(), integer(), integer(), display_color(), target()) ::
          :ok | {:error, term()}
  def draw_pixel(port, x, y, color, target \\ 0),
    do: Primitives.draw_pixel(port, x, y, color, target)

  @doc """
  指定した色値で高速な垂直線を描画します。
  """
  def draw_fast_vline(port, x, y, height, color, target \\ 0),
    do: Primitives.draw_fast_vline(port, x, y, height, color, target)

  @doc """
  指定した色値で高速な水平線を描画します。
  """
  def draw_fast_hline(port, x, y, width, color, target \\ 0),
    do: Primitives.draw_fast_hline(port, x, y, width, color, target)

  @doc """
  指定した色値で線を描画します。
  """
  def draw_line(port, x0, y0, x1, y1, color, target \\ 0),
    do: Primitives.draw_line(port, x0, y0, x1, y1, color, target)

  @doc """
  指定した色値で長方形の輪郭を描画します。
  """
  def draw_rect(port, x, y, width, height, color, target \\ 0),
    do: Primitives.draw_rect(port, x, y, width, height, color, target)

  @doc """
  指定した色値で長方形を塗りつぶします。
  """
  def fill_rect(port, x, y, width, height, color, target \\ 0),
    do: Primitives.fill_rect(port, x, y, width, height, color, target)

  @doc """
  指定した色値で角丸長方形の輪郭を描画します。
  """
  def draw_round_rect(port, x, y, width, height, radius, color, target \\ 0),
    do: Primitives.draw_round_rect(port, x, y, width, height, radius, color, target)

  @doc """
  指定した色値で角丸長方形を塗りつぶします。
  """
  def fill_round_rect(port, x, y, width, height, radius, color, target \\ 0),
    do: Primitives.fill_round_rect(port, x, y, width, height, radius, color, target)

  @doc """
  指定した色値で円の輪郭を描画します。
  """
  def draw_circle(port, x, y, radius, color, target \\ 0),
    do: Primitives.draw_circle(port, x, y, radius, color, target)

  @doc """
  指定した色値で円を塗りつぶします。
  """
  def fill_circle(port, x, y, radius, color, target \\ 0),
    do: Primitives.fill_circle(port, x, y, radius, color, target)

  @doc """
  指定した色値で楕円の輪郭を描画します。
  """
  def draw_ellipse(port, x, y, radius_x, radius_y, color, target \\ 0),
    do: Primitives.draw_ellipse(port, x, y, radius_x, radius_y, color, target)

  @doc """
  指定した色値で楕円を塗りつぶします。
  """
  def fill_ellipse(port, x, y, radius_x, radius_y, color, target \\ 0),
    do: Primitives.fill_ellipse(port, x, y, radius_x, radius_y, color, target)

  @doc """
  指定した色値で円弧の輪郭を描画します。
  """
  def draw_arc(port, x, y, radius0, radius1, angle0, angle1, color, target \\ 0),
    do: Primitives.draw_arc(port, x, y, radius0, radius1, angle0, angle1, color, target)

  @doc """
  指定した色値で円弧を塗りつぶします。
  """
  def fill_arc(port, x, y, radius0, radius1, angle0, angle1, color, target \\ 0),
    do: Primitives.fill_arc(port, x, y, radius0, radius1, angle0, angle1, color, target)

  @doc """
  指定した色値で2次ベジェ曲線を描画します。
  """
  def draw_bezier(port, x0, y0, x1, y1, x2, y2, color, target \\ 0),
    do: Primitives.draw_bezier(port, x0, y0, x1, y1, x2, y2, color, target)

  @doc """
  指定した色値で3次ベジェ曲線を描画します。
  """
  def draw_bezier(port, x0, y0, x1, y1, x2, y2, x3, y3, color, target \\ 0),
    do: Primitives.draw_bezier(port, x0, y0, x1, y1, x2, y2, x3, y3, color, target)

  @doc """
  指定した色値で三角形の輪郭を描画します。
  """
  def draw_triangle(port, x0, y0, x1, y1, x2, y2, color, target \\ 0),
    do: Primitives.draw_triangle(port, x0, y0, x1, y1, x2, y2, color, target)

  @doc """
  指定した色値で三角形を塗りつぶします。
  """
  def fill_triangle(port, x0, y0, x1, y1, x2, y2, color, target \\ 0),
    do: Primitives.fill_triangle(port, x0, y0, x1, y1, x2, y2, color, target)

  @doc """
  指定したハンドルに、対象の既定色深度を使用するスプライトを作成します。
  """
  def create_sprite(port, width, height, target),
    do: Sprites.create_sprite(port, width, height, target)

  @doc """
  指定したハンドルに、色深度を明示したスプライトを作成します。
  """
  def create_sprite(port, width, height, color_depth, target),
    do: Sprites.create_sprite(port, width, height, color_depth, target)

  @doc """
  指定したハンドルのスプライトを削除します。
  """
  def delete_sprite(port, target), do: Sprites.delete_sprite(port, target)

  @doc """
  既存の配色表式スプライト対象に配色表を作成します。
  """
  def create_palette(port, target), do: Sprites.create_palette(port, target)

  @doc """
  配色表を持つスプライト対象に RGB888 で1項目を設定します。
  """
  def set_palette_color(port, target, palette_index, rgb888),
    do: Sprites.set_palette_color(port, target, palette_index, rgb888)

  @doc """
  選択した対象の基準点を設定します。
  """
  def set_pivot(port, target, x, y), do: Sprites.set_pivot(port, target, x, y)

  @doc """
  描画元スプライトを描画先対象の `{x, y}` へ転送します。
  """
  def push_sprite_to(port, src_target, dst_target, x, y),
    do: Sprites.push_sprite_to(port, src_target, dst_target, x, y)

  @doc """
  透明色を使用して描画元スプライトを描画先対象の `{x, y}` へ転送します。
  """
  def push_sprite_to(port, src_target, dst_target, x, y, transparent),
    do: Sprites.push_sprite_to(port, src_target, dst_target, x, y, transparent)

  @doc """
  描画元スプライトを LCD の `{x, y}` へ転送します。
  """
  def push_sprite(port, src_target, x, y), do: Sprites.push_sprite(port, src_target, x, y)

  @doc """
  透明色を使用して描画元スプライトを LCD の `{x, y}` へ転送します。
  """
  def push_sprite(port, src_target, x, y, transparent),
    do: Sprites.push_sprite(port, src_target, x, y, transparent)

  @doc """
  角度と倍率を直接指定して、描画元スプライトを描画先対象へ転送します。
  """
  def push_rotate_zoom_to(port, src_target, dst_target, x, y, angle, zoom) do
    Sprites.push_rotate_zoom_to(port, src_target, dst_target, x, y, angle, zoom)
  end

  @doc """
  角度と倍率を直接指定して、描画元スプライトを描画先対象へ転送します。
  """
  def push_rotate_zoom_to(port, src_target, dst_target, x, y, angle, zoom_x, zoom_y) do
    Sprites.push_rotate_zoom_to(
      port,
      src_target,
      dst_target,
      x,
      y,
      angle,
      zoom_x,
      zoom_y
    )
  end

  @doc """
  角度と倍率を直接指定し、透明色を使用して描画元スプライトを描画先対象へ転送します。
  """
  def push_rotate_zoom_to(
        port,
        src_target,
        dst_target,
        x,
        y,
        angle,
        zoom_x,
        zoom_y,
        transparent
      ) do
    Sprites.push_rotate_zoom_to(
      port,
      src_target,
      dst_target,
      x,
      y,
      angle,
      zoom_x,
      zoom_y,
      transparent
    )
  end

  @doc """
  多数の変形済み描画元スプライトを1つの描画先対象へ転送します。

  `instances` は次の固定小数点形式の組の一覧です。

      {src_target, x, y, angle_cdeg, zoom_x1024, zoom_y1024}

  `angle_cdeg` は `0..35999` の百分の一度です。`zoom_x1024` と `zoom_y1024` は正の x1024 固定小数点倍率で、`1024` が `1.0倍` を表します。

  選択肢:

  - `:transparent` - RGB565 整数または `{:index, n}`
  - `:y_offset` - 各実体の Y 座標からネイティブ側で差し引く値
  - `:approx_cull` - 安全な場合に対象外の変形をネイティブ側で省く
  """
  def push_rotate_zoom_list_to(port, dst_target, instances, opts \\ []) do
    Sprites.push_rotate_zoom_list_to(port, dst_target, instances, opts)
  end

  @doc """
  現在のタッチ位置を画面座標で返します。
  """
  def get_touch(port), do: Touch.get_touch(port)

  @doc """
  現在の生のタッチ位置を制御器座標で返します。
  """
  def get_touch_raw(port), do: Touch.get_touch_raw(port)

  @doc """
  保存するタッチ補正値を設定します。
  """
  def set_touch_calibrate(port, params8), do: Touch.set_touch_calibrate(port, params8)

  @doc """
  対話式のタッチ補正を実行し、結果を8要素の組として返します。
  """
  def calibrate_touch(port), do: Touch.calibrate_touch(port)

  @doc """
  LovyanGFX 形式の倍率値を直接使用して文字倍率を設定します。
  """
  def set_text_size(port, scale, target \\ 0), do: Text.set_text_size(port, scale, target)

  @doc """
  LovyanGFX 形式の倍率値を直接使用して、X 軸と Y 軸の文字倍率を個別に設定します。
  """
  def set_text_size_xy(port, sx, sy, target \\ 0), do: Text.set_text_size_xy(port, sx, sy, target)

  @doc """
  選択した対象の文字基準位置を設定します。
  """
  def set_text_datum(port, datum, target \\ 0), do: Text.set_text_datum(port, datum, target)

  @doc """
  LovyanGFX の1引数形式に従って文字折り返しを設定します。

  設定内容は次のとおりです。

  - `wrap_x = wrap`
  - `wrap_y = false`

  両軸を明示的に制御する場合は `set_text_wrap_xy/4` を使用します。
  """
  def set_text_wrap(port, wrap, target \\ 0), do: Text.set_text_wrap(port, wrap, target)

  @doc """
  X 軸と Y 軸の文字折り返しを明示的に設定します。
  """
  def set_text_wrap_xy(port, wrap_x, wrap_y, target \\ 0),
    do: Text.set_text_wrap_xy(port, wrap_x, wrap_y, target)

  @doc """
  プロトコルで安定して扱える文字書体プリセットを設定します。

  対応する値は `:ascii` と `:jp` です。書体を選択すると、保持している文字倍率を `1.0倍` へ正規化します。描画倍率は `set_text_size/3` または `set_text_size_xy/4` で指定します。
  """
  def set_text_font_preset(port, preset, target \\ 0),
    do: Text.set_text_font_preset(port, preset, target)

  @doc """
  文字の前景色と、任意の背景色を設定します。
  """
  def set_text_color(port, fg_color, bg_color \\ nil, target \\ 0),
    do: Text.set_text_color(port, fg_color, bg_color, target)

  @doc """
  選択した対象の現在の文字カーソルを設定します。
  """
  def set_cursor(port, x, y, target \\ 0), do: Text.set_cursor(port, x, y, target)

  @doc """
  選択した対象の現在の文字カーソルを `{:ok, {x, y}}` で返します。
  """
  def get_cursor(port, target \\ 0), do: Text.get_cursor(port, target)

  @doc """
  選択した対象の `{x, y}` に UTF-8 文字列を描画します。
  """
  def draw_string(port, x, y, text, target \\ 0), do: Text.draw_string(port, x, y, text, target)

  @doc """
  選択した対象の現在のカーソル位置へ UTF-8 文字列を出力します。
  """
  def print(port, text, target \\ 0), do: Text.print(port, text, target)

  @doc """
  選択した対象の現在のカーソル位置へ UTF-8 文字列を改行付きで出力します。
  """
  def println(port, text, target \\ 0), do: Text.println(port, text, target)

  @doc """
  文字列の描画前に文字色と倍率を設定する補助関数です。
  """
  def draw_string_bg(port, x, y, fg_color, bg_color, scale, text, target \\ 0),
    do: Text.draw_string_bg(port, x, y, fg_color, bg_color, scale, text, target)

  @doc """
  Elixir ラッパーが保持する、選択対象の文字状態キャッシュを消去します。
  """
  def reset_text_state(port, target \\ 0), do: Text.reset_text_state(port, target)

  @doc """
  JPEG バイナリーを選択した対象の `{x, y}` へ描画します。
  """
  def draw_jpg(port, x, y, jpeg, target \\ 0), do: Images.draw_jpg(port, x, y, jpeg, target)

  @doc """
  拡張 `drawJpg` プロトコル形式を使用して JPEG バイナリーを描画します。
  """
  def draw_jpg(
        port,
        x,
        y,
        max_width,
        max_height,
        off_x,
        off_y,
        scale_x,
        scale_y,
        jpeg,
        target \\ 0
      ) do
    Images.draw_jpg(
      port,
      x,
      y,
      max_width,
      max_height,
      off_x,
      off_y,
      scale_x,
      scale_y,
      jpeg,
      target
    )
  end

  @doc """
  拡張 JPEG 描画を簡単に呼び出すための補助関数です。
  """
  def draw_jpg_scaled(
        port,
        x,
        y,
        max_width,
        max_height,
        off_x,
        off_y,
        scale,
        jpeg,
        target \\ 0
      ) do
    Images.draw_jpg_scaled(
      port,
      x,
      y,
      max_width,
      max_height,
      off_x,
      off_y,
      scale,
      jpeg,
      target
    )
  end

  @doc """
  拡張 JPEG 描画を簡単に呼び出すための補助関数です。
  """
  def draw_jpg_scaled(
        port,
        x,
        y,
        max_width,
        max_height,
        off_x,
        off_y,
        scale_x,
        scale_y,
        jpeg,
        target
      ) do
    Images.draw_jpg_scaled(
      port,
      x,
      y,
      max_width,
      max_height,
      off_x,
      off_y,
      scale_x,
      scale_y,
      jpeg,
      target
    )
  end

  @doc """
  RGB565 画像バイナリーを選択した対象へ転送します。

  データは、リトルエンディアンの16ビット語で符号化した通常の RGB565 画素として解釈します。これは LovyanGFX で一般的な `uint16_t*` の RGB565 画像バッファーに対応します。対象別のバイト入れ替えは `set_swap_bytes/3` で制御します。

  通常の生画像データには `AtomLGFX.Color.rgb565_le/1` または `AtomLGFX.Color.pixels_le/1` を使用します。上流 LovyanGFX の `swap565_t` 形式に相当する、事前に入れ替えたデータが必要な場合は `AtomLGFX.Color.rgb565_be/1` または `AtomLGFX.Color.pixels_swap565/1` を使用します。

  `AtomLGFX.Color.swap565/1` は1つの RGB565 値のバイト順を変換する関数であり、生バイナリーの符号化関数ではありません。

  大きなデータは、ドライバーが公開するバイナリー上限内に収まるよう、Elixir ラッパーが自動的に分割する場合があります。
  """
  def push_image_rgb565(port, x, y, width, height, pixels, stride_pixels \\ 0, target \\ 0) do
    Images.push_image_rgb565(port, x, y, width, height, pixels, stride_pixels, target)
  end

  @doc """
  ラッパーまたはプロトコルのエラーを読みやすい文字列へ整形します。
  """
  def format_error(reason), do: Errors.format_error(reason)

  defp require_public_op(op_name) do
    if OpSchema.public?(op_name) do
      :ok
    else
      {:error, {:unsafe_lgfx_op, op_name}}
    end
  end
end
