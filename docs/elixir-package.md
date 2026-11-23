# Elixir パッケージ

`AtomLGFX.LGFX` / `AtomLGFX` は AtomVM 上で使う API です。
先に [Getting Started](getting-started.md) でカスタムファームウェアと例示アプリを確認してください。
Hex の `atomlgfx` は Elixir API のみを配布します。ネイティブ NIF と LovyanGFX は対応する Git tag のソースから AtomVM 本体へ組み込みます。
PC の通常の BEAM でコンパイルしても、LCD 用 NIF は入りません。

## 自分のアプリを作る

```bash
mix new my_lcd
cd my_lcd
```

`mix.exs` の `project/0` に AtomVM の開始モジュールと書き込み先を追加します。
`application/0` の `extra_applications` は、この最小例では空にします。

```elixir
def project do
  [
    app: :my_lcd,
    version: "0.1.0",
    elixir: "~> 1.17",
    deps: deps(),
    atomvm: [start: MyLcd, flash_offset: 0x250000]
  ]
end

def application, do: [extra_applications: []]

defp deps do
  [
    {:atomlgfx, "~> 0.1.0"},
    {:exatomvm, github: "atomvm/ExAtomVM"}
  ]
end
```

Hex の `0.1.0` に対応するファームウェアは、atomlgfx の Git tag `v0.1.0` を checkout して構築します。
開発中に Git 依存を使用する場合は、ファームウェアと Elixir パッケージの両方を同じフルコミット SHA に固定してください。
取得後の `mix.lock` も管理してください。
`0x250000` はこのガイドの AtomVM パーティション構成でのアプリ領域です。

`lib/my_lcd.ex` を次の内容にします。Getting Started の既定配線を使う例です。

```elixir
defmodule MyLcd do
  def start do
    :ok = AtomLGFX.LGFX.init()
    :ok = AtomLGFX.LGFX.set_rotation(1)
    :ok = AtomLGFX.LGFX.fill_screen(:black)
    :ok = AtomLGFX.LGFX.set_text_color(:white)
    :ok = AtomLGFX.LGFX.draw_string("Hello AtomLGFX", 16, 16)
    :ok = AtomLGFX.LGFX.display()
  end
end
```

```bash
mix deps.get
mix atomvm.esp32.flash --port /dev/ttyACM0
```

ファームウェアが同じ操作契約の NIF を含む限り、描画処理の変更はアプリの書き込みだけで試せます。
実行時に配線などを変える場合は `AtomLGFX.LGFX.init/1` へ設定を渡します。

## API を選ぶ

| API | 使いどころ | 戻り値の例 |
| --- | --- | --- |
| `AtomLGFX.LGFX` | 画面に文字や図形を直接描く。最初の表示確認はこちらから | 描画は `:ok`、寸法は整数 |
| `AtomLGFX` | メモリー上で画像を組み立てて表示する（スプライト）、タッチ位置を読む、JPEG を表示する | 描画は `:ok`、照会は `{:ok, value}` |

入力エラーなどは `{:error, reason}` を返します。NIF の引数検査では例外になる場合もあります。
PC の BEAM で NIF へ到達すると `:nif_not_loaded` になります。詳細は各モジュールの API 文書を参照してください。

内部の AtomLGFX.Native は NIF 境界で、通常は直接呼びません。

`AtomLGFX` でスプライトを使う例:

```elixir
{:ok, handle} = AtomLGFX.open(panel_driver: :ili9488, width: 320, height: 480)
:ok = AtomLGFX.init(handle)
:ok = AtomLGFX.create_sprite(handle, 120, 72, 16, 1)

:ok =
  AtomLGFX.render_sprite(handle, 1, [
    {:clear, :navy},
    {:draw_string, "sprite", 8, 8}
  ])

:ok =
  AtomLGFX.render_lcd(handle, [
    {:fill_screen, :black},
    {:push_sprite, 1, 24, 32},
    :display
  ])

:ok = AtomLGFX.close(handle)
```

`handle` は、`open/1` で指定した設定を後の操作へ引き継ぐための値です。
設定とキャッシュは作成したプロセスが所有します。同じプロセスから使用してください。
別プロセスや不明なハンドルでの `init/1` は `{:error, :invalid_handle}` を返します。
`handle` ごとに物理 LCD が作られるわけではありません。ネイティブの LCD は単一実体なので、両 API で独立した LCD を同時に所有することはできません。

## 描画をまとめる

一度だけ使う命令列は `AtomLGFX.LGFX.batch/2` または `AtomLGFX.render/3` へ渡します。
同じ命令列を繰り返す場合は、ループの外で符号化します。

```elixir
{:ok, batch} =
  AtomLGFX.LGFX.encode_batch([
    {:fill_rect, 10, 10, 100, 40, :red},
    {:draw_string, "Hello", 20, 20}
  ])

:ok = AtomLGFX.LGFX.submit_batch(batch)
```

低水準の命令構築には `AtomLGFX.BinaryBatch` を使えます。

## 次に読む

- [ESP-IDF 部品](esp-idf-component.md): 構築時設定、実行時設定、再構築の範囲
- [構成](https://github.com/mnishiguchi/atomlgfx/blob/v0.1.0/docs/architecture.md): 所有権と内部の責務
- [例示アプリ](https://github.com/mnishiguchi/atomlgfx/blob/v0.1.0/examples/elixir/README.md): 描画と実機確認の実例

バージョン付きの API とネイティブ NIF は同じ Git tag から作られます。
公開・検証の手順は [RELEASE.md](https://github.com/mnishiguchi/atomlgfx/blob/v0.1.0/RELEASE.md) を参照してください。
