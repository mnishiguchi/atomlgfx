# 文書案内

初めて使う場合は **[Getting Started](getting-started.md)** から進めてください。
ファームウェアを書き込み、Elixir の例示アプリで480x320の表示を確認するまでを説明しています。

## 使うための文書

| 目的 | 読む文書 |
| --- | --- |
| 最初の構築・配線・書き込み | [Getting Started](getting-started.md) |
| 自分の Elixir アプリを作る、API を選ぶ | [Elixir パッケージ](elixir-package.md) |
| サンプルを切り替える | [例示アプリ](../examples/elixir/README.md) |
| パネル、GPIO、機能の構築時設定を変える | [ESP-IDF 部品](esp-idf-component.md) |
| M5Stack の設定を調べる | [M5Stack 基板](boards/m5stack.md) |
| 実機で機能・性能を確かめる | [実機確認](hardware-smoke-performance.md) |

## 開発・保守の文書

- [構成](architecture.md): NIF と装置層、所有権、データの生存期間
- [ネイティブ操作契約](protocol.md) / [生成された操作参照](protocol-reference.md)
- [ネイティブ実装](../native/README.md) / [LovyanGFX 装置層](../native/device/README.md)
- [公開前検証](pre-release-validation.md)
- [ADR 案内](adr/README.md): 現在の判断と過去の設計経緯

## 検証記録と過去資料

作業記録はその時点の構成と結果です。現在のセットアップには上記の手順を使ってください。

- [2026-11-22 Getting Started の ESP32-S3 実機検証](worklog/20261122-getting-started-hardware-validation.md)
- [2026-08-26 公開前検証](worklog/20260826-release-validation-report.md)
- [2026-08-22 NIF 呼び出し時の OOM 調査](worklog/20260822-atomvm-nif-oom-worklog.md)
- [2026-08-21 NIF-only 実機検証](worklog/20260821-nif-only-hardware-validation-report.md)
- [v1 → v2 移行](migration-v1-to-v2.md): 置き換え済み API の履歴

その他の測定・調査記録は `worklog/` にあります。
