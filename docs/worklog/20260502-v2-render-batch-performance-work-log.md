<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# v2 描画バッチ性能作業記録

## 背景

本記録は、[ADR 0009: v2 の高頻度描画をバイナリー描画バッチへ統一する](../adr/0009-binary-batch-for-native-like-animation.md) に続いて実施した MovingIcons の性能改善作業をまとめたものです。

ADR の判断は変わりません。v2 の高頻度描画では、アニメーションの1フレームを1回で送信し、ネイティブ側で実行できるよう、バイナリー描画バッチを使用します。

以下の測定結果は、描画バッチによって解消できた問題と、描画方式として引き続き改善が必要な点を明確にします。

## 測定条件

確認した記録の形式は次のとおりです。

```text
moving_icons stats obj_count=50 render_mode=... submit_mode=binary_batch draw_mode=... fps=... frame_ms=... batch_bytes=... strip_count=...
```

特記がない限り、記録には次の値を使用しました。

- `obj_count=50`
- `submit_mode=binary_batch`

## 経過

| 段階 | 描画方式 | 描画命令 | フレーム時間 | バッチ容量 | 帯数 | 補足 |
| --- | --- | --- | ---: | ---: | ---: | --- |
| 最初の描画バッチ | `strip_buffers` | `push_rotate_zoom_list` | 758-760 ms | 1289 | 2 | 1フレーム1送信は実現したが、各帯に全レコードを送っていた。 |
| 帯単位の事前除外 | `strip_buffers` | `push_rotate_zoom_list` | 653-683 ms | 845-929 | 2 | Elixir 側で帯に不要なレコードを除外し、送信量とフレーム時間を削減した。 |
| ネイティブ PRZL 高速実行 | `strip_buffers` | `push_rotate_zoom_list` | 659-684 ms | 881-941 | 2 | 描画先を1回だけ解決し、描画元の検索結果を保持しても、実行時間はほぼ改善しなかった。 |
| 描画命令別測定の基準 | `strip_buffers` | `push_rotate_zoom_list` | 642-658 ms | 833-881 | 2 | 描画命令を明示的に記録するようにした後の基準値。 |
| スプライト全体の一覧転送 | `strip_buffers` | `push_sprite_list` | 529-533 ms | 445-457 | 2 | 動的変形より高速だが、ネイティブ相当の FPS には届かなかった。 |
| 描画元領域の一覧転送 | `strip_buffers` | `push_sprite_region_list` | 576-581 ms | 1067-1081 | 2 | 画像集向けの基本命令として有用だが、当初はスプライト全体の一覧転送より遅かった。 |
| 連続領域の高速経路 | `strip_buffers` | `push_sprite_region_list` | 565-572 ms | 1025-1067 | 2 | 行単位転送の負荷を減らしたが、主なボトルネックではなかった。 |
| 描画元全体の高速経路 | `strip_buffers` | `push_sprite_region_list` | 552-563 ms | 955-1011 | 2 | 描画元全体のレコードはスプライト全体の一覧転送に近づいたが、帯バッファーの費用は残った。 |
| LCD 直接描画との比較 | `direct_lcd` | `push_rotate_zoom_list` | 637 ms | 653 | 1 | 毎フレーム表示全体を消去するため、性能基準としては不適切。 |
| LCD 直接描画との比較 | `direct_lcd` | `push_sprite_list` | 455 ms | 345 | 1 | 全画面への直接描画でもネイティブ相当にならないことを示す比較に限り有用。 |
| LCD 直接描画との比較 | `direct_lcd` | `push_sprite_region_list` | 484 ms | 745 | 1 | 診断用の比較に限り有用。 |
| 公開帯バッファーとの比較 | `strip_buffers` | `push_rotate_zoom_list` | 641 ms | 833 | 2 | ネイティブ帯表示の導入前。 |
| 公開帯バッファーとの比較 | `strip_buffers` | `push_sprite_list` | 539 ms | 487 | 2 | ネイティブ帯表示の導入前。 |
| 公開帯バッファーとの比較 | `strip_buffers` | `push_sprite_region_list` | 564 ms | 1025 | 2 | ネイティブ帯表示の導入前。 |

## 分かったこと

