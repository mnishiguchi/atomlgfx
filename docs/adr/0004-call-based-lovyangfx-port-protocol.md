<!--
SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi

SPDX-License-Identifier: Apache-2.0
-->

# ADR 0004: 呼び出し方式の LovyanGFX ポートプロトコル

## 状態

採用

この ADR は、v2 の単一命令呼び出しプロトコルに関する現行の判断です。

描画一括実行の詳細は、[ADR 0010: BinaryBatch を標準の描画トランザクション API とする](0010-binary-batch-as-render-transaction-api.md)によって置き換えられました。現在の v2 では、初期案のタプル／一覧方式ではなく、`submitBinaryBatch` を明示的なバイナリーフレームスクリプトの入口として使用します。

## 背景

このプロジェクトは、AtomVM 上で動作する Elixir コードから LovyanGFX の機能を利用できるようにする AtomVM 向け ESP-IDF 構成要素です。

既存の `atomlgfx` 実装によって、この構想が成立することは確認できています。AtomVM から LovyanGFX を呼び出し、ESP32 機器へ描画できます。一方で、上流の LovyanGFX ライブラリーを薄く包むことが本来の目的であるにもかかわらず、現在のネイティブ実装は望ましい規模を超えて大きくなっています。

複雑さの主因は、LovyanGFX の各操作ごとに次のようなネイティブ側の定型処理が繰り返されることです。

- 要求の復号
- 引数の検証
- 命令の振り分け
- 対象の解決
- LovyanGFX 関数の呼び出し
- 応答の符号化
- 任意の一括実行処理

単純な描画操作であっても、これによって C/C++ 側の実装範囲が大きくなります。

書き直しの目的は、単一の汎用呼び出し方式プロトコルを採用し、AtomVM ポートの接続面を単純化するとともに、ネイティブコードを減らすことです。

Elixir 側の API は、Elixir らしく、性能にも配慮した形にします。

```elixir
AtomLGFX.call(port, :draw_line, [0, 0, 120, 80, 0xFFFF])
AtomLGFX.call(port, :fill_rect, [10, 10, 80, 40, 0x07E0])
AtomLGFX.call(port, :set_rotation, [1])
```

その上に、使いやすい補助関数を用意しても構いません。

```elixir
AtomLGFX.draw_line(port, 0, 0, 120, 80, 0xFFFF)
AtomLGFX.fill_rect(port, 10, 10, 80, 40, 0x07E0)
AtomLGFX.set_rotation(port, 1)
```

LovyanGFX 自体は、`drawLine`、`fillRect`、`setRotation` のような C++ 形式の関数名を使用します。通常の Elixir API で、これらの名前をそのまま公開してはいけません。Elixir 層では `snake_case` の名前を使用し、AtomVM ポート境界を越える前に、生成済みの数値操作コードへ変換します。

境界の責務は明確に分けます。

- Elixir は、API の正しさとプロトコル方針を検証する。
- ネイティブは、異常終了の防止、実機状態、データ所有権を検証する。

Elixir 層が正しいプロトコルデータを構築し、ネイティブ層は意図的に薄く保ちます。

## 判断

呼び出し方式の LovyanGFX ポートプロトコルを採用します。

Elixir の公開 API では、`snake_case` の操作名をアトムとして使用します。

```elixir
AtomLGFX.call(port, :fill_rect, [10, 10, 80, 40, color])
```

要求をネイティブコードへ送る前に、Elixir 層が操作名アトムを数値操作コードへ変換します。

ネイティブの通信プロトコルでは、次の汎用要求形式を使用します。

```erlang
{lgfx, ProtocolVersion, call, OpCode, Target, Flags, Args}
```

例:

```erlang
{lgfx, 2, call, 1, 0, 0, [10, 10, 80, 40, 2016]}
```

この例では、操作コード `1` が `:fill_rect` を表すものとします。

Elixir 層は、正本となる操作定義、引数の正規化、公開／低水準 API の方針、操作コードの対応付けを管理します。ネイティブ層は、最小限の防御的検査、ネイティブ側でしか判断できない状態の検証、LovyanGFX への振り分けを行います。

