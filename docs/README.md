<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# 文書案内

`docs/` には、現在の設計・利用方法と、設計判断や実機検証の履歴を置きます。

現在の実装を理解する場合は、まず「現在の設計」から読んでください。
ADR と作業記録には過去の構成や名称が残ることがあります。

## 現在の設計

- [構成](architecture.md)
  - Elixir API、NIF、バイナリーバッチ、LovyanGFX 装置層の全体像
- [Elixir パッケージ](elixir-package.md)
  - `LGFX` と `AtomLGFX` の利用方法
- [ESP-IDF 部品](esp-idf-component.md)
  - ネイティブ部品の構成と構築方法
- [ネイティブ操作契約](protocol.md)
  - Elixir と NIF が共有する内部操作契約
- [ネイティブ操作参照](protocol-reference.md)
  - ソースから生成する操作一覧

## 検証

- [実機の動作確認と性能検証](hardware-smoke-performance.md)
- [公開前検証の確認表](pre-release-validation.md)

## 移行

- [v3 API への移行](migration-to-v3.md)
  - 旧 v2 API から現在の描画優先 API への移行
- [v1 から v2 への移行](migration-v1-to-v2.md)
  - 置き換え済み API の履歴資料

## 基板

- [M5Stack 基板](boards/m5stack.md)

## 構成設計判断記録

- [ADR 案内](adr/README.md)
- 現在のネイティブ構成は [ADR 0018](adr/0018-nif-only-native-architecture.md) を参照

古い ADR は判断の履歴として維持します。
現在の実装仕様としてではなく、当時の背景を確認するために利用します。

## 作業記録

実機検証、性能測定、障害調査などの時点記録は `docs/worklog/` に置きます。

- [2026-08-21 NIF-only 実機検証報告](worklog/20260821-nif-only-hardware-validation-report.md)
- [2026-08-22 AtomVM NIF 呼び出し時の OOM 調査](worklog/20260822-atomvm-nif-oom-worklog.md)

作業記録は判断の根拠を残すための資料であり、現在の仕様そのものではありません。
