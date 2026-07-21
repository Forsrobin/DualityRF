#!/usr/bin/env bash
#
# Update the project version stored in CMakeLists.txt (the single source of
# truth, consumed by src/core/Version.h.in). Usage: bump-version.sh <version>
set -euo pipefail

version="${1:?usage: bump-version.sh <version>}"
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "error: invalid version '$version'" >&2
    exit 1
}

sed -i -E \
    "s/(project\(DualityRF VERSION )[0-9]+\.[0-9]+\.[0-9]+/\1${version}/" \
    CMakeLists.txt

grep -q "VERSION ${version} " CMakeLists.txt || {
    echo "error: failed to update version in CMakeLists.txt" >&2
    exit 1
}
echo "CMakeLists.txt version set to ${version}"
