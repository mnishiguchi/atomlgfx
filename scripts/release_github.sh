#!/bin/sh
# SPDX-FileCopyrightText: 2026 Masatoshi Nishiguchi
#
# SPDX-License-Identifier: Apache-2.0

# GitHub Release の作成補助。Hex への公開は別途手動で行う。
set -eu

repo="mnishiguchi/atomlgfx"
mode=${1:-}
version=${2:-}

usage() {
    printf '使い方: sh scripts/release_github.sh {notes|check|publish} VERSION\n' >&2
    exit 2
}

fail() {
    printf 'エラー: %s\n' "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "必要なコマンドがありません: $1"
}

case "$mode" in
    notes|check|publish) ;;
    *) usage ;;
esac
[ -n "$version" ] && [ "$#" -eq 2 ] || usage
printf '%s\n' "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$' || fail "バージョンは 0.1.1 のような形式で指定してください: $version"
tag="v$version"

# リポジトリのルートで実行する。
[ -f mix.exs ] && [ -f CHANGELOG.md ] || fail "atomlgfx のリポジトリルートで実行してください"

mix_version=$(sed -n 's/^[[:space:]]*@version[[:space:]]*"\([^"]*\)".*/\1/p' mix.exs | head -n 1)
[ "$mix_version" = "$version" ] || fail "mix.exs のバージョンは '$mix_version' です（指定: '$version'）"

grep -Fqx "## $version" CHANGELOG.md || fail "CHANGELOG.md に '## $version' の見出しがありません"

render_notes() {
    awk -v version="$version" '
        /^## / {
            if (found) exit
            if ($0 == "## " version) found = 1
            next
        }
        found { print }
    ' CHANGELOG.md | sed "s@](docs/@](https://github.com/$repo/blob/$tag/docs/@g"
    printf '\n## リンク\n\n'
    printf -- '- [Hex パッケージ](https://hex.pm/packages/atomlgfx/%s)\n' "$version"
    printf -- '- [HexDocs](https://hexdocs.pm/atomlgfx/%s/)\n' "$version"
    printf -- '- [Getting Started](https://github.com/%s/blob/%s/docs/getting-started.md)\n' "$repo" "$tag"
}

if [ "$mode" = notes ]; then
    render_notes
    exit 0
fi

for cmd in git gh curl; do require_command "$cmd"; done
[ "$(git branch --show-current)" = main ] || fail "main ブランチに切り替えてください"
[ -z "$(git status --porcelain)" ] || fail "作業ツリーに未コミットの変更や未追跡ファイルがあります"

git fetch --quiet origin main --tags
head=$(git rev-parse HEAD)
remote_head=$(git rev-parse refs/remotes/origin/main)
[ "$head" = "$remote_head" ] || fail "main が origin/main と一致しません（git pull --ff-only を実行してください）"

gh auth status >/dev/null 2>&1 || fail "GitHub CLI が未認証です（gh auth login）"

# 対応する Hex パッケージの公開を確認する。
curl --fail --silent --show-error --location --max-time 20 \
    "https://hex.pm/api/packages/atomlgfx/releases/$version" >/dev/null \
    || fail "Hex パッケージ atomlgfx $version を確認できません。先に Hex へ公開してください"

local_tag=false
if git show-ref --verify --quiet "refs/tags/$tag"; then
    local_tag=true
    tag_commit=$(git rev-list -n 1 "$tag")
    [ "$tag_commit" = "$head" ] || fail "既存のローカルタグ $tag は $tag_commit を指しています（main: $head）"
fi

remote_tag=false
if git ls-remote --exit-code --tags origin "refs/tags/$tag" >/dev/null 2>&1; then
    remote_tag=true
    [ "$local_tag" = true ] || fail "リモートに $tag がありますが、ローカルにはありません。git fetch --tags origin の後に再実行してください"
    # ローカルとリモートのタグが同じオブジェクトを指すことを確認する。
    local_object=$(git rev-parse "refs/tags/$tag")
    remote_object=$(git ls-remote --tags origin "refs/tags/$tag" | awk '{ print $1 }')
    [ "$local_object" = "$remote_object" ] || fail "ローカルとリモートのタグが一致しません: $tag"
fi

# 既存の GitHub Release は上書きしない。
if gh release view "$tag" --repo "$repo" >/dev/null 2>&1; then
    fail "GitHub Release $tag は既に存在します。本文の変更には gh release edit を使ってください"
fi

printf 'GitHub Release の対象: %s\n' "$tag"
printf '  コミット: %s\n' "$head"
printf '  Hex:    https://hex.pm/packages/atomlgfx/%s\n' "$version"
printf '  タグ:    ローカル=%s リモート=%s\n' "$local_tag" "$remote_tag"

if [ "$mode" = check ]; then
    printf '確認完了。公開するには just release-github %s を実行してください\n' "$version"
    exit 0
fi

printf '\n必要に応じて注釈付き Git タグを作成・push し、GitHub Release を公開します。\n'
printf '%s（%s）を公開しますか？ [y/N] ' "$tag" "$head"
IFS= read -r answer || fail "確認が必要です"
case "$answer" in
    y|Y|yes|YES) ;;
    *) fail "中止しました" ;;
esac

if [ "$local_tag" = false ]; then
    git tag -a "$tag" -m "AtomLGFX $tag"
fi
if [ "$remote_tag" = false ]; then
    git push origin "refs/tags/$tag"
fi

notes=$(mktemp)
trap 'rm -f "$notes"' EXIT HUP INT TERM
render_notes > "$notes"
gh release create "$tag" --repo "$repo" --verify-tag \
    --title "AtomLGFX $tag" --notes-file "$notes"
printf '公開しました: https://github.com/%s/releases/tag/%s\n' "$repo" "$tag"
