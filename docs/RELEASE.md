# Release & Versioning

DualityRF ships through a GitHub Actions pipeline that turns a merge into a
`release/*` branch into a fully built, versioned and published GitHub Release —
with no manual version bumping and no pre-provisioned runners.

- [Overview](#overview)
- [Conventional Commits](#conventional-commits)
- [Semantic versioning strategy](#semantic-versioning-strategy)
- [The release pipeline](#the-release-pipeline)
- [Build environment](#build-environment)
- [Developer workflow](#developer-workflow)
- [Files](#files)

---

## Overview

The single source of truth for the application version is the top-level
`CMakeLists.txt`:

```cmake
project(DualityRF VERSION 2.0.0 LANGUAGES CXX)
```

That value is compiled into the app via `src/core/Version.h.in`
(`@PROJECT_VERSION@`). The pipeline reads the previous release from git tags,
computes the next version from Conventional Commits, builds, publishes, then
writes the new version back into `CMakeLists.txt`.

## Conventional Commits

Only three commit types are allowed. The format is:

```
<type>[!]: <description>
```

| Part | Rule |
| --- | --- |
| `<type>` | one of `feat`, `fix`, `chore` |
| `!` | optional; marks a **breaking change** |
| `:` | required |
| `<description>` | required |

Examples:

```
feat: add SDR recording
fix: prevent startup crash
chore: update dependencies
feat!: redesign configuration format
fix!: remove deprecated API
```

Enforcement happens in two places, so bad commits are caught locally **and**
can't be merged even if the local hook is bypassed:

- **Locally** — a Husky `commit-msg` hook runs `commitlint`
  (`.husky/commit-msg`, `commitlint.config.js`).
- **In CI** — the [`Commit Lint`](../.github/workflows/commitlint.yml) workflow
  re-validates every commit on pull requests and on pushes to `release/*` / `main`.

## Semantic versioning strategy

`scripts/next-version.sh` calculates the next version. It takes the base version
from the newest `vX.Y.Z` tag (falling back to the `CMakeLists.txt` version for
the very first release) and inspects every commit since that tag.

Bump priority — the **highest** matching rule wins:

| Priority | Trigger | Result |
| --- | --- | --- |
| 1. Major | any commit with `!` (or a `BREAKING CHANGE:` footer) | `X+1.0.0` |
| 2. Minor | any `feat` | `X.Y+1.0` |
| 3. Patch | any `fix` or `chore` | `X.Y.Z+1` |
| — | nothing relevant | no release |

Worked examples from `1.3.3`:

```
fix: added logging          → 1.3.4
feat: added SDR scanning     → 1.4.0
chore: update documentation  → 1.3.4
feat!: redesign project      → 2.0.0
fix!: remove deprecated conf → 2.0.0
```

## The release pipeline

Defined in [`.github/workflows/release.yml`](../.github/workflows/release.yml),
triggered on push to the `release` branch or any `release/**` branch. It runs
four jobs:

1. **`version`** — full-history checkout, runs `next-version.sh`, and exposes
   `bump`, `version`, `tag`, `previous` as job outputs. It is skipped for the
   pipeline's own version-bump commit (guarded by the commit-message prefix).
2. **`build`** — a matrix over `x86_64` (`ubuntu-24.04`) and `arm64`
   (`ubuntu-24.04-arm`). Each runner sets up the toolchain via the composite
   action, configures with CMake + Ninja + ccache, builds, then packages a
   `dualityrf-<version>-linux-<arch>.tar.gz` (plus a `.sha256`) and uploads it
   as a **workflow artifact**. `fail-fast: true` means any build failure fails
   the release immediately.
3. **`release`** — downloads the artifacts, generates categorized notes from the
   Conventional Commits (`release-notes.sh`), and creates the GitHub Release at
   tag `vX.Y.Z`, attaching both archives (and checksums) as **release assets**.
4. **`sync-version`** — bumps `CMakeLists.txt` (`bump-version.sh`), commits
   `chore: release vX.Y.Z [skip ci]`, and pushes it back to the release branch
   so the repo always reflects the latest release.

### No re-trigger loop

The sync commit is pushed with the built-in `GITHUB_TOKEN`, and GitHub does not
start new workflow runs from `GITHUB_TOKEN` pushes. As belt-and-suspenders the
commit carries `[skip ci]` and the `version` job also skips any
`chore: release v…` head commit.

## Build environment

The composite action
[`.github/actions/setup-build`](../.github/actions/setup-build/action.yml)
prepares each runner from scratch (nothing is assumed pre-installed) and is
shared by both matrix legs. It:

- installs `build-essential`, `cmake`, `ninja-build`, `pkg-config`, `ccache`,
  `qt6-base-dev` (+dev-tools), `libsoapysdr-dev`, `soapysdr-tools`,
  `libfftw3-dev`, and the SoapySDR device modules (HackRF, RTL-SDR);
- restores/saves a per-architecture **ccache** to cut rebuild times;
- **verifies** the toolchain before compiling: `qmake6 --version` + the Qt6
  Widgets CMake package, `SoapySDRUtil --info` + `pkg-config SoapySDR`, and
  `pkg-config fftw3f`. A missing dependency fails the job before the build.

> **ARM64 runners:** the pipeline uses GitHub's native `ubuntu-24.04-arm`
> hosted runners (free for public repositories). For a private repository,
> enable Arm64 hosted runners or swap that matrix leg for a QEMU/`docker`
> cross-build.

## Developer workflow

1. Install the local hooks once:
   ```bash
   npm install    # runs "prepare" → installs the Husky commit-msg hook
   ```
2. Branch, commit using Conventional Commits, open a PR. The `Commit Lint`
   check must pass.
3. Merge into a `release/*` branch (e.g. `release/1.x`). The pipeline builds,
   publishes `vX.Y.Z`, and pushes the version bump back automatically.
4. Download binaries from the GitHub Release page or from the workflow's
   artifacts.

## Files

| Path | Purpose |
| --- | --- |
| `.github/workflows/release.yml` | Version → build → release → sync pipeline |
| `.github/workflows/commitlint.yml` | Conventional Commit enforcement in CI |
| `.github/actions/setup-build/action.yml` | Composite: install + verify + cache toolchain |
| `scripts/next-version.sh` | Compute next semver from commits |
| `scripts/release-notes.sh` | Categorized release notes from commits |
| `scripts/bump-version.sh` | Write the new version into `CMakeLists.txt` |
| `commitlint.config.js` | Allowed types (`feat`/`fix`/`chore`) + rules |
| `.husky/commit-msg` | Local commit-message validation hook |