これにより、次を実現します。

- Elixir らしい API 名
- 小さな通信データ
- 効率的なネイティブ振り分け
- ネイティブ側のプロトコル知識を最小化
- 通常呼び出しと一括実行で共通する単一の要求形式

## 責務

### Elixir 側の責務

Elixir 層は、次を担当します。

- 公開 API の提供
- 正本となる操作定義の管理
- Elixir らしい `snake_case` の操作名の使用
- 公開、低水準限定、内部限定、タプル一括実行対応、バイナリー限定など、各操作の方針決定
- 利用者入力の正規化
- 引数の個数と形の検証
- 色をネイティブ側が期待する表現へ変換
- 正規化済み入力から操作フラグを構築
- 操作名から数値操作コードへの対応付け
- ポート要求タプルの構築
- 一括実行データの構築
- 不正な公開一括実行をポート境界の手前で拒否
- 危険な公開 API の利用方法を拒否
- 分かりやすいエラーの提供
- 共通定義から定型的な補助関数や付随情報を生成または導出

低水準 API の例:

```elixir
def call(port, op_name, args \\ [], opts \\ []) when is_atom(op_name) do
  opcode = Protocol.opcode!(op_name)
  target = Keyword.get(opts, :target, 0)
  flags = Keyword.get(opts, :flags, 0)

  Protocol.call(port, opcode, target, flags, args)
end
```

任意の補助関数の例:

```elixir
def draw_line(port, x0, y0, x1, y1, color, opts \\ []) do
  call(port, :draw_line, [x0, y0, x1, y1, color], opts)
end
```

操作名には、既知のアトムだけを使用します。利用者から渡された文字列をもとに、アトムを動的生成してはいけません。

適切:

```elixir
Protocol.opcode!(:fill_rect)
```

不適切:

```elixir
String.to_atom(user_input)
```

### ネイティブ側の責務

ネイティブ層は、次を担当します。

- 共通要求包絡の復号
- プロトコル識別子と版の確認
- 操作コード範囲と対象値の表現可能性の確認
- 数値項と借用バイナリーポインターの安全な取得
- 過大なバイナリーや不正な要求項の拒否
- 対象ディスプレーまたはスプライトの解決
- 初期化済み／未初期化など、ネイティブ状態の検証
- 対象やスプライトの存在、実際の対象状態に対するデータ寸法など、実機状態の検証
- データ所有権と要求の生存期間に関する規則の適用
- 数値操作コードによる振り分け
- 対応する LovyanGFX 関数の呼び出し
- 応答の符号化
- 不正なデータによるネイティブ異常終了の防止

ネイティブ層は、公開 API の方針や利用者向けの検証を管理せず、完全なプロトコル定義も重複保持しません。未定義動作を防ぎ、Elixir からは分からない実際のネイティブ状態を扱うために必要な防護だけを保持します。

ネイティブ振り分けの例:

```cpp
esp_err_t lgfx_dispatch(
    lgfx_context_t *ctx,
    uint16_t opcode,
    uint8_t target,
    uint32_t flags,
    const lgfx_arg_list_t *args,
    lgfx_reply_t *reply)
{
    auto *gfx = lgfx_resolve_target(ctx, target);

    switch (opcode) {
        case LGFX_OP_DRAW_LINE:
            gfx->drawLine(
                args->i32(0),
                args->i32(1),
                args->i32(2),
                args->i32(3),
                args->u16(4));
            return ESP_OK;

        case LGFX_OP_FILL_RECT:
            gfx->fillRect(
                args->i32(0),
                args->i32(1),
                args->i32(2),
                args->i32(3),
                args->u16(4));
            return ESP_OK;

        case LGFX_OP_SET_ROTATION:
            gfx->setRotation(args->u8(0));
            return ESP_OK;

        default:
            return ESP_ERR_NOT_FOUND;
    }
}
```

ネイティブ側に振り分け処理は残りますが、意図的に薄く保ちます。LovyanGFX の操作ごとに個別の処理ファイルや機器包みを作ることはしません。

