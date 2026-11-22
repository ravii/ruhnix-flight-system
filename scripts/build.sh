#!/usr/bin/env bash
# Builds the modules. With no options: a host Debug build of every module for
# developing and debugging in WSL. Only the host compiler is used; the chip
# toolchains (arm-none-eabi, aarch64-linux-gnu) are never invoked.
#
#   scripts/build.sh                                host build of all modules
#   scripts/build.sh flight-controller simulator    host build of these modules only
#   scripts/build.sh -t                             host build, then run the unit tests
#   scripts/build.sh --arm64 -t                     perception + localization for ARM64 (tests under qemu)
#   scripts/build.sh --fw stm32f4                   flight-controller firmware for bsp/stm32f4
#   scripts/build.sh --sitl                         flight-controller firmware as a Linux process (SITL)
#   scripts/build.sh --host --sitl -t               host build + SITL, then tests + SITL lockstep check
#   scripts/build.sh --host --arm64                 several targets in one run
#
# Options:
#   --host          host toolchain (the default when no target option is given)
#   --arm64         aarch64-linux-gnu cross build: perception, localization
#   --fw BOARD      arm-none-eabi firmware build: flight-controller for bsp/BOARD
#                   (needs the FreeRTOS-Kernel submodule)
#   --sitl          host-compiler firmware build: flight-controller for bsp/sitl
#                   (FreeRTOS POSIX port). With -t: runs scripts/sitl.sh --check,
#                   which needs the host simulator build (add --host)
#   -t, --test      run ctest after building (host, arm64; MCU firmware has no tests)
#   -r, --release   CMAKE_BUILD_TYPE=Release (default: Debug)
#   -c, --clean     delete the selected build directories before configuring
#   -h, --help      show this help
#
# Build directories: $BUILD_ROOT/<target>/<module>, target = host | arm64 | sitl | fw-<BOARD>.
# Every build uses -Werror (RFS_WARNINGS_AS_ERRORS), the same as CI.
#
# Environment:
#   BUILD_ROOT   build directory root (default: <repo>/build)
#   BUILD_TYPE   CMAKE_BUILD_TYPE (default: Debug; --release overrides it)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${BUILD_ROOT:-$REPO_ROOT/build}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"

HOST_MODULES=(common flight-controller simulator perception localization)
ARM64_MODULES=(perception localization)
FW_MODULES=(flight-controller)

# CMake >= 3.21 picks up a toolchain file from the environment; the host build
# must never see one, and cross builds pass theirs explicitly.
unset CMAKE_TOOLCHAIN_FILE

die() {
    echo "build.sh: $*" >&2
    exit 2
}

usage() {
    sed -n '2,/^$/p' "${BASH_SOURCE[0]}" | sed -e 's/^# \{0,1\}//'
}

contains() {
    local needle=$1 x
    shift
    for x in "$@"; do
        [[ "$x" == "$needle" ]] && return 0
    done
    return 1
}

# ---- arguments -------------------------------------------------------------
want_host=0
want_arm64=0
want_sitl=0
boards=()
modules=()
run_tests=0
clean=0
while [[ $# -gt 0 ]]; do
    case "$1" in
    --host) want_host=1 ;;
    --arm64) want_arm64=1 ;;
    --sitl) want_sitl=1 ;;
    --fw)
        [[ $# -ge 2 && "$2" != -* ]] || die "--fw needs a board name (a directory under flight-controller/bsp/)"
        boards+=("$2")
        shift
        ;;
    --fw=*) boards+=("${1#--fw=}") ;;
    -t | --test) run_tests=1 ;;
    -r | --release) BUILD_TYPE=Release ;;
    -c | --clean) clean=1 ;;
    -h | --help)
        usage
        exit 0
        ;;
    -*) die "unknown option '$1' (see --help)" ;;
    *)
        contains "$1" "${HOST_MODULES[@]}" || die "unknown module '$1' (modules: ${HOST_MODULES[*]})"
        modules+=("$1")
        ;;
    esac
    shift
