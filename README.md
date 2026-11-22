# atomlgfx

ESP32 上の AtomVM から [LovyanGFX](https://github.com/lovyan03/LovyanGFX) を使う Elixir ライブラリです。
文字・図形・画像の描画、スプライト、パレット、タッチ入力、描画バッチに対応します。

## はじめる

**[Getting Started: ファームウェアの構築から画面表示まで](docs/getting-started.md)**

必要なのは、次の2つです。Elixir の依存を追加するだけでは NIF は入りません。

| 書き込むもの | このリポジトリのコード | 役割 |
| --- | --- | --- |
| カスタム AtomVM ファームウェア | `CMakeLists.txt`、`native/`、`third_party/LovyanGFX/` | LCD を動かす NIF |
| Elixir アプリ（`.avm`） | `lib/`、自分のアプリまたは `examples/elixir/` | 描画する内容 |

両方で同じ atomlgfx の Git コミットを使ってください。
動作確認済みの入口は **ESP32-S3 + ILI9488、横向き480x320** です。
配線、必要なツール、初回の書き込みは Getting Started にまとめています。

## API を選ぶ

| API | こんなときに使う |
| --- | --- |
| `AtomLGFX.Simple` | 画面に文字や図形を直接描きたい。少ない手順で表示を試せます |
| `AtomLGFX` | メモリー上で画像を組み立ててから表示したい（スプライト）、タッチ位置を読みたい、JPEG を表示したい |

迷ったら `AtomLGFX.Simple` で文字の表示から始めてください。
スプライトやタッチなどが必要になったら、`AtomLGFX` の使用例を参照してください。

上記のファームウェアと既定配線で動かす例:

```elixir
alias AtomLGFX.Simple

:ok = Simple.init()
:ok = Simple.set_rotation(1)
:ok = Simple.fill_screen(:black)
:ok = Simple.set_text_color(:white)
:ok = Simple.draw_string("Hello AtomLGFX", 16, 16)
:ok = Simple.display()
```

両 API は同じ AtomVM NIF と単一の表示機器を使います。
`AtomLGFX.Simple` は既存の `LGFX` へ委譲します。`LGFX` は互換用として引き続き使えます。
実行時設定、スプライト、バッチの例は [Elixir パッケージ](docs/elixir-package.md) を参照してください。

## 文書

- [Getting Started](docs/getting-started.md): ツール、配線、構築、書き込み、成功ログ
- [Elixir パッケージ](docs/elixir-package.md): 自分のアプリへの組み込みと API の使い分け
- [ESP-IDF 部品](docs/esp-idf-component.md): ファームウェアの設定と再構築
- [文書案内](https://github.com/mnishiguchi/atomlgfx/blob/main/docs/README.md): 基板別設定、内部仕様、検証記録
- [変更履歴](CHANGELOG.md)

`mix.exs` の `0.1.0` は構築用の仮値です。現在は Git コミットを指定して利用します。
