<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# AtomLGFX ネイティブ操作参照

この文書は、ソース情報と同期して生成する NIF 操作契約の参照表を収録します。

人が管理する契約本文は [ネイティブ操作契約](protocol.md) を参照してください。

## 生成されたエラー理由

NIF 境界で使用するエラー理由です。

<!-- BEGIN:generated_error_reasons_table -->
<!-- scripts/sync_lgfx_protocol_doc.exs により生成 -->

| C マクロ | アトム | 種別 |
| --- | --- | --- |
| `LGFX_ERR_BAD_ARGS` | `bad_args` | `正規` |
| `LGFX_ERR_BAD_TARGET` | `bad_target` | `正規` |
| `LGFX_ERR_NOT_INITIALIZED` | `not_initialized` | `正規` |
| `LGFX_ERR_NO_MEMORY` | `no_memory` | `正規` |
| `LGFX_ERR_INTERNAL` | `internal` | `正規` |
| `LGFX_ERR_UNSUPPORTED` | `unsupported` | `正規` |
| `LGFX_ERR_RESOURCE_BUSY` | `resource_busy` | `正規` |
<!-- END:generated_error_reasons_table -->

## 実装済み操作表

実装済みの NIF 操作面を示します。

実装上の正式な定義元は `ops.def` です。

<!-- BEGIN:generated_ops_matrix -->
<!-- scripts/sync_lgfx_protocol_doc.exs により生成 -->

