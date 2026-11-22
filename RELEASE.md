# リリース手順

Hex は Elixir API を配布し、対応する NIF は同じ Git タグから構築します。
`v0.1.0` ではファームウェア画像を配布せず、Hex・HexDocs・GitHub Release を手動で公開します。

## 公開前

`mix.exs` の版、ExDoc の `source_ref`、[変更履歴](CHANGELOG.md) をそろえ、次を実行します。

```sh
just check
mix docs --warnings-as-errors
reuse lint --plain
mix hex.build --unpack --output /tmp/atomlgfx-package
mix hex.publish --dry-run --yes
```

- 展開先に Elixir API・利用ガイド・ライセンスが入り、ネイティブソースや構築成果物が入っていないことを確認します。
- 展開したパッケージを新規 Mix アプリの `path:` 依存として取得・コンパイルします。
- リリース候補の CI と、[対応環境](docs/compatibility.md) に記載した実機結果を確認します。
  検証後にネイティブや描画処理が変わった場合は、[検証手順](docs/testing.md) に従って再検証します。
- `mix hex.info atomlgfx` で名前の使用状況、`mix hex.user whoami` で公開アカウントを確認します。
  初回のパッケージ所有者は公開したアカウントです。

`--dry-run` は公開しません。通常の `mix hex.publish` はパッケージと文書を公開します。

## 公開

PR をマージし、CI が成功した `main` の対象コミットで実行します。

```sh
git switch main
git pull --ff-only
git rev-parse HEAD
mix hex.user whoami
git tag -a v0.1.0 -m "AtomLGFX v0.1.0"
mix hex.publish
git push origin v0.1.0
```

`mix hex.publish` はファイル一覧・版・アカウントを確認して対話的に進めます。
[Hex](https://hex.pm/packages/atomlgfx) と [HexDocs](https://hexdocs.pm/atomlgfx/0.1.0/) を確認後、
タグ `v0.1.0` の GitHub Release を作成します。本文には変更履歴の要点と、Hex・HexDocs・
[Getting Started](docs/getting-started.md) へのリンクを載せます。

公開後は別のアプリで `{:atomlgfx, "~> 0.1.0"}` を取得・コンパイルし、
実機確認には同じタグから構築した NIF を使います。
公開済みのタグやパッケージは上書きせず、修正は次のパッチ版で公開します。
