#!/usr/bin/env bash
#
# Configure and build DualityRF in ./build, mirroring the CI release workflow
# (.github/workflows/release.yml + .github/actions/setup-build).
#
# Pass --restart to kill any running duality_rf and relaunch it with the new
# build once done (reusing its previous args/DISPLAY, e.g. --kiosk).
set -euo pipefail

RESTART=0
if [[ "${1:-}" == "--restart" ]]; then
  RESTART=1
fi

cd "$(dirname "${BASH_SOURCE[0]}")"

CMAKE_ARGS=(-S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release)
if command -v ccache >/dev/null 2>&1; then
  CMAKE_ARGS+=(-DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build build --parallel

BIN="$(pwd)/build/src/duality_rf"
RUNNING_PID="$(pgrep -f -o "^${BIN}\b" || true)"

if [[ -n "$RUNNING_PID" ]]; then
  if [[ "$RESTART" -eq 0 ]]; then
    echo "duality_rf is running (pid ${RUNNING_PID}). Rerun with './build.sh --restart' to kill and relaunch it."
  else
    # Reuse the running instance's args/env (e.g. --kiosk, DISPLAY) so the
    # restarted app behaves the same as however it was originally launched.
    mapfile -d '' RUN_ARGS < <(tr '\0' '\n' < "/proc/${RUNNING_PID}/cmdline" | tail -n +2 | tr '\n' '\0')
    RUN_DISPLAY="$(tr '\0' '\n' < "/proc/${RUNNING_PID}/environ" | sed -n 's/^DISPLAY=//p' | head -1)"

    kill "$RUNNING_PID"
    for _ in $(seq 1 20); do
      kill -0 "$RUNNING_PID" 2>/dev/null || break
      sleep 0.2
    done

    DISPLAY="${RUN_DISPLAY:-${DISPLAY:-:0}}" nohup "$BIN" "${RUN_ARGS[@]}" >/dev/null 2>&1 &
    disown
    echo "Restarted duality_rf (pid $!)."
  fi
fi