## プロトコル形式

### 直接呼び出し

```erlang
{lgfx, 2, call, OpCode, Target, Flags, Args}
```

各要素:

- `lgfx`: プロトコル識別子
- `2`: プロトコル版
- `call`: 要求種別
- `OpCode`: 既知の Elixir 操作名から生成した数値操作コード
- `Target`: 対象ディスプレーまたはスプライトの識別子
- `Flags`: 操作フラグ
- `Args`: 引数一覧

Elixir 呼び出しの例:

```elixir
AtomLGFX.call(port, :fill_screen, [0])
```

ネイティブ通信データの例:

```erlang
{lgfx, 2, call, 4, 0, 0, [0]}
```

### 一括呼び出し

実装済みの v2 一括実行経路は、通常の呼び出し形式を使う操作です。その引数として、数値命令を詰めたバイナリー命令列を渡します。

Elixir 側の表現例:

```elixir
batch =
  IO.iodata_to_binary([
    AtomLGFX.BinaryBatch.fill_screen(0),
    AtomLGFX.BinaryBatch.draw_line(0, 0, 120, 80, 65535),
    AtomLGFX.BinaryBatch.fill_rect(10, 10, 80, 40, 2016)
  ])

AtomLGFX.submit_binary_batch(port, batch, 0)
```

ネイティブ通信データの例:

```erlang
{lgfx, 2, call, SubmitBinaryBatchOpCode, 0, 0, [CommandBinary]}
```

詰め込み命令列では、対応範囲に含まれる通常の数値操作コードを再利用します。ただし、汎用的なタプル／一覧方式の一括実行環境ではありません。ネイティブ側の一括実行処理は、不正データの防御、未対応操作コードの拒否、機器状態の検証、データ生存期間の管理、同期的で効率的な命令実行に限定します。

## 操作名

Elixir 側の API では、`snake_case` の操作名を使用します。

LovyanGFX 自体は、`drawLine`、`fillRect`、`setRotation` のような C++ 形式の関数名を使用します。通常の Elixir API で、これらを直接公開してはいけません。

代わりに、Elixir 層では次のような Elixir らしい操作名を使用します。

- `:draw_line`
- `:fill_rect`
- `:set_rotation`
- `:set_text_color`
- `:push_image`

これらの操作名は、AtomVM ポート境界を越える前に、生成済みの数値操作コードへ変換します。

例:

```elixir
AtomLGFX.call(port, :draw_line, [0, 0, 120, 80, color])
```

この呼び出しは数値操作コードへ符号化され、最終的に次へ振り分けられます。

```cpp
gfx->drawLine(...)
```

これにより、内部では LovyanGFX との直接的な対応を保ちながら、公開 API は Elixir らしくできます。

## 公開 API の安全方針

公開 Elixir API では、LovyanGFX の全関数をそのまま公開しません。

ネイティブプロトコルが呼び出し方式であっても、公開 API は既知の不適切な利用方法を防ぐ必要があります。特に、AtomVM ポート境界を越えるには細かすぎる操作を、通常の公開補助関数として提供してはいけません。

`drawPixel` や `writePixel` のような画素単位の操作は、Elixir から非効率な繰り返しを行いやすいため、主要 API から意図的に除外します。

次のような公開補助関数は避けます。

```elixir
AtomLGFX.draw_pixel(port, x, y, color)
```

この種の API があると、次のようなコードを容易に書けてしまいます。

```elixir
for x <- 0..319, y <- 0..239 do
  AtomLGFX.draw_pixel(port, x, y, color)
end
```

これは画素ごとに AtomVM ポート境界を越えるため、この API には適していません。

細かな描画操作を繰り返す場合は、次のいずれかを使用します。

- `fill_rect` や `draw_line` のような大きな描画単位
- 一括実行
- スプライト
- 画像バッファー
- ネイティブ側の補助操作

低水準 API は、明示的な退避口として用意しても構いません。ただし、通常の公開 API とは分離します。

例:

