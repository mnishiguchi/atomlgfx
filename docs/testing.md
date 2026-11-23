# 検証手順

コード変更後の確認手順です。ツール・配線の準備は [Getting Started](getting-started.md)、
確認済みの構成は [対応環境](compatibility.md) を参照してください。

## ホストで確認する

リポジトリのルートで実行します。

```bash
just check
mix docs --warnings-as-errors
mix hex.build --unpack --output /tmp/atomlgfx-package
reuse lint --plain
```

`just check` は整形、コンパイル、テスト、生成された操作参照、例示アプリを検査します。
ホストの BEAM には LCD 用 NIF がないため、実機の描画は次の手順で確認します。
操作定義を変えた場合は、先に `elixir scripts/sync_lgfx_protocol_doc.exs` で生成物を更新してください。

## 実機で確認する

ネイティブや SDK 設定を変えた場合は、[ESP-IDF 部品](esp-idf-component.md) の手順で
ファームウェアを再構築・書き込みします。対象パネル・PSRAM の設定と、アプリ領域に収まる画像サイズを確認してください。
Elixir の描画処理だけの変更では、アプリの書き込みで試せます。

```bash
just flash-app nif
just monitor
```

ポートを変える場合は `ATOMLGFX_PORT=/dev/ttyUSB0` を各コマンドの前に付けます。
必要なら接続後にリセットし、`nif_smoke ok viewport=480x320`、`nif_smoke close=:ok`、
正常終了と画面の描画を確認します。寸法は基板・回転により変わります。
モニターは Ctrl+T、Q で終了し、ポートを解放してから次のアプリを書き込んでください。

| モード | 確認すること |
| --- | --- |
| `nif` | `AtomLGFX.LGFX` の基本描画、RGB565、バッチ、不正引数、初期化と終了 |
| `all` | `AtomLGFX` の図形・文字、画像・JPEG、切り抜き、パレット、スプライト、タッチ取得 |
| `dma` | PSRAM の転送元を再描画・色深度変更・破棄・再作成し、`dma_smoke ok` と正常終了を確認 |
| `moving_icons` | PSRAM スプライトの継続描画と `moving_icons stats` の出力 |
| `face` | 顔の描画と `face stats` の出力 |

`dma` / `moving_icons` は PSRAM 有効構成で実行します。
`nif` はファームウェアの構築時設定、他のモードは主に `SampleApp` の実行時設定を使います。
追加モードは [例示アプリ](../examples/elixir/README.md) を参照してください。
ログの成功に加えて、画面の欠け・ちらつきと物理タッチの応答も確認します。

## 性能と結果を記録する

`nif` と `perf` は、符号化・個別呼び出し・バッチ送信を別々に計測します。
`nif_perf speedup_x100=269` は、バッチ送信が個別呼び出しの約2.69倍の速度だったことを示します。
符号化時間は送信時間に含まれません。再利用するバッチは描画ループの外で符号化してください。

測定値はパネル・SPI 周波数・PSRAM・描画内容で変わります。比較時は条件をそろえ、次を記録します。

- atomlgfx・AtomVM・LovyanGFX のコミットと ESP-IDF の版
- 基板・配線・回転後の寸法・SPI 周波数・PSRAM 設定
- ファームウェアサイズ、実行モード、成功・失敗ログ、測定時間
- 画面・タッチを実際に確認した範囲と未確認事項

記録の例は [実機の確認結果](validation.md)、公開時の確認は [リリース手順](../RELEASE.md) を参照してください。
