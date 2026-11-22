#!/usr/bin/env bash
# Builds and tests everything that needs no chip toolchain. Used by CI (inside
# the dev container) and locally:
#   - every module on the host toolchain, with unit tests
#   - the SITL firmware (FreeRTOS POSIX port), with the lockstep check
#     (scripts/sitl.sh --check: SITL trace == in-process algorithm loop)
# Same as `scripts/build.sh --host --sitl --test`; see build.sh for the
# options, build directories and environment variables.
#
#   scripts/build_all.sh                              everything
#   scripts/build_all.sh flight-controller simulator  only these modules

set -euo pipefail

exec "$(dirname "${BASH_SOURCE[0]}")/build.sh" --host --sitl --test "$@"
