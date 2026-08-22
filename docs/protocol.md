<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ネイティブ操作契約

AtomLGFX のネイティブ実行経路は AtomVM NIF のみです。
この文書は、Elixir と NIF が共有する内部操作契約とバイナリーバッチ契約を説明します。

旧 AtomVM Port の要求／応答組は現在の契約ではありません。

## 定義元

主な定義元は次のとおりです。

- `native/include/atom_lgfx/ops.def`
  - 安定した操作番号
  - 引数個数
  - 許可するフラグ
  - 対象規則
  - 初期化状態規則
  - 機能ビット
  - バッチ対応情報
- `native/include/atom_lgfx/constants.h`
  - 色、機能、バイナリーバッチの定数と制限
- `native/include/atom_lgfx/nif_dispatch.inc`
  - 内部 NIF 呼び出しの term 検証と操作実行
- `native/render_batch.cpp`
  - バイナリーバッチの解析と実行

生成された操作一覧は [操作参照](protocol-reference.md) を参照してください。

## 通常 NIF 呼び出し

高度な `AtomLGFX` API は、内部で次の形へ正規化します。

```text
Native.call(opcode, target, flags, args)
```

NIF 境界では次を検証します。

- `opcode` が定義済みである
- `target` が操作の対象規則に合う
- `flags` に未定義ビットがない
- `args` の個数が操作定義と一致する
- 各引数の型と範囲が正しい
- 初期化が必要な操作は初期化後だけ実行する

検証後は対応する `lgfx_device_*` 関数を直接呼び出します。

`LGFX` の頻出操作は、さらに薄い専用 NIF を利用できます。どちらの経路も同じ
`native/device/` の LovyanGFX 装置層へ到達します。

## 対象

対象値は次のように扱います。

- `0`: LCD
- `1..254`: スプライト

操作ごとの対象規則は `ops.def` で定義します。

- `LGFX_OP_TARGET_BAD_TARGET`
  - LCD のみ
- `LGFX_OP_TARGET_ANY`
  - LCD またはスプライト
- `LGFX_OP_TARGET_SPRITE_ONLY`
  - スプライトのみ

## 色

通常の表示色は RGB565 を基本とします。
Elixir 側では名前付き色や RGB 表現を受け付け、NIF へ渡す前に正規化します。

パレット付きスプライトでは、フラグで RGB565 とパレット番号を区別します。

## バイナリーバッチ

多数の描画命令をまとめる場合は、Elixir 側で命令列をバイナリーへ符号化し、
一度の NIF 呼び出しで `render_batch` へ渡します。

```text
Elixir commands
     |
     v
batch encoder
     |
     v
binary
     |
     v
NIF
     |
     v
render_batch
     |
     v
lgfx_device_*
```

一度だけ使う命令列には `LGFX.batch/2` が便利です。同じ命令列を繰り返す場合は
`LGFX.encode_batch/2` で事前符号化し、`LGFX.submit_batch/1` をホットループ内で使います。

バッチ命令番号は既存の実機利用との互換性を保つため安定させます。

## バイナリーの生存期間

画像、JPEG、文字列、バッチなどのバイナリーは NIF 呼び出し中だけ借用します。
`lgfx_device` は呼び出し終了後に Elixir バイナリーへのポインターを保持しません。

## エラー

NIF は不正な入力を LovyanGFX へ渡す前に拒否します。
主な理由は次のとおりです。

- `bad_args`
- `bad_target`
- `not_initialized`
- `no_memory`
- `unsupported`
- `resource_busy`
- `internal`

詳細な一覧は [操作参照](protocol-reference.md) にあります。

## 変更規則

操作番号やバイナリーバッチ形式を変更する場合は、既存の Elixir コードとファームウェアの
組み合わせに影響するため、互換性を明示的に検討します。

`ops.def` を変更した場合は生成物を同期します。

```bash
elixir scripts/sync_lgfx_protocol_doc.exs
elixir scripts/sync_lgfx_protocol_doc.exs --check
```
