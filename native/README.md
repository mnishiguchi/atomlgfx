# ネイティブ実装

AtomVM NIF と LovyanGFX 装置層を置きます。利用者向けの構築手順は
[Getting Started](../docs/getting-started.md)、設定は [ESP-IDF 部品](../docs/esp-idf-component.md) を参照してください。

## ファイルと担当

| ファイル | 担当 |
| --- | --- |
| `nif.c` | AtomVM NIF 集合の入口 |
| `include/atom_lgfx/nif.inc` | 頻出操作の専用 NIF と項変換 |
| `include/atom_lgfx/nif_dispatch.inc` | `AtomLGFX.Native.call/4` の検証と振り分け |
| `open_config.c` | 実行時設定の解析 |
| `render_batch.cpp` | バイナリーバッチの解析と実行 |
| `include/atom_lgfx/ops.def` | 操作番号と検証情報の定義元 |
| `include/atom_lgfx/constants.h` | 機能ビット、操作フラグ、バッチ定数 |
| `device/` | LovyanGFX 装置層 |

## 変更時の規則

- NIF 層で AtomVM 項を変換・検証し、描画は `lgfx_device_*` へ渡す。
- 直接 NIF とバッチで同じ描画を二重実装しない。
- NIF 呼び出し終了後に AtomVM バイナリーへのポインターを保持しない。
- 操作契約の変更は `ops.def` と [操作契約](../docs/protocol.md) に反映し、生成物を同期する。

変更後の検査は [検証手順](../docs/testing.md) を参照してください。
ネイティブ変更には、ホスト検査に加えて AtomVM / ESP-IDF の構築と実機確認が必要です。