done
if [[ $want_arm64 -eq 0 && $want_sitl -eq 0 && ${#boards[@]} -eq 0 ]]; then
    want_host=1
fi

# ---- plan: one job per (target, module) ------------------------------------
job_targets=()
job_modules=()
plan() {
    local target=$1 m
    shift
    for m in "$@"; do
        if [[ ${#modules[@]} -eq 0 ]] || contains "$m" "${modules[@]}"; then
            job_targets+=("$target")
            job_modules+=("$m")
        fi
    done
}

need_tool() {
    command -v "$1" >/dev/null 2>&1 || die "$1 not found; install the toolchain with: sudo bash docker/setup_ubuntu.sh"
}

if [[ $want_host -eq 1 ]]; then
    plan host "${HOST_MODULES[@]}"
fi
if [[ $want_arm64 -eq 1 ]]; then
    need_tool aarch64-linux-gnu-g++
    plan arm64 "${ARM64_MODULES[@]}"
fi
if [[ $want_sitl -eq 1 ]]; then
    plan sitl "${FW_MODULES[@]}"
fi
for board in "${boards[@]}"; do
    [[ "$board" != _template ]] || die "bsp/_template is a template, not a board: copy it to bsp/<board> first"
    [[ "$board" != sitl ]] || die "bsp/sitl is built with the host compiler: use --sitl, not --fw sitl"
    if [[ ! -f "$REPO_ROOT/flight-controller/bsp/$board/board.cmake" ]]; then
        available="$(find "$REPO_ROOT/flight-controller/bsp" -mindepth 2 -maxdepth 2 -name board.cmake \
            -not -path '*/_template/*' -printf '%h\n' | xargs -rn1 basename | tr '\n' ' ')"
        die "no board '$board' (flight-controller/bsp/$board/board.cmake). Available: ${available:-none}." \
            "Create one from bsp/_template, see flight-controller/bsp/README.md"
    fi
    need_tool arm-none-eabi-g++
    plan "fw-$board" "${FW_MODULES[@]}"
done

for m in "${modules[@]}"; do
    contains "$m" "${job_modules[@]}" || die "module '$m' is not built by the selected target(s)" \
        "(arm64: ${ARM64_MODULES[*]}; --sitl, --fw: ${FW_MODULES[*]})"
done

# ---- build -----------------------------------------------------------------
generator=()
if command -v ninja >/dev/null 2>&1; then
    generator=(-G Ninja)
fi

emulator=()
if command -v qemu-aarch64 >/dev/null 2>&1; then
    emulator=(-DCMAKE_CROSSCOMPILING_EMULATOR=qemu-aarch64)
    export QEMU_LD_PREFIX="${QEMU_LD_PREFIX:-/usr/aarch64-linux-gnu}"
fi

# Fails if a host build directory ended up with a cross toolchain (stale cache,
# or CC/CXX pointing at a chip compiler).
check_host_cache() {
    local cache="$1/CMakeCache.txt"
    if grep -qE '^(CMAKE_TOOLCHAIN_FILE:[A-Z]*=.+|CMAKE_C(XX)?_COMPILER:[A-Z]*=.*(arm-none-eabi|aarch64-linux-gnu))' \
        "$cache"; then
        echo "build.sh: $1 is configured with a cross toolchain; host builds must use the host compiler." >&2
        echo "          Check CC/CXX, then rerun with --clean." >&2
        return 1
    fi
}

build_one() {
    local target=$1 module=$2
    local dir="$BUILD_ROOT/$target/$module"
    local args=(-S "$REPO_ROOT/$module" -B "$dir" "${generator[@]}"
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DRFS_WARNINGS_AS_ERRORS=ON)
    case "$target" in
    host)
        if [[ "$module" == flight-controller ]]; then
            args+=(-DFC_BOARD=)  # host build: core + SimBoard + unit tests
        fi
        ;;
    arm64)
        args+=(-DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/cmake/toolchains/aarch64-linux-gnu.cmake" "${emulator[@]}")
        ;;
    sitl)
        args+=(-DFC_BOARD=sitl)  # host compiler, FreeRTOS POSIX port
        ;;
    fw-*)
        args+=(-DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/cmake/toolchains/arm-none-eabi.cmake"
            -DFC_BOARD="${target#fw-}")
        ;;
    esac

    if [[ $clean -eq 1 ]]; then
        rm -rf "$dir"
    fi
    cmake "${args[@]}" || return 1
    if [[ "$target" == host || "$target" == sitl ]]; then
        check_host_cache "$dir" || return 1
    fi
    cmake --build "$dir" --parallel || return 1

    if [[ $run_tests -eq 1 ]]; then
        case "$target" in
        host) ctest --test-dir "$dir" --output-on-failure || return 1 ;;
        sitl) "$REPO_ROOT/scripts/sitl.sh" --no-build --check || return 1 ;;
        arm64)
            if [[ ${#emulator[@]} -gt 0 ]]; then
                ctest --test-dir "$dir" --output-on-failure || return 1
            else
                echo "build.sh: qemu-aarch64 not found, skipping arm64 tests" >&2
            fi
            ;;
        esac
    fi
}

failed=()
for i in "${!job_targets[@]}"; do
    echo "===== ${job_targets[$i]}: ${job_modules[$i]} ($BUILD_TYPE) ====="
    if ! build_one "${job_targets[$i]}" "${job_modules[$i]}"; then
        failed+=("${job_targets[$i]}/${job_modules[$i]}")
    fi
done

if [[ ${#failed[@]} -gt 0 ]]; then
    echo "FAILED: ${failed[*]}" >&2
    exit 1
fi
echo "OK: ${#job_targets[@]} build(s) in $BUILD_ROOT ($BUILD_TYPE$([[ $run_tests -eq 1 ]] && echo ', tested'))"
