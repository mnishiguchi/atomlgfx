<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# 構成設計判断記録

構成上の判断は、Markdown の ADR としてリポジトリへ残します。

## 現在の判断関係

現在の実装を理解する場合は、次の有効な ADR から読み始めます。

- [ADR 0018: AtomVM Port を削除し、ネイティブ実行を NIF に統一する](0018-nif-only-native-architecture.md)
  - NIF-only の実行経路、`LGFX` / `AtomLGFX`、バイナリーバッチ、`native/` 構成を定義
- [ADR 0003: 制御器を中心としたパネル・タッチ対応](0003-controller-first-panel-and-touch-support.md)
  - ハードウェア構成の方向性

ADR 0015〜0017 と、それ以前の AtomVM Port／v2 プロトコルに関する ADR は履歴として維持します。
現在の実行経路として読まないでください。

### 基本規則

- 長期的な構成、階層、実行方式、所有権モデル、バッファーモデル、公開 API の動作を変える判断は ADR に残す。
- 1つの ADR は1つの判断へ集中させる。
- 状態には、次の安定した値を使用する。
  - `提案中`
  - `採用`
  - `置換済み`
  - `非推奨`
- 判断が合意された後は、一貫して `採用` を使用する。
- 判断を `提案中` から `採用` へ移す場合は、同じ ADR ファイルを更新する。
- 後の判断が以前の判断を変更または置き換える場合だけ、新しい ADR を作成する。
- その場合、以前の ADR は残し、`置換済み` と記す。
- ある ADR が別の ADR を土台とする場合や置き換える場合は、相互に参照する。

### 対象の目安

次のような判断に ADR を使用します。

- プロトコルと API の意味
- 実行方式の変更
- 実行時処理とバッファー方針
- ドライバー層と機器層の責務分担
- データの所有権と生存期間の規則
- 互換性と版管理の方針

日常的な実装詳細、一時的な確認項目、通常の作業管理には ADR を使用しません。

### 性能測定と作業記録の目安

ADR には長期的に有効な判断を記します。性能測定記録、一時的な測定値、作業確認項目は `docs/worklog/` 配下へ置き、関連する ADR から参照します。

作業記録の推奨名:

```text
docs/worklog/YYYYMMDD-short-topic-work-log.md
```

これにより、判断の根拠となった証拠を残しながら、ADR を読みやすく保てます。

### 変更者向けの規則

ADR がネイティブ操作契約から見える動作を変更する場合は、次の定義元と文書も同時に更新します。

- `native/include/atom_lgfx/ops.def`
- 必要に応じた処理関数または機器コード
- `docs/protocol.md`
- 同期生成するプロトコル表

現在の変更者向け案内でも、外部から見える動作を変更するときは、これらのプロトコル関連箇所を更新することを求めています。

### ファイル名

推奨するファイル名:

```text
docs/adr/NNNN-short-title.md
```

例:

- `docs/adr/0001-explicit-batching-execution-model.md`
- `docs/adr/0002-driver-managed-strip-buffer-composition.md`

### 最小 ADR 雛形

```markdown
# ADR NNNN: 判断名

## 状態

提案中

## 背景

## 判断

## 理由

## 影響

### 良い影響

### 悪い影響

## 採用しなかった代替案

## 今後への影響
```
