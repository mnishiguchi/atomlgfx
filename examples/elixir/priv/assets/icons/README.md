<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# アイコン画像

このディレクトリには、Elixir の例で使う生の RGB565 アイコン画像を置きます。

## ファイル

- `info.rgb565`
- `alert.rgb565`
- `close.rgb565`
- `piyopiyo.rgb565`

## 形式

各ファイルの形式は次のとおりです。

- 生の RGB565 画素データ
- 32 x 32 画素
- 1画素あたり2バイト
- 合計2048バイト
- 通常のリトルエンディアン16ビット RGB565 語として保存

Elixir からは、そのまま渡すことを前提とします。`SampleApp.Assets` でバイト順を入れ替えないでください。

## 実行時契約

`AtomLGFX.push_image_rgb565/8` で読み込む場合は、次の契約に従います。

- 画像バイト列を通常の RGB565 データとして扱う
- 転送前に、転送先スプライトで `set_swap_bytes(port, true, target)` を有効にする
- 後続処理が必要とする場合は、転送後に入れ替え設定を戻す

これは、例で使う確定済み `pushImage` 契約と一致します。

## MovingIcons での扱い

`SampleApp.MovingIcons` は、これらのファイルをアイコン用スプライトへ読み込み、RGB565 の `0x0000` を透過色として使います。

そのため、透過背景が必要な画像では `0x0000` を背景画素用に予約してください。

## 再生成

再生成時も次の契約を維持します。

- 32 x 32
- 生の RGB565
- リトルエンディアン語順
- 透過が必要な場合の背景色は `0x0000`

バイト順が変わると、MovingIcons の色が正しく表示されません。
