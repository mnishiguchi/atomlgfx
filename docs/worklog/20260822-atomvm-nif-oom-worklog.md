# 2026-08-22 AtomVM NIF 呼び出し時の OOM 調査

## 概要

ESP32-S3 上で `SAMPLE_APP_MODE=all` を実行すると、スプライト描画後に AtomVM が OOM で終了する問題を調査した。

不要な一時スプライトの保持を修正した後も OOM が残り、最終的に NIF 呼び出し前の Elixir 側処理によるメモリー消費が主因と判明した。

## 原因

`Protocol.call` の5引数版では、NIF 呼び出し前に以下の処理を行っていた。

- `OpSchema.validate_wire_call/3`
- `Map` を用いた操作名検索
- `Keyword` を用いた opcode 取得
- 引数数・flags の検証

一方、NIF 側でも同等の検証を行っていたため、AtomVM 上では重複した処理と一時データ生成が発生していた。

メモリー余裕が少ない状態では、この処理中に OOM となり、NIF 自体へ到達できないケースがあった。

## 対応

- `OpSchema.opcode/1` を実行時の `Map` / `Keyword` 検索から、生成済み関数節による検索へ変更
- `Protocol.call` と `call_ok` から重複する Elixir 側の検証を削減
- binary batch は generic `call/5` を経由せず `Native.batch/2` を直接利用
- 一時スプライトを不要になった時点で早めに解放

## 確認結果

- `mix test`
  - 102 tests, 0 failures
- `SAMPLE_APP_MODE=all`
  - 成功
- `SAMPLE_APP_MODE=moving_icons`
  - 目標 5 FPS を維持
- 調査用ログ、GC、直接 NIF 呼び出しはすべて削除

## 学び

AtomVM のようなメモリー制約の強い環境では、固定的なプロトコル情報を毎回 `Map` や `Keyword` で検索する処理でも無視できない負荷になる。

固定された opcode やメタデータは、可能であればコンパイル時に関数節へ展開し、NIF 境界までの処理を小さく保つ方が適している。
