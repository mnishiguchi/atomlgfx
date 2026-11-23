# 変更履歴

Hex の Elixir API と、ファームウェアへ組み込む NIF は同じ Git タグで対応させます。

## 未リリース

## 0.2.0

- MovingIcons を C++ / FreeRTOS で実装し、Elixir から NIF 経由で起動できるようにした
- 50個のアイコンを継続描画する `moving_icons_native` サンプルを追加
- ファームウェア構築の解説と GitHub Release 作成用コマンドを追加

## 0.1.0

初回リリース。AtomVM から LovyanGFX を使うための Elixir API と ESP-IDF / NIF 部品を提供します。

- ハンドル不要の `AtomLGFX.LGFX` と、スプライト・タッチ・JPEG などを扱う `AtomLGFX` を提供
- 直接描画とバイナリーバッチ、PSRAM スプライト、パレット、切り抜きに対応
- LovyanGFX 1.3.0 を採用し、DMA 転送完了前のバッファ再利用・破棄を防止
- ファームウェア構築スクリプト、Elixir サンプル、Getting Started を整備

動作確認した基板と制約は [対応環境](docs/compatibility.md) を参照してください。
Hex は Elixir API の配布先です。LCD の動作には、Git タグ `v0.1.0` から構築した NIF が必要です。
