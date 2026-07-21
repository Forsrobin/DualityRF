#!/usr/bin/env bash
#
# Generate Markdown release notes from the Conventional Commits between the
# previous release tag and HEAD. Grouped into Breaking / Features / Fixes /
# Chores. Usage: release-notes.sh <new-version>
set -euo pipefail

new_version="${1:?usage: release-notes.sh <new-version>}"

latest_tag="$(git tag --list 'v*' --sort=-v:refname | head -n1 || true)"
if [[ -n "$latest_tag" ]]; then
    range="${latest_tag}..HEAD"
    since="since ${latest_tag}"
else
    range="HEAD"
    since="initial release"
fi

breaking="" features="" fixes="" chores=""
commit_re='^(feat|fix|chore)(\([^)]+\))?(!)?:[[:space:]](.*)$'
while IFS=$'\t' read -r hash subject; do
    [[ "$subject" =~ $commit_re ]] || continue
    type="${BASH_REMATCH[1]}"
    bang="${BASH_REMATCH[3]}"
    desc="${BASH_REMATCH[4]}"
    line="- ${desc} (${hash})"$'\n'
    if [[ -n "$bang" ]]; then
        breaking+="$line"
        continue
    fi
    case "$type" in
    feat) features+="$line" ;;
    fix) fixes+="$line" ;;
    chore) chores+="$line" ;;
    esac
done < <(git log --format='%h%x09%s' "$range")

echo "## v${new_version}"
echo
echo "_Changes ${since}._"
echo
[[ -n "$breaking" ]] && { echo "### ⚠ Breaking changes"; echo; printf '%s\n' "$breaking"; }
[[ -n "$features" ]] && { echo "### Features"; echo; printf '%s\n' "$features"; }
[[ -n "$fixes" ]] && { echo "### Fixes"; echo; printf '%s\n' "$fixes"; }
[[ -n "$chores" ]] && { echo "### Chores"; echo; printf '%s\n' "$chores"; }
if [[ -z "$breaking$features$fixes$chores" ]]; then
    echo "No Conventional Commits found."
fi
