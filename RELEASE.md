# リリース手順

Hex で Elixir API を公開し、対応する NIF は同じ Git タグのソースから構築します。
ファームウェア画像は配布せず、Hex と GitHub Release は手動で公開します。

## 公開前

`mix.exs` のバージョン、ExDoc の `source_ref`、[変更履歴](CHANGELOG.md) をそろえ、次を実行します。

```sh
just check
mix docs --warnings-as-errors
reuse lint --plain
mix hex.build --unpack --output /tmp/atomlgfx-package
mix hex.publish --dry-run --yes
```

- Hex パッケージに Elixir API、利用ガイド、ライセンスが含まれ、ネイティブソースやビルド成果物が含まれないことを確認します。
- 展開したパッケージを新規 Mix アプリから `path:` で参照し、取得とコンパイルを確認します。
- CI と[対応環境](docs/compatibility.md)の実機検証結果を確認します。検証後にネイティブコードを変更した場合は、[検証手順](docs/testing.md)に従って再検証します。
- `mix hex.info atomlgfx` でパッケージ名、`mix hex.user whoami` で公開先アカウントを確認します。

## 公開

リリース対象の変更を `main` にマージし、CI と実機検証を確認してから Hex に公開します。

```sh
mix hex.publish
```

Hex と HexDocs の公開を確認したら、Git タグと GitHub Release を作成します。

```sh
just release-notes 0.1.1
just release-check 0.1.1
just release-github 0.1.1
```

- `release-notes`: `CHANGELOG.md` からリリースノートを表示
- `release-check`: バージョン、Git、Hex、GitHub の状態を確認
- `release-github`: 確認後、注釈付き Git タグを作成・push し、GitHub Release を公開

`0.1.1` は例です。実際のバージョンに読み替えてください。
GitHub CLI（`gh`）での認証が必要です（`gh auth login`）。
既存のタグや Release がある場合は、上書きせずに停止します。

公開後は別の Mix アプリから Hex パッケージを取得し、コンパイルを確認します。
公開済みの版に修正が必要な場合は、パッチリリースとして公開します。
