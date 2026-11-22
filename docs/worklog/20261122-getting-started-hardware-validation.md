# Getting Started 実機検証 - 2026-11-22

PR #5 の手順を、ホストに接続した ESP32-S3 と ILI9488 で確認した。
専用の新規 AtomVM checkout からカスタムファームウェアを構築・書き込みし、
Elixir アプリを別に書き込む経路が成功した。既定構成で `nif` と `face`、
PSRAM 有効構成で `all` と `face` を確認した。

## ソースと機器

| 項目 | 構成 |
| --- | --- |
| PR ブランチ | `docs/clarify-firmware-and-elixir-setup` |
| 検証開始時の atomlgfx | `b3fbb393b39e72bcf15858ffc456c6a064c67b25` |
| 取り込んだ origin の justfile 更新 | `19dddce34c730efd70d4664b3a19f01435dfcb84` |
| AtomVM | `e2cacc998f455ad66b1aa9e6391f7b32928cc38d` |
| LovyanGFX | `6f6e9a052fc719ecb2b42640b0207215050d2560` |
| ESP-IDF | v5.5.4（EIM の選択名は `v5.5`） |
| Elixir / OTP | 1.19.5 / 28.5.0.7 |
| esptool / tio | 5.6.0 / 3.9 |
| ESP32-S3 | QFN56 revision v0.2、8 MB flash、8 MB 内蔵 PSRAM |
| USB Serial/JTAG | `/dev/ttyACM0`、MAC `74:4d:bd:81:5f:c8` |
| LCD | ILI9488、320x480、回転1で480x320 |
| SPI2 | SCLK 7、MOSI 9、MISO 8、LCD CS 43、DC 3、RST 2 |
| タッチ | XPT2046、CS 44、IRQ なし |

ネイティブソースの変更はない。検証中に origin の justfile と案内文の更新を取り込み、
本報告と手順の修正、ExDoc 登録、`just monitor` の修正を追加した。

## 構築と実機結果

`ATOMVM_DIR=/tmp/atomlgfx-pr5-AtomVM` を指定して既存の AtomVM 作業ツリーから分離した。
`scripts/atomvm_esp32.exs build --target esp32s3 --component .` は、新規 clone、
固定コミットへの切り替え、部品のリンク、ホストツール、Elixir boot AVM、ESP32-S3
ファームウェアの構築まで成功した。続けて同スクリプトの `flash` を実行した。

| ファームウェア | 画像サイズ | アプリ領域の空き | 実機確認 |
| --- | ---: | ---: | --- |
| 追加 SDK 設定なし | 1,776,368 bytes | 58,640 bytes（3%） | `nif`、`face` |
| 8 MB flash + octal PSRAM、80 MHz | 1,789,088 bytes | 45,920 bytes（3%） | `all`、`face` |

PSRAM 構成には Getting Started の `--sdkconfig` 例と同じ4項目を渡した。
起動ログで `Found 8MB PSRAM device`、`Speed: 80MHz`、`SPI SRAM memory test OK`、
8192K のヒープ追加、`psram_sprites=1` を確認した。既定構成では `psram_sprites=0` だった。

- `nif`: `nif_api_smoke ok`、`nif_error_path ok`、`nif_smoke ok viewport=480x320`、
  `nif_smoke close=:ok`、正常終了を確認。120矩形の個別 NIF は414,029 us、
  事前符号化バッチは153,911 us（約2.69倍）だった。
- `face`: 回転1、480x320、320x240 の4-bit sprite を確認。
  約30秒の記録中、300フレームまで統計と表情変更が出力され、異常終了はなかった。
- `all`: `just flash-app all` で書き込み。バッチ、基本図形、文字、切り抜き、JPEG、
  画像、色・パレット、タッチ probe、スプライト、sprite protocol の成功と
  `AtomLGFX closed`、`Return value: ok` を確認した。

最後に `just flash-app face` で顔のデモへ戻し、PSRAM 有効構成でも回転1、480x320 と
連続するフレーム統計を確認した。基板はこのデモを実行したままにした。

確認は USB シリアルログによる。画面の外観や物理的なタッチ操作は目視確認していない。
単一の配布用 `.img` を作る `mkimage` は今回の確認対象に含めていない。

## ホスト側検査と修正した点

- `mix test`: 105試験、失敗0。
- ルートと例示アプリの整形検査、警告をエラーにするコンパイルが成功。
- `scripts/sync_lgfx_protocol_doc.exs --check` が成功。
- `mix docs --warnings-as-errors`、`mix hex.build --unpack` が成功。
- `just --list`、ファームウェア書き込みの dry run、`just check`、
  `just flash-app all`、`just monitor` の接続・終了を確認。`just --fmt --check` が成功。
- REUSE 検査が成功。

新しい案内文を ExDoc の `extras` に登録しないと、ルート README のリンクが解決せず
CI の文書構築が失敗した。Getting Started、例示アプリの README、そこから参照する
実機手順と検証報告を登録して解消した。

`nif` は `LGFX.init/0` を呼ぶため、`@sample_open_options` の変更は適用されない。
この違い、既定配線、PSRAM 設定、回転後の寸法、成功ログを手順へ追記した。

構築補助スクリプトの `monitor` は、PTY 付きで起動しても EIM 経由の標準入力が
TTY と判定されず失敗した。手順と `just monitor` は `tio` を直接使う形にした。

Hex の DNS 解決はこのホストで失敗したが、既存キャッシュで依存を取得できた。
just の実機検証と `just check` では `HEX_OFFLINE=1` を指定した。
依存キャッシュのないホストでの完全な新規依存取得は未検証。
AtomVM のホスト構築には upstream の警告があり、ファームウェア構築には
アプリ領域の残量警告があったが、いずれも構築・実行を妨げなかった。
