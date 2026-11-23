# 実機の確認結果

2026-11-22 に実施した [LovyanGFX 1.3.0 対応（PR #8）](https://github.com/mnishiguchi/atomlgfx/pull/8) と
[描画 API の検証（PR #9）](https://github.com/mnishiguchi/atomlgfx/pull/9) の検証記録をまとめています。
2026-11-23 の開発ブランチでの追加検証は別節に記録します。
再現手順は [検証手順](testing.md) を参照してください。

## 構成

- LovyanGFX 1.3.0、AtomVM `e2cacc998f455ad66b1aa9e6391f7b32928cc38d`、ESP-IDF v5.5
- ホスト: Elixir 1.19.5 / OTP 28
- XIAO ESP32S3: 8 MB flash、8 MB octal PSRAM、CPU 160 MHz、PSRAM 80 MHz
- ILI9488: 回転1で480x320、通常60 MHz SPI、`nif` は20 MHz SPI
- 配線は [Getting Started](getting-started.md)、M5Stack の設定は [基板別ガイド](boards/m5stack.md) と同じ

## XIAO ESP32S3 + ILI9488

ファームウェアを全再構築して書き込み、PSRAM の検出とメモリーテストの成功を確認しました。

| 確認 | 結果 |
| --- | --- |
| `nif` | 基本描画、RGB565、バッチ、不正引数、480x320、正常終了 |
| `smoke` / `all` / `sprites` | 図形・文字、クリップ、JPEG、画像、色・パレット、タッチ取得、スプライト、正常終了 |
| `dma` | 二重の書き込み区間を維持し、38,400 bytes の転送元を40回再利用。直接呼び出し・バッチで再描画、色深度変更、破棄、再作成が成功 |
| `moving_icons` | 45秒間、6オブジェクトで5 FPS の統計を継続出力 |
| 描画 API | 更新した `nif` サンプルで `nif_api_smoke ok`、`nif_error_path ok`、`nif_smoke ok viewport=480x320`、`close=:ok`、正常終了 |
| `face` | Simple API の確認後に書き戻し、480x320 で初期化とフレーム更新ログを確認 |

採取中に panic、heap corruption、watchdog による再起動はありませんでした。
これらはシリアルログによる確認です。画面の外観や物理タッチの精度は確認していません。

## 2026-11-23: ネイティブ MovingIcons（開発ブランチ）

XIAO ESP32-S3 + ILI9488（回転1、480x320、SPI 60 MHz）で `moving_icons_native` を実行しました。
50個のアイコンを対象に、30秒の自動終了を撤廃した後も描画統計が継続して出力され、
31.3秒時点で259フレーム、平均約8 FPS を確認しました。`just check` は109テスト成功です。
観測範囲でクラッシュや再起動はありませんでした。

これは開発ブランチのシリアルログによる確認であり、リリース版 `v0.1.0` の検証ではありません。
画面の目視、長時間安定性、停止・再初期化は未確認です。
設定上の注意点は [作業記録](worklog/20261123-native-moving-icons-validation.md) に残しました。

## M5Stack Core2 / CoreS3

両基板とも PSRAM を無効にし、320x240、20 MHz SPI で確認しました。

| 確認 | Core2 | CoreS3 |
| --- | --- | --- |
| プリセット | `m5stack_core2` | `m5stack_cores3` |
| 初期化・文字・図形・48x32 RGB565 スプライト | 成功 | 成功 |
| タッチ取得 | 80回正常応答、入力は `:none` | `{77, 111, 1}` などの座標を観測 |
| 顔デモの55秒記録 | 最後の統計 frame=420 | 最後の統計 frame=480 |

両画面の文字・矩形・小スプライトは利用者が目視確認しました。
顔デモは両基板とも320x240・4-bit sprite を使用し、ログ採取中に描画エラーや再起動はありませんでした。
顔デモの動きは目視確認していません。サーボ制御は含みません。

## ファームウェアサイズ

使用したアプリ領域は `0x1c0000`（1,835,008 bytes）です。

| 構成 | 画像サイズ | アプリ領域の空き |
| --- | ---: | ---: |
| ESP32-S3 + PSRAM | 1,803,520 bytes | 31,488 bytes |
| ESP32 / Core2（4 MB flash 設定） | 1,795,760 bytes | 39,248 bytes |
| CoreS3（16 MB flash 設定） | 1,792,608 bytes | 42,400 bytes |

XIAO の ELF SHA256 は `b29794fe6dbae18b44763eac8274acb447bd4ea1d1d8837bad3a8159460adb19` で、起動ログと一致しました。
領域の空きが少ないため、機能やフォントを追加する際はサイズを再確認してください。

## 未確認の範囲

- タッチの較正・精度、Core2 の物理タップによる座標取得、各基板の全周辺機能
- M5Stack の PSRAM 有効構成、PSRAM のピーク使用量、長時間動作
- LCD の画素読み戻しによる一致試験、表示用短冊の専用実機試験、SCT 無効版との実機比較