```elixir
AtomLGFX.fill_rect(port, 0, 0, 320, 240, color)
```

こちらを推奨します。

```elixir
AtomLGFX.Raw.call(port, :draw_pixel, [x, y, color])
```

必要であれば、低水準 API を通した場合だけ許可します。

一括実行 API も、既定では危険な操作を拒否します。多数の単画素操作を含む一括実行は検証で失敗させ、`push_image` などのバッファー指向 API を案内します。

## 性能上の考慮

通常の LovyanGFX 操作では、ディスプレー入出力が総処理時間の大半を占めるため、呼び出し方式のプロトコルで十分と見込まれます。`fill_screen`、`fill_rect`、`draw_line`、`draw_string`、`push_image` などは、多くの場合、プロトコル振り分けよりもディスプレー転送や LovyanGFX 内部処理に時間を要します。

一方、Elixir から画素単位の頻繁な処理に使用してはいけません。細かな操作を繰り返すと、呼び出しごとに AtomVM ポート境界を越え、項を復号する必要があるため高コストです。

公開 Elixir API では、操作名をアトムとして使用できます。

```elixir
AtomLGFX.call(port, :draw_line, [0, 0, 120, 80, 0xFFFF])
```

ネイティブ側の性能を保つため、通信プロトコルでは文字列ではなく、生成済みの数値操作コードを使用します。

```erlang
{lgfx, 2, call, OpCode, Target, Flags, Args}
```

Elixir 層が操作名を操作コードへ変換します。これにより、ネイティブ層は文字列比較を繰り返さず、小さな `switch` 文で振り分けられます。

複数の数値操作をまとめた呼び出し負荷を減らす主要な制御方法として、詰め込みバイナリーによる一括実行を使用します。同種の処理を繰り返す高頻度描画では、タプル／一覧方式の一括実行環境を拡張するのではなく、固定配置のバイナリー操作、スプライト、画像バッファー、ネイティブ側の表示補助処理を優先します。

初期の性能規則:

- 大きな描画単位であれば直接呼び出しを許可する
- 細かな数値呼び出しの繰り返しは、詰め込みバイナリー一括実行へまとめるか、バッファー指向操作へ置き換える
- Elixir からの画素単位ループは API 設計によって避ける
- 危険な画素単位操作は主要公開 API へ出さない
- データを伴う操作は、明確なメモリー所有権を定義する
- 文字列振り分けより生成済み操作コードを優先する
- 同種のアニメーションデータを繰り返す場合は、固定配置のバイナリーデータを優先する

## データを伴う操作

次のように、文字列またはバイナリーデータを伴う操作があります。

- `draw_string`
- `print`
- `println`
- `push_image`
- 画像描画関数

初期実装では、データを伴う操作は直接呼び出しだけで許可しても構いません。

データを伴う操作の一括実行対応は、データ所有権を明確にしてから追加します。ネイティブ側は、現在の要求の生存期間を超えて AtomVM バイナリーへの借用ポインターを保持してはいけません。

初期規則:

```text
直接呼び出し:
  データを伴う操作を許可する。

一括呼び出し:
  データを実行環境所有のメモリーへ複製しない限り、データを伴う操作を拒否する。
```

## 低水準 API

明示的な退避口として、低水準 API を用意しても構いません。

例:

```elixir
AtomLGFX.Raw.call(port, :draw_pixel, [x, y, color])
```

低水準 API は、通常のアプリケーション API ではありません。実験、調査、高度な利用のために設けます。

主要 API から意図的に除外した操作を低水準 API で公開しても構いませんが、モジュール名、文書、検証動作を通じて、その代償が明確に分かるようにします。

最小構成では、低水準 API を無効化または省略しても構いません。

## コード生成

長期的には、操作定義を単一の正本に置き、そこから定型コードを生成する方針を推奨します。

定義元の例:

