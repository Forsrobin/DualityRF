#!/usr/bin/env bash
#
# Update the project version in every file that records it. CMakeLists.txt is
# the single source of truth consumed by src/core/Version.h.in (and hence the
# app's displayed version); package.json is kept in sync for the npm/Husky
# tooling so the repository never shows a stale version.
#
# Usage: bump-version.sh <version>
set -euo pipefail

version="${1:?usage: bump-version.sh <version>}"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "error: invalid version '$version'" >&2
    exit 1
}

# CMakeLists.txt — the source of truth.
sed -i -E \
    "s/(project\(DualityRF VERSION )[0-9]+\.[0-9]+\.[0-9]+/\1${version}/" \
    CMakeLists.txt

grep -q "VERSION ${version} " CMakeLists.txt || {
    echo "error: failed to update version in CMakeLists.txt" >&2
    exit 1
}
echo "CMakeLists.txt version set to ${version}"

# package.json — kept in sync; only the first top-level "version" field.
if [[ -f package.json ]]; then
    sed -i -E \
        "0,/(\"version\"[[:space:]]*:[[:space:]]*\")[0-9]+\.[0-9]+\.[0-9]+(\")/s//\1${version}\2/" \
        package.json

    grep -q "\"version\": \"${version}\"" package.json || {
        echo "error: failed to update version in package.json" >&2
        exit 1
    }
    echo "package.json version set to ${version}"
fi
