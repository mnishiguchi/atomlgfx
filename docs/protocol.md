<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# LovyanGFX AtomVM ポートプロトコル

この文書は、AtomVM のホストアプリケーションとネイティブ `lgfx_port` ドライバーの間で使う組プロトコルを定義します。

リポジトリ全体は [構成](architecture.md)、ポート層の詳細は [`lgfx_port` README](../lgfx_port/README.md)、生成された操作・機能・エラー表は [プロトコル参照](protocol-reference.md) を参照してください。

## 対象範囲

この文書で定義する内容:

- 要求と応答の形
- 外部から見える検証規則
- データ表現
- フラグと機能ビット
- 外部契約に含まれる操作の意味

この文書で定義しない内容:

- `open_port/2` へ渡す開始時設定
- ポート層または装置層の内部実装
- ネイティブ部品内の契約に影響しない実行構造
- 保守者だけが使う試験・同期確認手順

## 正式な定義元

プロトコル契約は、次の情報から定義します。

- `lgfx_port/include_internal/lgfx_port/ops.def`
  - 操作面
  - 操作番号の順序
  - 引数数
  - 許可するフラグ
  - 対象規則
  - 状態規則
  - 機能との関連付け
- `lgfx_port/include_internal/lgfx_port/protocol.h`
  - プロトコル定数
  - 機能ビット
  - ワイヤー単位の上限
- 構築時に生成する `lgfx_port/lgfx_port_config.h`
  - `lgfx_port/cmake/lgfx_port_config.h.in` から生成
  - 部品が使う構築条件
- この文書
  - 人が読む契約
- `docs/protocol-reference.md`
  - ソース情報から同期生成する参照表
- `lib/atom_lgfx/generated.ex`
  - Elixir 操作名から操作番号への生成済み対応
  - 公開・生呼び出し・バッチでの公開方針
  - ラッパーのスキーマが使う引数数、フラグ、対象、状態、機能情報

重要な不変条件:

- `ops.def` に宣言されていない操作はプロトコルに含まれない
- `getCaps` は、情報と有効な振り分け面から `FeatureBits` を導出する
- `FeatureBits` にはプロトコル機能ビットだけを含める
- タッチ操作を構築し、実際にタッチ装置が接続された場合だけタッチ対応を通知する
- 生成参照表と実装は一致する
- 正規の Elixir 操作名は `snake_case` アトム
- LovyanGFX 風の `camelCase` アトムは Elixir 向けプロトコルに含めない
- 同じプロトコル版では、既存操作の数値順序を固定する
- プロトコル版を上げない限り、新しい操作は末尾へ追加する

## 操作名の契約

プロトコル操作には、Elixir 側で使う名前が一つあります。

- 正規の Elixir 名
  - `snake_case` アトム
  - 生成スキーマ、プロトコル符号化器、公開ラッパー、生呼び出しで使用

`ops.def` はハンドラー表生成のため内部で LovyanGFX 風識別子を保持できますが、v3 の要求組には正規の `snake_case` 操作アトムを入れます。ラッパーが所有する生経路向けに、小さな数値操作番号もネイティブ解析器が受け入れます。

規則:

- 公開呼び出しと生呼び出しでは正規の `snake_case` 名を使う
- `:fillRect` などの LovyanGFX 風 `camelCase` アトムは未知の操作として扱う
- 操作番号は `ops.def` の順序だけから導出する
- `elixir scripts/sync_lgfx_protocol_doc.exs` で生成ファイルを更新する
- `mix lgfx.generated.check` で生成差分を検出する
- 操作名または操作番号順を変更する前に、プロトコル固定試験へ合格する

## 要求と応答

v3 要求は平坦な組です。対象を持たない操作では対象情報を省略します。

```erlang
{lgfx, ProtoVer, Op, ...Args}
```

対象を持つ操作では、対象とフラグを組の先頭側へ含めます。

```erlang
{lgfx, ProtoVer, Op, Target, Flags, ...Args}
```