```elixir
@ops [
  fill_screen: [
    opcode: 1,
    native: :fillScreen,
    c_function: :lgfx_device_fill_screen,
    args: [:rgb565],
    public: true,
    raw: true,
    direct: true,
    tuple_batchable: true
  ],
  draw_line: [
    opcode: 2,
    native: :drawLine,
    c_function: :lgfx_device_draw_line,
    args: [:i16, :i16, :i16, :i16, :rgb565],
    public: true,
    raw: true,
    direct: true,
    tuple_batchable: true
  ],
  fill_rect: [
    opcode: 3,
    native: :fillRect,
    c_function: :lgfx_device_fill_rect,
    args: [:i16, :i16, :u16, :u16, :rgb565],
    public: true,
    raw: true,
    direct: true,
    tuple_batchable: true
  ],
  set_rotation: [
    opcode: 4,
    native: :setRotation,
    c_function: :lgfx_device_set_rotation,
    args: [:u8],
    public: true,
    raw: true,
    direct: true,
    tuple_batchable: false
  ],
  draw_pixel: [
    opcode: 100,
    native: :drawPixel,
    c_function: :lgfx_device_draw_pixel,
    args: [:i16, :i16, :rgb565],
    public: false,
    raw: true,
    direct: true,
    tuple_batchable: false
  ]
]
```

生成物の例:

```text
lib/atom_lgfx/generated/ops.ex
lib/atom_lgfx/generated/wrappers.ex
lgfx_port/generated/opcodes.h
lgfx_port/generated/simple_dispatch.cpp
```

単純な数値操作は生成するか、機械的な振り分けへ集約します。手書きのネイティブコードは、状態を持つ操作、データを伴う操作、ライフサイクル、スプライト、画像、文字列データ、一括実行環境、ネイティブ側の表示補助処理に残します。

これにより、手書きのネイティブコードを API 接続の定型処理ではなく、実行時の責務へ集中させられます。

## 影響

### 良い影響

- AtomVM ポートの接続面が単純になる。
- ネイティブ C/C++ の実装範囲が減る。
- Elixir 層がプロトコルの正本になる。
- 公開 Elixir API を Elixir らしく保てる。
- Elixir では `snake_case` の操作名を使用できる。
- 数値操作コードにより、ネイティブ通信プロトコルを小さく保てる。
- ネイティブ側は文字列比較ではなく `switch` で振り分けられる。
- LovyanGFX 操作を追加するときのネイティブ定型コードが減る。
- 単純な数値直接呼び出しとタプル一括実行命令を同じ定義から生成できる。
- 公開 Elixir 補助関数を使いやすく保ちつつ、ネイティブ API の重複を避けられる。
- 既知の性能上の落とし穴を主要 API から禁止できる。
- 第2の描画抽象層を作らず、LovyanGFX に近い実装を保てる。

### 悪い影響

- C++ 関数を名前で動的に呼び出せないため、ネイティブ側に振り分け処理は必要になる。
- 操作コードの対応は、生成または慎重な保守が必要になる。
- Elixir 側の検証を迂回した不正データでは、低水準なネイティブエラーが返る場合がある。
- LovyanGFX の一部の多重定義には、明示的なプロトコル上の選択が必要になる。
- 文字列、バイナリー、画像バッファーのデータ所有権を慎重に扱う必要がある。
- 対応する LovyanGFX API が増えるにつれて、コード生成が必要になる可能性がある。
- 生成を導入するまでは、Elixir とネイティブ間で操作情報がずれる可能性が残る。
- 低水準 API を不用意に公開すると、誤用される可能性がある。

### 中立的な影響

- 完全に型付けされたネイティブ結合ではなく、薄い橋渡しを意図的に優先する。
- ネイティブ検証は防御的なものであり、正本ではない。
- このプロトコルは、伝統的な Elixir 包みより遠隔手続き呼び出しに近い。
- 公開 API は選別され、LovyanGFX API を一対一では再現しない。

## 検討した代替案

### LovyanGFX 操作ごとに1つのネイティブ関数を作る

以前の実装で採用していた方法です。

操作数が少ないうちは明示的で理解しやすい一方、API 範囲が広がるにつれて、ネイティブ側の定型コードが増えすぎます。