| 操作 | 対象規則 | フラグ規則 | 引数 | 状態規則 | 機能ビット | バッチ経路 |
| --- | --- | --- | --- | --- | --- | --- |
| `ping` | `T0/bad_target` | `F0` | `0` | `any` | - | `sync` |
| `get_caps` | `T0/bad_target` | `F0` | `0` | `any` | - | `sync` |
| `get_last_error` | `T0/bad_target` | `F0` | `0` | `any` | `LGFX_CAP_LAST_ERROR` | `sync` |
| `width` | `Tany` | `F0` | `0` | `requires_init` | - | `sync` |
| `height` | `Tany` | `F0` | `0` | `requires_init` | - | `sync` |
| `init` | `T0/bad_target` | `F0` | `0` | `any` | - | `sync` |
| `close` | `T0/bad_target` | `F0` | `0` | `any` | - | `sync` |
| `start_write` | `T0/bad_target` | `F0` | `0` | `requires_init` | - | `sync` |
| `end_write` | `T0/bad_target` | `F0` | `0` | `requires_init` | - | `sync` |
| `set_rotation` | `T0/bad_target` | `F0` | `1` | `requires_init` | - | `sync` |
| `set_brightness` | `T0/bad_target` | `F0` | `1` | `requires_init` | - | `sync` |
| `set_color_depth` | `Tany` | `F0` | `1` | `requires_init` | - | `sync` |
| `set_swap_bytes` | `Tany` | `F0` | `1` | `requires_init` | - | `sync` |
| `display` | `T0/bad_target` | `F0` | `0` | `requires_init` | - | `batch`<br>`boundary` |
| `fill_screen` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `1` | `requires_init` | - | `batch` |
| `clear` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `1` | `requires_init` | - | `batch` |
| `draw_pixel` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `3` | `requires_init` | - | `batch` |
| `draw_fast_vline` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `4` | `requires_init` | - | `batch` |
| `draw_fast_hline` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `4` | `requires_init` | - | `batch` |
| `draw_line` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `5` | `requires_init` | - | `batch` |
| `draw_rect` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `5` | `requires_init` | - | `batch` |
| `fill_rect` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `5` | `requires_init` | - | `batch` |
| `draw_round_rect` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `6` | `requires_init` | - | `batch` |
| `fill_round_rect` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `6` | `requires_init` | - | `batch` |
| `draw_circle` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `4` | `requires_init` | - | `batch` |
| `fill_circle` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `4` | `requires_init` | - | `batch` |
| `draw_ellipse` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `5` | `requires_init` | - | `batch` |
| `fill_ellipse` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `5` | `requires_init` | - | `batch` |
| `draw_arc` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `7` | `requires_init` | - | `batch` |
| `fill_arc` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `7` | `requires_init` | - | `batch` |
| `draw_bezier` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `7/9` | `requires_init` | - | `batch` |
| `draw_triangle` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `7` | `requires_init` | - | `batch` |
| `fill_triangle` | `Tany` | `Fmask(LGFX_F_COLOR_INDEX)` | `7` | `requires_init` | - | `batch` |
| `set_text_size` | `Tany` | `F0` | `1/2` | `requires_init` | - | `batch` |
| `set_text_datum` | `Tany` | `F0` | `1` | `requires_init` | - | `batch` |
| `set_text_wrap` | `Tany` | `F0` | `1/2` | `requires_init` | - | `batch` |
| `set_text_font_preset` | `Tany` | `F0` | `1` | `requires_init` | - | `batch` |
| `set_text_color` | `Tany` | `Fmask((LGFX_F_TEXT_HAS_BG | LGFX_F_TEXT_FG_INDEX | LGFX_F_TEXT_BG_INDEX))` | `1/2` | `requires_init` | - | `batch` |
| `set_cursor` | `Tany` | `F0` | `2` | `requires_init` | - | `batch` |
| `get_cursor` | `Tany` | `F0` | `0` | `requires_init` | - | `sync` |
| `draw_string` | `Tany` | `F0` | `3` | `requires_init` | - | `batch`<br>`payload` |
| `print` | `Tany` | `F0` | `1` | `requires_init` | - | `batch`<br>`payload` |
| `println` | `Tany` | `F0` | `1` | `requires_init` | - | `batch`<br>`payload` |
| `draw_jpg` | `Tany` | `F0` | `3/9` | `requires_init` | - | `payload`<br>`sync` |
| `push_image` | `Tany` | `F0` | `6` | `requires_init` | `LGFX_CAP_PUSHIMAGE` | `payload`<br>`sync` |
| `set_clip_rect` | `Tany` | `F0` | `4` | `requires_init` | - | `batch` |
| `clear_clip_rect` | `Tany` | `F0` | `0` | `requires_init` | - | `batch` |
| `create_sprite` | `Tsprite` | `F0` | `2/3` | `requires_init` | `LGFX_CAP_SPRITE` | `sync` |
| `delete_sprite` | `Tsprite` | `F0` | `0` | `requires_init` | `LGFX_CAP_SPRITE` | `sync` |
| `create_palette` | `Tsprite` | `F0` | `0` | `requires_init` | `LGFX_CAP_PALETTE` | `sync` |
| `set_palette_color` | `Tsprite` | `F0` | `2` | `requires_init` | `LGFX_CAP_PALETTE` | `batch` |
| `set_pivot` | `Tany` | `F0` | `2` | `requires_init` | `LGFX_CAP_SPRITE` | `batch` |
| `push_sprite` | `Tsprite` | `Fmask(LGFX_F_TRANSPARENT_INDEX)` | `3/4` | `requires_init` | `LGFX_CAP_SPRITE` | `batch` |
| `push_rotate_zoom` | `Tsprite` | `Fmask(LGFX_F_TRANSPARENT_INDEX)` | `6/7` | `requires_init` | `LGFX_CAP_SPRITE` | `batch` |
| `get_touch` | `T0/bad_target` | `F0` | `0` | `requires_init` | `LGFX_CAP_TOUCH` | `sync` |
| `get_touch_raw` | `T0/bad_target` | `F0` | `0` | `requires_init` | `LGFX_CAP_TOUCH` | `sync` |
| `set_touch_calibrate` | `T0/bad_target` | `F0` | `8` | `requires_init` | `LGFX_CAP_TOUCH` | `sync` |
| `calibrate_touch` | `T0/bad_target` | `F0` | `0` | `requires_init` | `LGFX_CAP_TOUCH` | `sync` |
| `push_rotate_zoom_list` | `Tany` | `Fmask(LGFX_F_TRANSPARENT_INDEX)` | `1` | `requires_init` | `LGFX_CAP_SPRITE` | `batch` |
| `submit_binary_batch` | `Tany` | `F0` | `1` | `requires_init` | `LGFX_CAP_BATCH` | `sync` |
<!-- END:generated_ops_matrix -->

## 生成された機能語彙

<!-- BEGIN:generated_caps_table -->
<!-- scripts/sync_lgfx_protocol_doc.exs により生成 -->

| C マクロ | プロトコルビット | 桁位置 | 値 | 定義元 |
| --- | --- | --- | --- | --- |
| `LGFX_CAP_SPRITE` | `CAP_SPRITE` | `0` | `0x0001` | `ops.def` の `feature_cap_bit` |
| `LGFX_CAP_PUSHIMAGE` | `CAP_PUSHIMAGE` | `1` | `0x0002` | `ops.def` の `feature_cap_bit` |
| `LGFX_CAP_LAST_ERROR` | `CAP_LAST_ERROR` | `2` | `0x0004` | `ops.def` の `feature_cap_bit` |
| `LGFX_CAP_TOUCH` | `CAP_TOUCH` | `3` | `0x0008` | `ops.def` の `feature_cap_bit` |
| `LGFX_CAP_PALETTE` | `CAP_PALETTE` | `4` | `0x0010` | `ops.def` の `feature_cap_bit` |
| `LGFX_CAP_BATCH` | `CAP_BATCH` | `5` | `0x0020` | `ops.def` の `feature_cap_bit` |
<!-- END:generated_caps_table -->
