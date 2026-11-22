# 文書案内

初めて使う場合は、次の順に進めてください。

1. [Getting Started](getting-started.md): ツールと配線を準備し、ファームウェアとサンプルを書き込む
2. [Elixir パッケージ](elixir-package.md): 自分のアプリから文字や図形を描く
3. [対応環境](compatibility.md): 使用する基板と確認済みの機能を調べる

## 必要に応じて読む

| 目的 | 文書 |
| --- | --- |
| サンプルを切り替える | [例示アプリ](../examples/elixir/README.md) |
| パネル・GPIO・PSRAM の設定を変える | [ESP-IDF 部品](esp-idf-component.md) |
| Core2 / CoreS3 を使う | [M5Stack 基板](boards/m5stack.md) |

## 開発・保守

- [構成](architecture.md): NIF と装置層の責務、ハンドルとメモリーの扱い
- [操作契約](protocol.md) / [操作参照](protocol-reference.md): ネイティブ境界の形式と検証規則
- [検証手順](testing.md) / [実機の確認結果](validation.md): 変更後の検査と確認済みの範囲
- [リリース手順](../RELEASE.md): Hex と対応する Git タグの公開
