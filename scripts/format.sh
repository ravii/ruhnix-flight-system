#!/usr/bin/env bash
# Formats C/C++ sources with clang-format using the repository's .clang-format.
#
#   scripts/format.sh                 format every tracked C/C++ file
#   scripts/format.sh --check         report unformatted files, exit 1 if any (CI)
#   scripts/format.sh FILE...         format only the given files
#   scripts/format.sh --check FILE... check only the given files
#
# clang-format is looked up in $CLANG_FORMAT, then PATH, then the copy bundled
# with Visual Studio. Use the version pinned in CLANG_FORMAT_VERSION below so
# that everyone produces identical output.

set -euo pipefail

CLANG_FORMAT_VERSION="20.1.8"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EXTENSIONS_RE='\.(c|h|cc|cpp|cxx|hh|hpp|hxx)$'
EXCLUDE_RE='^(third_party/|build/|out/)|/bsp/[^/]+/vendor/'

find_clang_format() {
    if [[ -n "${CLANG_FORMAT:-}" ]]; then
        echo "$CLANG_FORMAT"
        return
    fi
    if command -v clang-format >/dev/null 2>&1; then
        command -v clang-format
        return
    fi
    local vs
    for vs in "/c/Program Files/Microsoft Visual Studio"/*/*/VC/Tools/Llvm/x64/bin/clang-format.exe; do
        if [[ -x "$vs" ]]; then
            echo "$vs"
            return
        fi
    done
    echo "error: clang-format not found. Install clang-format ${CLANG_FORMAT_VERSION}" \
        "(e.g. 'pip install clang-format==${CLANG_FORMAT_VERSION}') or set CLANG_FORMAT." >&2
    exit 2
}

CF="$(find_clang_format)"
installed="$("$CF" --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)"
if [[ "$installed" != "$CLANG_FORMAT_VERSION" ]]; then
    echo "warning: clang-format $installed found, project pins $CLANG_FORMAT_VERSION;" \
        "output may differ from CI." >&2
fi

check=0
if [[ "${1:-}" == "--check" ]]; then
    check=1
    shift
fi

cd "$REPO_ROOT"
files=()
if [[ $# -gt 0 ]]; then
    for f in "$@"; do
        if [[ "$f" =~ $EXTENSIONS_RE ]] && [[ -f "$f" ]]; then
            files+=("$f")
        fi
    done
else
    while IFS= read -r f; do
        if [[ "$f" =~ $EXTENSIONS_RE ]] && ! [[ "$f" =~ $EXCLUDE_RE ]] && [[ -f "$f" ]]; then
            files+=("$f")
        fi
    done < <(git ls-files --cached --others --exclude-standard)
fi

if [[ ${#files[@]} -eq 0 ]]; then
    exit 0
fi

if [[ $check -eq 1 ]]; then
    bad=()
    for f in "${files[@]}"; do
        if ! "$CF" --style=file --dry-run --Werror "$f" >/dev/null 2>&1; then
            bad+=("$f")
        fi
    done
    if [[ ${#bad[@]} -gt 0 ]]; then
        echo "The following files are not clang-formatted:" >&2
        printf '  %s\n' "${bad[@]}" >&2
        echo "Run: scripts/format.sh" >&2
        exit 1
    fi
    echo "All ${#files[@]} files are formatted."
else
    "$CF" --style=file -i "${files[@]}"
    echo "Formatted ${#files[@]} files."
fi
