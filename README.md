# atomlgfx

ESP32 上の AtomVM から [LovyanGFX](https://github.com/lovyan03/LovyanGFX) を使う Elixir ライブラリです。
文字・図形・画像の描画、スプライト、パレット、タッチ入力、描画バッチに対応します。

![AtomLGFX を使った LCD デモ](https://github.com/user-attachments/assets/f2e5db7d-2a52-4fb7-b34b-e3689d4b3f2a)

## はじめる

**[Getting Started: ファームウェアの構築から画面表示まで](docs/getting-started.md)**

必要なのは、次の2つです。Elixir の依存を追加するだけでは NIF は入りません。

| 書き込むもの                   | このリポジトリのコード                                | 役割             |
| ------------------------------ | ----------------------------------------------------- | ---------------- |
| カスタム AtomVM ファームウェア | `CMakeLists.txt`、`native/`、`third_party/LovyanGFX/` | LCD を動かす NIF |
| Elixir アプリ（`.avm`）        | `lib/`、自分のアプリまたは `examples/elixir/`         | 描画する内容     |

動作確認済みの入口は **ESP32-S3 + ILI9488、横向き480x320** です。
配線、必要なツール、初回の書き込みは Getting Started にまとめています。

## インストール

アプリケーションの `mix.exs` に次の依存関係を追加します。

```elixir
{:atomlgfx, "~> 0.1.0"}
```

Hex は Elixir API を配布します。カスタムファームウェアは、対応する
[Git タグ `v0.1.0`](https://github.com/mnishiguchi/atomlgfx/tree/v0.1.0) から構築してください。

## API を選ぶ

| API             | こんなときに使う                                                                                  |
| --------------- | ------------------------------------------------------------------------------------------------- |
| `AtomLGFX.LGFX` | 画面に文字や図形を直接描きたい。少ない手順で表示を試せます                                        |
| `AtomLGFX`      | メモリー上で画像を組み立ててから表示したい（スプライト）、タッチ位置を読みたい、JPEG を表示したい |

迷ったら `AtomLGFX.LGFX` で文字の表示から始めてください。
スプライトやタッチなどが必要になったら、`AtomLGFX` の使用例を参照してください。

上記のファームウェアと既定配線で動かす例:

```elixir
:ok = AtomLGFX.LGFX.init()
:ok = AtomLGFX.LGFX.set_rotation(1)
:ok = AtomLGFX.LGFX.fill_screen(:black)
:ok = AtomLGFX.LGFX.set_text_color(:white)
:ok = AtomLGFX.LGFX.draw_string("Hello AtomLGFX", 16, 16)
:ok = AtomLGFX.LGFX.display()
```

## 文書

- [Getting Started](docs/getting-started.md): ツール、配線、構築、書き込み、成功ログ
- [Elixir パッケージ](docs/elixir-package.md): 自分のアプリへの組み込みと API の使い分け
- [ESP-IDF 部品](docs/esp-idf-component.md): ファームウェアの設定と再構築
- [互換性表](docs/compatibility.md): 動作環境と実機で確認済みの構成
- [文書案内](https://github.com/mnishiguchi/atomlgfx/blob/v0.1.0/docs/README.md): 基板別設定、内部仕様、検証記録
- [変更履歴](CHANGELOG.md)
- [リリース手順](https://github.com/mnishiguchi/atomlgfx/blob/v0.1.0/RELEASE.md): 公開担当者向けの手順
