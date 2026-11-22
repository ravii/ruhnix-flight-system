#!/usr/bin/env bash
# Builds and tests every module on the host toolchain (inside the dev container).
# Used locally and by CI.
#
#   scripts/build_all.sh               all modules
#   scripts/build_all.sh esc simulator only these
#
# Environment:
#   BUILD_ROOT   build directory root (default: build/docker)
#   BUILD_TYPE   CMAKE_BUILD_TYPE (default: Debug)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${BUILD_ROOT:-$REPO_ROOT/build/docker}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
MODULES=(common flight-controller esc simulator perception localization)
if [[ $# -gt 0 ]]; then
    MODULES=("$@")
fi

generator=()
if command -v ninja >/dev/null 2>&1; then
    generator=(-G Ninja)
fi

failed=()
for m in "${MODULES[@]}"; do
    echo "===== $m ====="
    dir="$BUILD_ROOT/$m"
    if cmake -S "$REPO_ROOT/$m" -B "$dir" "${generator[@]}" \
            -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DUAV_WARNINGS_AS_ERRORS=ON &&
        cmake --build "$dir" --parallel &&
        ctest --test-dir "$dir" --output-on-failure; then
        :
    else
        failed+=("$m")
    fi
done

if [[ ${#failed[@]} -gt 0 ]]; then
    echo "FAILED: ${failed[*]}" >&2
    exit 1
fi
echo "All modules built and tested: ${MODULES[*]}"
