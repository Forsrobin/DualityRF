#!/usr/bin/env bash
#
# Compute the next semantic version from Conventional Commits.
#
# Base version comes from the newest `vX.Y.Z` git tag (the previous release);
# if there are no tags yet it falls back to the version in CMakeLists.txt and
# considers the entire history. Only `feat`, `fix` and `chore` commit types are
# recognised; a `!` after the type (or a `BREAKING CHANGE:` footer) forces a
# major bump.
#
# Bump priority (highest wins):
#   major  — any commit with `!` / BREAKING CHANGE
#   minor  — any `feat`
#   patch  — any `fix` or `chore`
#   none   — nothing relevant found (no release)
#
# Outputs `bump`, `version`, `tag` and `previous` to $GITHUB_OUTPUT when set,
# otherwise to stdout.
set -euo pipefail

rank() {
    case "$1" in
    none) echo 0 ;;
    patch) echo 1 ;;
    minor) echo 2 ;;
    major) echo 3 ;;
    esac
}

latest_tag="$(git tag --list 'v*' --sort=-v:refname | head -n1 || true)"
if [[ -n "$latest_tag" ]]; then
    base="${latest_tag#v}"
    range="${latest_tag}..HEAD"
else
    base="$(grep -Po 'project\([^)]*VERSION \K[0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | head -n1)"
    range="HEAD"
fi
: "${base:=0.0.0}"

IFS=. read -r major minor patch <<<"$base"

bump="none"
commit_re='^(feat|fix|chore)(\([^)]+\))?(!)?:[[:space:]]'
while IFS= read -r subject; do
    [[ "$subject" =~ $commit_re ]] || continue
    type="${BASH_REMATCH[1]}"
    bang="${BASH_REMATCH[3]}"
    if [[ -n "$bang" ]]; then
        level="major"
    elif [[ "$type" == "feat" ]]; then
        level="minor"
    else
        level="patch"
    fi
    if [[ "$(rank "$level")" -gt "$(rank "$bump")" ]]; then
        bump="$level"
    fi
done < <(git log --format='%s' "$range")

# A BREAKING CHANGE footer anywhere in the range also forces a major bump.
if git log --format='%B' "$range" | grep -qE '^BREAKING CHANGE:'; then
    bump="major"
fi

case "$bump" in
major) major=$((major + 1)); minor=0; patch=0 ;;
minor) minor=$((minor + 1)); patch=0 ;;
patch) patch=$((patch + 1)) ;;
none) ;;
esac

version="${major}.${minor}.${patch}"

{
    echo "bump=${bump}"
    echo "version=${version}"
    echo "tag=v${version}"
    echo "previous=${base}"
} >>"${GITHUB_OUTPUT:-/dev/stdout}"