各欄の意味:

- `lgfx`
  - 識別用アトム
- `ProtoVer`
  - 整数
  - `LGFX_PORT_PROTO_VER` と一致する必要がある
- `Op`
  - 正規の `snake_case` 操作アトム
  - ラッパーが所有する生経路向けに数値操作番号も受け入れる
- `Target`
  - `0`: LCD
  - `1..254`: スプライト番号
  - `255`: 予約済みで無効
  - 対象を持たない操作では省略する。ただし生試験が意図的に対象を送る場合を除く
- `Flags`
  - 整数のビット集合
  - 未使用時は `0`
  - 対象を持たない操作では省略する。ただし生試験が意図的に送る場合を除く
- `Args`
  - 操作固有の位置引数
  - v3 では入れ子の引数一覧を使わない

応答は常に次の形です。

```erlang
{ok, Result}
{error, Reason}
```

慣例:

- 返り値のない操作は `{ok, ok}`
- 取得操作は `{ok, Value}`
- 構造化した返り値は組を使う
- `Reason` はアトムまたは詳細組

## プロトコル境界の実行方式

外部から見える操作方式は二つです。

- 通常操作
  - 直ちに実行する
  - 実際の成功または失敗を直ちに返す
- `submitBinaryBatch` によるバイナリーバッチ送信
  - 命令構築は明示的で、呼び出し側が所有する
  - 一つのバイナリーが一つのフレーム命令列を表す
  - 同期的に実行する
  - 成功は、命令列全体の解析と実行が完了したことを示す

この区別は `AtomLGFX.submit_binary_batch/2` の外部契約です。内部の実行時期や実行構造は実装詳細です。

## 検証方式

共通の失敗対応:

- 組の識別子またはプロトコル版が不正: `bad_proto`
- 未知の操作番号: `bad_op`
- 引数数または型が不正: `bad_args`
- 値がワイヤー範囲外: `bad_args`
- 対象が不正: `bad_target`
- 許可されない非零フラグ: `bad_flags`

検証は層ごとに行います。

- ポート層は要求包絡と操作情報を検証する
- ハンドラーは操作固有のワイヤー値を解析する
- 装置側の意味については装置層を正式な判断元とする

装置側で検証する例:

- 転送元・転送先スプライトの存在
- 番号指定色に必要なパレット付きスプライト
- `push_image` の行幅正規化と必要バイト数
- `drawJpg` の復号・描画動作
- 回転と拡大縮小の妥当性
- 決定的なスプライト確保規則

バイナリーバッチ送信では、さらに次を検証します。

- `submit_binary_batch` が外側の要求包絡を検証する
- バイナリーバッチ解析器が各描画命令を解析・検証してから実行する

## バイナリーデータの寿命

呼び出し側のバイナリーデータを指す生ポインターは、要求の間だけ有効です。

規則:

- 寿命を明示的に管理しない限り、要求境界を越えて呼び出し側のバイナリーデータへのポインターを保持しない

現在の通常操作:

- ハンドラーは要求のバイナリーデータへのポインターを借用し、同じ要求内で `lgfx_device_*` へ直接渡す
- 現在の文字・画像装置呼び出しは同期的
- 装置コードは戻る前にバイト列を完全に消費し、呼び出し後にポインターを保持しない

特に `draw_string`、`print`、`println`、`draw_jpg`、`push_image` で重要です。

明示的バッチ:

- `submitBinaryBatch` は同期要求の間だけバイナリーを借用する
- 描画解析器は呼び出し側のバイナリーへのポインターを保持しない
- データを伴うバイナリーバッチ命令は、描画経路内で同期的に解析・消費する
- データを伴う通常操作が自動的にバッチ対応すると仮定しない。文書化された `AtomLGFX.BinaryBatch` 構築器だけを対象とする

## 共通データと表現

### 整数範囲

ドライバーが検証する範囲:

