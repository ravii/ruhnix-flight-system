#!/usr/bin/env bash
# Builds and tests every module on the host toolchain. Used by CI (inside the
# dev container) and locally. Same as `scripts/build.sh --test`; see build.sh
# for the options, build directories and environment variables.
#
#   scripts/build_all.sh                              all modules
#   scripts/build_all.sh flight-controller simulator  only these

set -euo pipefail

exec "$(dirname "${BASH_SOURCE[0]}")/build.sh" --test "$@"