今回の書き直しではネイティブ定型コードの削減を目指すため、採用しません。

### 通信上で文字列の操作名を使用する

通信プロトコルで、操作名を文字列またはアトムとして送る方法です。

```erlang
{lgfx, 2, call, fillRect, 0, 0, [10, 10, 80, 40, 2016]}
```

読みやすい一方、名前処理がネイティブ側へ寄り、振り分けで文字列またはアトム比較を行いやすくなります。

操作集合は固定かつ既知であり、数値操作コードの方が小さく効率的に振り分けられるため、採用しません。

### Elixir で LovyanGFX の camelCase 名を使用する

Elixir API で LovyanGFX の名前をそのまま使用する方法です。

```elixir
AtomLGFX.call(port, :fillRect, [10, 10, 80, 40, color])
```

LovyanGFX との対応は明確になりますが、Elixir に C++ の命名様式を露出させます。

公開 Elixir API では Elixir らしい `snake_case` を使用するため、採用しません。

### 完全に型付けしたネイティブ操作登録表

すべての操作、引数型、対象方針、応答型を記述する型付きネイティブ登録表を持つ方法です。

安全性は高まりますが、本来 Elixir 層に属するプロトコル知識を重複させます。

初期の書き直しでは C/C++ 側に定義上の責務を持たせすぎるため、採用しません。

### C++ 関数名による完全な動的呼び出し

任意の LovyanGFX 関数を名前で動的に呼び出せれば理想的ですが、この用途に実用的な実行時関数反映機構は C++ にありません。

実装方式として採用しません。

採用する設計は、薄い手書きまたは生成済み振り分けを備えた、低水準の動的プロトコル橋渡しです。

### LovyanGFX の全関数を公開する

対応する LovyanGFX 関数を、すべて通常の Elixir 補助関数として公開する方法です。

一部の操作は AtomVM ポート境界越しの利用に適しません。画素単位操作や書き込みトランザクション形式の API は、Elixir から繰り返し呼ぶと性能低下を起こしやすいため、採用しません。

公開 API を選別し、必要な場合だけ低水準の退避口を提供します。

## v2 の初期範囲

最初の実装では、小規模ながら実用的な LovyanGFX 操作の集合を対応対象とします。

主要公開 API:

- `init`
- `close`
- `width`
- `height`
- `set_rotation`
- `set_brightness`
- `fill_screen`
- `draw_line`
- `draw_rect`
- `fill_rect`
- `draw_circle`
- `fill_circle`
- `set_cursor`
- `set_text_color`
- `set_text_size`
- `draw_string`
- `push_image`
- `batch`

低水準または内部限定 API:

- `draw_pixel`
- `write_pixel`
- `write_color`
- `write_fast_hline`
- `write_fast_vline`
- `start_write`
- `end_write`

成功基準は、対応関数の数ではありません。LovyanGFX 関数を1つ追加するとき、振り分けへの小さな追加または生成済み操作項目だけで済むことを基準とします。

## 判断の要約

次の単一呼び出し方式の AtomVM ポートプロトコルを使用します。

```erlang
{lgfx, 2, call, OpCode, Target, Flags, Args}
```

Elixir の公開 API では、`snake_case` の操作アトムを使用します。

Elixir 層が操作アトムを生成済みの数値操作コードへ変換します。

ネイティブ層は薄い橋渡しとして、共通包絡を復号し、対象を解決し、操作コードで振り分け、LovyanGFX を呼び出して応答を返します。

実装方針としては、単一の操作定義から Elixir 側の付随情報とネイティブ側の単純振り分けの両方を生成することを推奨します。手書きのネイティブコードは、状態を持つ操作、データを伴う操作、表示に特化した操作に限定します。

主要公開 API は意図的に選別します。画素単位操作のような既知の性能上の落とし穴は公開しません。必要であれば、明確に分離した低水準 API だけで提供します。

これにより、低水準の動的プロトコルが持つ単純さを得ながら、C++ 関数の完全な動的呼び出しが現実的でない点と、文字列によるネイティブ振り分けの性能費用を避けられます。
