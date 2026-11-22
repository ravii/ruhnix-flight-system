#!/usr/bin/env bash
# Runs a command in WSL on this same checkout (seen from WSL as /mnt/<drive>/...).
# For editing on Windows while building, testing and debugging in WSL, without
# keeping a second clone in sync. Run from Git Bash on Windows.
#
#   scripts/wsl.sh scripts/build_all.sh        build and test all modules
#   scripts/wsl.sh scripts/build_all.sh simulator   only some modules
#   scripts/wsl.sh                             interactive WSL shell in the repo
#
# BUILD_ROOT defaults to ~/rfs-build inside WSL: building on /mnt/* is slow, so
# build output stays on the WSL filesystem. Sources are read from /mnt/<drive>.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Git Bash would otherwise rewrite /unix/style arguments into Windows paths.
export MSYS_NO_PATHCONV=1

if ! command -v wsl.exe >/dev/null 2>&1; then
    echo "wsl.exe not found: run this from Git Bash on Windows (inside WSL, run commands directly)" >&2
    exit 2
fi

setup='export BUILD_ROOT="${BUILD_ROOT:-$HOME/rfs-build}"'
if [[ $# -eq 0 ]]; then
    exec wsl.exe --cd "$(cygpath -w "$REPO_ROOT")" -e bash -lc "$setup; exec bash -l"
fi
exec wsl.exe --cd "$(cygpath -w "$REPO_ROOT")" -e bash -lc "$setup; $*"