- `i16`: `-32768..32767`
- `i32`: `-2147483648..2147483647`
- `u16`: `0..65535`
- `u32`: `0..4294967295`

一般的な用途:

- `x`, `y`: `i16`
- `w`, `h`: `u16`

### LovyanGFX 風の数値

一部の数値引数は、ワイヤー上で LovyanGFX 風の浮動小数意味を持ちます。

現在の対象:

- `setTextSize`
- `drawJpg` の拡大縮小指定
- `pushRotateZoom`

規則:

- ワイヤーでは整数項と浮動小数項の両方を受け入れる
- ハンドラー層でネイティブ `float` へ変換する
- 値は有限でなければならない
- 倍率は正でなければならない

例:

- `1` は `1.0`
- `1.5` は `1.5`
- `90` は `90.0`

### 文字列

- 文字引数は UTF-8 バイナリー
- 末尾 NUL は不要
- C 文字列 API を使う操作では、埋め込み NUL を拒否する場合がある

### 色

プロトコルでは、関連する次の四つの色領域を区別します。

- 基本図形と文字で使う表示色
- パレットのライフサイクル色
- パレット番号
- `push_image` の画素データを格納したバイナリー

#### 基本図形、文字、番号指定でない透過スプライト操作の表示色

番号指定でない表示色は、ワイヤー上で RGB565 を使います。

- ワイヤー形式は `u16` の RGB565
- ハンドラーは、装置側の基本図形または文字経路で使う表示色として値を渡す
- 対象の色深度にかかわらず、この契約は同じ
- `setColorDepth(Target, 24)` は転送先対象の色深度を変えるが、表示色のワイヤー形式は変えない
- `setColorDepth(Target, 24)` だけではパレット番号の意味を有効にしない

パレット番号モード:

- 操作固有フラグだけで有効にする
- 対応する数値引数をパレット番号として解釈する
- 解析した数値の下位8ビットをパレット番号として使う
- LCD 対象の基本図形・文字色では無効
- スプライト対象では、実際にパレット領域が必要
- 対象の色深度だけでは番号指定の意味を暗黙に有効にしない

対象となる操作例:

- `fillScreen`
- `clear`
- `drawPixel`
- `drawFastVLine`
- `drawFastHLine`
- `drawLine`
- `drawRect`
- `fillRect`
- `drawRoundRect`
- `fillRoundRect`
- `drawCircle`
- `fillCircle`
- `drawEllipse`
- `fillEllipse`
- `drawArc`
- `fillArc`
- `drawBezier`
- `drawTriangle`
- `fillTriangle`
- `setTextColor`
- `pushSprite` の任意透過値
- `pushRotateZoom` の任意透過値

#### パレットのライフサイクル色

パレット操作は、ワイヤー上で RGB888 を直接使います。

- `setPaletteColor` は `u32` の `0x00RRGGBB` 詰め込み RGB888 を受け取る
- パレット操作の引数を RGB565 表示色として再解釈しない
- `createPalette` は既存のパレット色深度スプライトへパレット領域を作る
- `setPaletteColor` は、そのスプライトのパレット項目を一つ書き換える

#### パレット番号

パレット番号は、フラグで明示的に選ぶ引数解釈です。

- 基本図形の番号指定色は `LGFX_F_COLOR_INDEX`
- 文字前景の番号指定色は `LGFX_F_TEXT_FG_INDEX`
- 文字背景の番号指定色は `LGFX_F_TEXT_BG_INDEX`
- スプライト透過番号は `LGFX_F_TRANSPARENT_INDEX`
- 文書で指定する箇所では、番号指定の意味に実際のパレット領域が必要
- 色深度だけでは番号指定の意味を有効にしない

#### `push_image` の画素データを格納したバイナリー

- RGB565 のみ
- 各画素は通常の16ビット RGB565 語としてリトルエンディアン `lo hi`
- `setColorDepth` の影響を受けない
- 対象側のバイト入れ替えは、別途 `setSwapBytes` で制御する

