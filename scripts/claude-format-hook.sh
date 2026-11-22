#!/usr/bin/env bash
# Claude Code PostToolUse hook (Edit|Write): clang-formats the file that was
# just written, if it is a C/C++ source. Reads the hook JSON from stdin.
# Pure bash + grep/sed so it works without jq or python.

set -euo pipefail

payload="$(cat)"
path="$(printf '%s' "$payload" |
    grep -o '"file_path"[[:space:]]*:[[:space:]]*"[^"]*"' | head -1 |
    sed -e 's/^"file_path"[[:space:]]*:[[:space:]]*"//' -e 's/"$//' -e 's/\\\\/\\/g')"

[[ -n "$path" ]] || exit 0
[[ "$path" =~ \.(c|h|cc|cpp|cxx|hh|hpp|hxx)$ ]] || exit 0
[[ "$path" =~ (third_party|/vendor/|\\vendor\\) ]] && exit 0

if command -v cygpath >/dev/null 2>&1; then
    path="$(cygpath -u "$path")"
fi
[[ -f "$path" ]] || exit 0

exec "$(dirname "${BASH_SOURCE[0]}")/format.sh" "$path"