- 描画バッチにより、フレーム処理内で繰り返していた AtomVM ポート呼び出しを除去できた。これは v2 に引き続き必要である
- 変形レコードの送信数を減らすと改善したため、帯単位の事前除外は有効である
- `push_rotate_zoom_list` 内で描画元と描画先の検索を最適化しても大きな改善はなく、検索や命令振り分けの繰り返しは主なボトルネックではなかった
- `push_sprite_list` は `push_rotate_zoom_list` より高速であり、動的変形の費用が無視できないことを確認できた
- `push_sprite_list` の改善幅は限定的であり、帯の表示、スプライト転送、メモリー帯域、または表示器への転送量も重要だと考えられる
- `push_sprite_region_list` は画像集形式の描画に有用だが、スプライト全体の転送と競合するには、配置の工夫とネイティブ高速経路が必要である
- `direct_lcd` は現在、毎フレーム表示全体を消去する。アニメーション性能の主基準ではなく、正しさの確認や診断用として扱う

## 検証の改善

この作業では、バイナリーバッチの通信契約を厳格にし、状態を観測しやすくすることに注力しました。

完了した項目:

- 途中で切れた命令に対する不正バイナリーバッチ検証
- 未知の操作コードに対する不正バイナリーバッチ検証
- 不正な色形式に対する検証
- 不正なスプライト一覧フラグに対する検証
- 不正な領域一覧データに対する検証
- 不正な回転・拡大縮小一覧データに対する検証
- 描画バッチ命令の復号・概要検証
- 一覧命令と帯命令の概要件数
- すでに構築済みのバッチバイナリーを直接扱う高速経路

## ネイティブ帯表示

次の最適化では、`strip_buffers + binary_batch` 方式から、公開フレームバッファー用スプライトの転送を除きます。

想定するフレームの形は次のとおりです。

```elixir
[
  AtomLGFX.BinaryBatch.begin_strip(y0),
  AtomLGFX.BinaryBatch.target(0),
  AtomLGFX.BinaryBatch.clear(background_color),
  draw_commands,
  overlay_commands,
  AtomLGFX.BinaryBatch.present_strip()
]
```

重要な検証項目は帯の高さです。メモリーが不足する場合、ネイティブ側は希望値より小さな帯を確保する可能性があります。Elixir 側の帯処理は固定値ではなく、ネイティブ側と合意した帯の高さを使用しなければなりません。

## 次回の測定組み合わせ

ネイティブ帯表示を接続した後は、有用な帯バッファーの組み合わせだけを再測定します。

| 描画方式 | 送信方式 | 描画命令 |
| --- | --- | --- |
| `strip_buffers` | `binary_batch` | `push_rotate_zoom_list` |
| `strip_buffers` | `binary_batch` | `push_sprite_list` |
| `strip_buffers` | `binary_batch` | `push_sprite_region_list` |

記録する値:

- `fps`
- `frame_ms`
- `batch_bytes`
- `strip_count`
- ネイティブ側の帯の高さ

## 次の診断

ネイティブ帯表示の導入後もフレーム時間が長い場合は、次の箇所へネイティブ時間計測を追加します。

- 描画バッチ振り分け全体
- 事前検証
- 帯の消去
- スプライト一覧描画
- 回転・拡大縮小一覧描画
- 帯の表示
- `startWrite` / `endWrite` の範囲

次のボトルネックが命令の復号、LovyanGFX の描画、メモリー転送、LCD 転送のどこにあるかを特定することが目的です。

## 後日の結論

本記録は、基本命令による描画バッチ改善の測定経過として残します。現在の v2 高頻度描画に関する判断は次のとおりです。

- [ADR 0011: BinaryBatch を小さく保ち、測定に基づいて拡張する](../adr/0011-keep-binary-batch-minimal-and-measured.md)
- [ADR 0012: 高頻度アニメーション処理にネイティブのフレーム描画命令を認める](../adr/0012-native-frame-render-commands-for-hot-animation.md)

その後の測定から、基本命令の描画バッチは必要である一方、最も高頻度なアニメーション処理では、それだけで十分とは限らないことが分かりました。MovingIcons のような処理では、アニメーション状態を Elixir 側に保ちつつ、選択した汎用ネイティブ描画命令が帯描画の密な繰り返し処理を担当する方針を採用しました。

v2 プロトコルを固定する前に、スプライト全体の一覧転送、スプライト領域の一覧転送、汎用基本図形一覧などの試験的な圧縮一覧命令は、現行バッチ API から削除しました。測定値は経緯として有用ですが、維持する v2 プロトコルには含まれません。