## エラー理由

正規のプロトコルエラーアトムと詳細タグは、[生成エラー参照](protocol-reference.md#生成されたエラー理由) にあります。

任意の詳細形式:

- `{error, {bad_args, Detail}}`
- `{error, {internal, EspErr}}`

利用側の規則:

- `{error, Reason}` に一致させ、`Reason` は不透明な値として扱う

## 操作規則の表記

この表記は `ops.def` と対応します。

### 対象規則

- `T0/bad_target`
  - `Target == 0` を要求し、それ以外は `{error, bad_target}`
- `T0/unsupported`
  - `Target == 0` を要求し、それ以外は `{error, unsupported}`
- `LGFX_OP_TARGET_ANY`
  - LCD またはスプライト対象を受け入れる
  - `255` は無効
- `LGFX_OP_TARGET_SPRITE_ONLY`
  - スプライト対象 `1..254` を要求する

### フラグ規則

- `F0`
  - `Flags == 0` を要求する
- `Fmask(X)`
  - `(Flags & ~X) == 0` を要求する

### 状態規則

- `any`
  - `init` 前にも呼び出せる
- `requires_init`
  - 初期化済み表示状態を要求する

## 実装済み操作表

生成された実装済み操作表は [プロトコル参照](protocol-reference.md#実装済み操作表) にあります。

表にない操作は未実装であり、`{error, bad_op}` を返す必要があります。

## 機能情報

### `getCaps()`

要求:

- `Target == 0` の `getCaps()`

応答:

```erlang
{ok, FeatureBits}
```

`FeatureBits`:

- プロトコル機能のビット集合だけを含む
- 利用側は公開 `supports_*?` 補助関数で分かりやすく確認できる

導出規則:

- `0` から開始する
- `ops.def` に宣言された操作を走査する
- 操作の `feature_cap_bit` が非零で、構築済み振り分け面で有効なら、そのビットを `FeatureBits` へ加える
- 実際の構築条件と実行時条件を適用する
- 既知のプロトコルビットだけに制限して返す

生成された機能語彙は [プロトコル参照](protocol-reference.md#生成された機能語彙) にあります。

意味:

- `CAP_SPRITE`: スプライト操作を利用できる
- `CAP_PUSHIMAGE`: `push_image` を利用できる
- `CAP_LAST_ERROR`: `getLastError` を利用できる
- `CAP_TOUCH`: タッチ操作を利用できる
- `CAP_PALETTE`: `createPalette` と `setPaletteColor` を利用できる
- `CAP_BATCH`: `submitBinaryBatch` / `AtomLGFX.submit_binary_batch/2` を利用できる

タッチに関する注意:

- 構築時にタッチ対応を有効にし、実際に接続されている場合だけ `CAP_TOUCH` を通知する
- `LGFX_PORT_TOUCH_CS_GPIO = -1` で構築すると、タッチは未接続として機能通知しない

## 描画バッチ

`submitBinaryBatch` は v3 のバッチ送信経路です。

スケジューラー、待ち行列、汎用の組・一覧バッチ実行系ではありません。高負荷描画向けに、バイナリーフレーム命令列を明示的に送る入口です。

Elixir 側は `AtomLGFX.BinaryBatch` でフレーム命令列を構築し、`AtomLGFX.submit_binary_batch/2` または `AtomLGFX.BinaryBatch.render/2` で送信します。

生成または試験的な命令列では、次の補助を利用できます。

- `AtomLGFX.BinaryBatch.validate/1`
  - ネイティブコードを呼ばずに命令列を事前検証する
- `AtomLGFX.BinaryBatch.render_checked/2`
  - 検証してから送信する
  - ネイティブの `LGFX_PORT_RENDER_BATCH_PREVALIDATE` が無効でも、部分描画を避ける任意経路になる
- `AtomLGFX.BinaryBatch.summary/1`
  - バッチバイト数、描画専用命令数、動的データ量、固定負担、詰め込み一覧記録量、一覧命令数、一覧要素数、1000倍整数のワイヤー効率比などをネイティブ呼び出しなしで集計する
- `AtomLGFX.BinaryBatch.diagnose/1`
  - 正しい命令列では同じ集計を返し、不正な命令列では失敗命令番号、操作番号、推定操作名、最後に解析成功した命令などの途中情報も返す
- `AtomLGFX.BinaryBatch.compare/2`
  - 基準命令列と候補命令列を同じ指標で比較する
- `AtomLGFX.BinaryBatch.check_budget/2`
  - 呼び出し側が指定した上限に対して診断値を検証し、継続的検証や生成フレームの防護に使う

バイナリーバッチの操作面は意図的に小さく保ちます。汎用基本図形一覧、スプライト領域一覧、バッチ内 JPEG 描画、バッチ内 RGB565 画像転送は v3 のバッチ面に含めません。

## 低メモリーのバッチ描画

メモリー不足の危険を下げ、公開面を簡素化するため、保持型ネイティブ描画プログラム API とネイティブ表示短冊バッチ命令は [v3 低メモリープロトコル ADR](adr/0015-v3-low-memory-protocol.md) で削除しました。

MovingIcons のようなアニメーションは、物体状態を Elixir 側に保持し、計測した高負荷処理だけをアプリケーション固有の描画器へ隔離します。`BinaryBatch` は明示的な同期最適化として残し、現在の例では場面固有のプロトコル状態や隠れ表示バッファを追加せず、汎用の変形スプライト一覧を使います。

### ワイヤー形式

通常の v3 平坦要求を使います。

```erlang
{lgfx, ProtoVer, submit_binary_batch, 0, 0, CommandBinary}
```

規則:

- `Flags` は `0`
- `CommandBinary` は空でない
- `CommandBinary` は `LGFX_PORT_MAX_BINARY_BYTES` 以下
- 対象と色の解釈は、バイナリーバッチ内状態として命令ごとに管理する
- 同期的に実行し、最初の不正命令または失敗で停止する
- 成功は `{ok, ok}`
- 失敗は `{error, Reason}`

各命令:

```text
opcode u8 + 操作番号固有データ
```

不正な描画命令は `bad_args`、未対応の描画命令番号は `bad_op` を返します。

### 命令内の描画状態

バイナリー描画バッチは、フレーム命令列実行中に小さな状態を保持します。

- 現在の対象
  - `target` で選ぶ
  - 既定値は LCD 対象 `0`
- 現在の色モード
  - `colorMode` で選ぶ
  - RGB565 モードは数値色欄を RGB565 として解釈する
  - パレット番号モードは、対応箇所で数値色欄をパレット番号として解釈する

### 代表的な命令配置

```text
target:
  op u8
  target u8

colorMode:
  op u8
  mode u8

fillScreen / clear:
  op u8
  color u16le

fillRect:
  op u8
  x i16le
  y i16le
  w u16le
  h u16le
  color u16le

drawString:
  op u8
  x i16le
  y i16le
  byte_len u16le
  utf8 bytes[byte_len]

pushSprite:
  op u8
  source_target u8
  x i16le
  y i16le

pushRotateZoomList:
  一つの PRZL データを格納したバイナリーを持つ通常操作番号

display:
  op u8
```

数値操作番号の正式な参照元は生成プロトコル参照です。描画専用命令番号は、バイナリー描画バッチの命令列の内部だけで使います。

## 診断

### `getLastError()`

要求:

- `Target == 0` の `getLastError()`

応答:

```erlang
{ok, {last_error, LastOp, Reason, LastFlags, LastTarget, EspErr}}
```

各欄:

- `LastOp`: 最後に失敗した操作アトム。ない場合は `none`
- `Reason`: 最後のエラー理由。ない場合は `none`
- `LastFlags`: 失敗要求のフラグ
- `LastTarget`: 失敗要求の対象
- `EspErr`: `esp_err_t` 整数。ない場合は `0`

動作:

- ドライバーは最後のエラー状態を写し取り、返す
- 成功時は、応答データの符号化後に最後のエラー状態を消去する

バッチに関する注意:

- 現在の実装面には、後からバッチ状態を取得する専用操作がない
- `submit_binary_batch` は同期的なため、後続の状態確認は不要

## 文字種

プロトコルが所有する安定した文字種選択は、次で公開します。

- `setTextFontPreset(PresetIdU8)`

文字種プリセットと文字倍率は別の関心です。

- `setTextFontPreset` は字形元を選ぶ
- `setTextSize` は描画倍率を制御する
- プリセット選択時に文字倍率を `1.0x` へ正規化する

### `setTextSize`

引数:

- `setTextSize(ScaleF32)`
- `setTextSize(ScaleXF32, ScaleYF32)`

規則:

- ワイヤーでは整数項と浮動小数項を受け入れる
- 1引数形は両軸へ同じ倍率を適用する
- 2引数形は両軸を個別に設定する
- 倍率は正でなければならない
- ハンドラーがワイヤー値をネイティブ `float` へ正規化する
- 装置コードが最終値を検証し、固定版 LovyanGFX 呼び出し面へ渡す

エラー:

- 零、負、非有限、型不正: `{error, bad_args}`

### `setTextDatum`

引数:

- `setTextDatum(DatumU8)`

規則:

- `DatumU8` は `0..255` の整数
- 固定版 LovyanGFX の文字基準位置 API へ生の数値として渡す

エラー:

- 範囲外: `{error, bad_args}`

### `setTextWrap`

引数:

- `setTextWrap(WrapXBool)`
- `setTextWrap(WrapXBool, WrapYBool)`

規則:

- アトム `true` / `false` を受け入れる
- ハンドラー解析経路では数値 `0` / `1` も受け入れる
- 1引数形は `wrap_x = WrapXBool`, `wrap_y = false`
- 2引数形は両軸を明示する

### `setTextFontPreset`

プリセット番号:

- `0` = `ascii`
  - 固定版の既定 ASCII 文字種を選ぶ
  - 文字倍率を `1.0` へ正規化する
- `1` = `jp`
  - 組み込みの日本語対応プリセットを選ぶ
  - 文字倍率を `1.0` へ正規化する

エラー:

- 未知のプリセット: `{error, bad_args}`
- 構築から除外されたプリセット: `{error, unsupported}`

## フラグ

特記がない限り、`Flags` の意味は操作ごとに異なります。

定義済みプロトコルフラグ:

- `LGFX_F_TEXT_HAS_BG = 1 bsl 0`
  - `setTextColor` が背景色引数を含む
- `LGFX_F_COLOR_INDEX = 1 bsl 1`
  - 基本図形の色引数を表示色ではなくパレット番号として解釈する
- `LGFX_F_TEXT_FG_INDEX = 1 bsl 2`
  - `setTextColor` の前景色引数をパレット番号として解釈する
- `LGFX_F_TEXT_BG_INDEX = 1 bsl 3`
  - `setTextColor` の背景色引数をパレット番号として解釈する
- `LGFX_F_TRANSPARENT_INDEX = 1 bsl 4`
  - `pushSprite` または `pushRotateZoom` の透過値引数をパレット番号として解釈する

一般規則:

- フラグは `ops.def` の許可マスクに含まれる操作でだけ有効
- 番号指定色フラグは引数解釈を選ぶだけで、パレット領域を作らない
- `LGFX_F_TEXT_BG_INDEX` は `LGFX_F_TEXT_HAS_BG` も設定されている場合だけ有効

### `setTextColor`

引数:

- `setTextColor(FgColor)`
- `LGFX_F_TEXT_HAS_BG` 設定時の `setTextColor(FgColor, BgColor)`

意味:

- 前景色と背景色は、それぞれ表示色またはパレット番号を選べる
- 前景番号モードは `LGFX_F_TEXT_FG_INDEX`
- 背景番号モードは `LGFX_F_TEXT_BG_INDEX`
- 背景引数の有無は `LGFX_F_TEXT_HAS_BG`
- LCD の番号指定色は無効
- スプライトの番号指定色にはパレット付きスプライトが必要

## 重要な操作の意味

### `setColorDepth`

引数:

- `setColorDepth(DepthU8)`

許可値:

- `1`
- `2`
- `4`
- `8`
- `16`
- `24`

意味:

- 転送先対象の色深度を変更する
- 番号指定でない表示色のワイヤー形式は変えない
- 番号指定数値色の意味を単独では有効にしない
- スプライトのパレット領域を単独では作らない
- 対象の色深度にかかわらず `push_image` は RGB565 だけを扱う

### `drawJpg`

要求引数:

- `drawJpg(Xi16, Yi16, JpegBinary)`
- `drawJpg(Xi16, Yi16, MaxWu16, MaxHu16, OffXi16, OffYi16, ScaleXF32, ScaleYF32, JpegBinary)`

規則:

- 最後の引数はバイナリー
- 短縮形は `MaxW = 0`, `MaxH = 0`, `OffX = 0`, `OffY = 0`, `ScaleX = 1.0`, `ScaleY = 1.0`
- 拡張形の倍率は整数項と浮動小数項を受け入れる
- 倍率は有限かつ正
- 対象は LCD `0` またはスプライト `1..254`

### `push_image`

要求引数:

- `push_image(Xi16, Yi16, Wu16, Hu16, StridePixelsU16, DataRgb565Binary)`

規則:

- `W > 0`
- `H > 0`
- 最後の引数はバイナリー
- `StridePixelsU16 == 0` の場合、有効行幅を `W` とする
- 有効行幅は `W` 以上
- データのバイト数は偶数
- 要求画像を格納できる大きさが必要
- 必要最小量を越える末尾バイトは無視する

### `createSprite`

要求ヘッダーの `Target` が確保するスプライト番号です。

- `1..254`: 候補番号
- `0`: 無効

引数:

- `createSprite(Wu16, Hu16)`
- `createSprite(Wu16, Hu16, ColorDepthU8)`

規則:

- 指定番号へ確保する
- `W` と `H` は非零
- 任意の色深度は有効値でなければならない
- 指定番号が使用中なら失敗する
- 同時スプライト上限へ達している場合は失敗する
- パレット色深度は `1`, `2`, `4`, `8`
- 真彩色色深度は `16`, `24`
- パレット色深度だけではパレット領域を作らない

### `createPalette` と `setPaletteColor`

スプライト対象のパレット領域を管理します。

要求ヘッダーの `Target` はスプライト番号です。

- `1..254`: 候補番号
- `0`: 無効

`createPalette` の引数:

- なし

`setPaletteColor` の引数:

- `setPaletteColor(PaletteIndexU8, Rgb888U32)`

規則:

- どちらもスプライト専用
- 対象スプライトは作成済みでなければならない
- `createPalette` は色深度 `1`, `2`, `4`, `8` を要求する
- `createPalette` は対象スプライトへパレット領域を作る
- 番号指定数値色には実際のパレット領域が必要
- `setPaletteColor` はパレット付きスプライトを要求する
- `Rgb888U32` は `0x00RRGGBB`
- 有効なパレット番号範囲はスプライトの色深度による

### `pushSprite`

転送先を指定するスプライト全体の転送です。

要求ヘッダーの `Target` は転送元スプライト番号です。

- `1..254`: 有効
- `0`: 無効

引数:

- `pushSprite(DstTargetU8, DstXi16, DstYi16)`
- `pushSprite(DstTargetU8, DstXi16, DstYi16, TransparentValue)`

規則:

- `DstTargetU8 == 0`: LCD へ転送
- `DstTargetU8 in 1..254`: スプライトへ転送
- 転送元と転送先の存在は装置層で解決する
- 任意の透過値は既定で番号指定でない表示色契約を使う
- `LGFX_F_TRANSPARENT_INDEX` は透過値をパレット番号として解釈する
- 透過番号モードには転送元スプライトのパレット領域が必要
- 端での切り取りを許可する

領域指定のスプライト転送操作はありません。

### `pushRotateZoom`

転送元スプライトを回転・拡大縮小して転送先へ描画します。

要求ヘッダーの `Target` は転送元スプライト番号です。

引数:

- `pushRotateZoom(DstTargetU8, DstXi16, DstYi16, AngleDegF32, ZoomXF32, ZoomYF32)`
- `pushRotateZoom(DstTargetU8, DstXi16, DstYi16, AngleDegF32, ZoomXF32, ZoomYF32, TransparentValue)`

規則:

- 転送先対象の規則は `pushSprite` と同じ
- `setPivot` で設定した転送元スプライトの基準点を使う
- 転送元と転送先の存在は装置層で解決する
- 角度と倍率は整数項と浮動小数項を受け入れる
- 角度と倍率は有限
- 倍率は正
- 任意の透過値は既定で番号指定でない表示色契約を使う
- `LGFX_F_TRANSPARENT_INDEX` は透過値をパレット番号として解釈する
- 透過番号モードには転送元スプライトのパレット領域が必要
- 端での切り取りを許可する

### `pushRotateZoomList`

多数の変形スプライトを一つの転送先へ描画する、小さな高負荷向けバイナリー経路です。

要求ヘッダーの `Target` は転送先です。

- `0`: LCD
- `1..254`: スプライト

引数:

- `pushRotateZoomList(PayloadBinary)`

データ配置はリトルエンディアンです。

```text
magic             bytes[4] = "PRZL"
version           u8 = 1
options           u8
transparent       u16
y_offset          i16
instance_count    u16
InstanceRecord    instance_count * 12 bytes

InstanceRecord:
  src_target      u8
  reserved        u8 = 0
  x               i16
  y               i16
  angle_cdeg      u16
  zoom_x1024      u16
  zoom_y1024      u16
```

規則:

- `options & 0x01` は `transparent` が存在することを示す
- 未知の選択ビットは無効
- `LGFX_F_TRANSPARENT_INDEX` 設定時、`transparent` の下位バイトをパレット番号として使う
- 透過選択なしの `LGFX_F_TRANSPARENT_INDEX` は無効
- `x` と `y` はネイティブの `y_offset` 調整前の転送先座標
- ネイティブコードは各要素の `y` から `y_offset` を引く
- `angle_cdeg` は100分の1度で、`0..35999`
- `zoom_x1024` と `zoom_y1024` は正の固定小数倍率で、`1024 == 1.0x`
- `reserved` は `0`
- データ長は `12 + instance_count * 12` と完全に一致する
- 転送元と転送先の存在は装置層で解決する

## 互換性規則

次の変更はプロトコルへ影響するため、変更時に `LGFX_PORT_PROTO_VER` を上げます。

- 要求組の形を変える
- 応答の形を変える
- 操作の意味を変える
- 既存操作の数値順序を変える
- 引数順を変える
- 引数解釈を変える
- フラグの意味を変える
- 受け入れるワイヤー表現を変える
- 既存契約違反に対する正規エラー理由を変える
- 実装済み操作をプロトコル面から削除する

通常、プロトコル版を上げない変更:

- 外部契約を維持する内部整理
- 要求・応答面を変えない実装変更
- 意味を変えない文書の明確化
- 通常の機能照会で保護した新しい操作の追加
- 既存の不透明エラー一致を維持した内部詳細の追加
- `lib/atom_lgfx/generated.ex` の再生成
  - Elixir `snake_case` 操作名
  - Elixir 操作名から操作番号への対応
  - 公開・生呼び出し・バッチでの公開方針
