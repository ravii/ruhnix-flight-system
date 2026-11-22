#!/usr/bin/env bash
# SITL: runs the flight-controller firmware as a Linux process (FreeRTOS POSIX
# port, bsp/sitl) in lockstep with the simulator over UDP.
#
#   scripts/sitl.sh                    build, fly the rfs_sim script, trace in build/sitl/trace.csv
#   scripts/sitl.sh --check            ... and require the trace to be identical to the
#                                      in-process algorithm loop (same core, no RTOS)
#   scripts/sitl.sh -- --duration 3    pass the remaining arguments to rfs_sim
#
# Options:
#   --no-build    use the existing builds (build/host/simulator, build/sitl/flight-controller)
#   --check       compare against the in-process run; exit 1 on any difference
#   -h, --help    show this help
#
# To debug the firmware instead, start it yourself (gdb) and point rfs_sim at it:
#   gdb -ex 'handle SIGALRM SIGUSR1 nostop noprint pass' build/sitl/flight-controller/flight_controller
#   build/host/simulator/rfs_sim --udp 127.0.0.1:14560 > trace.csv
#
# Environment:
#   BUILD_ROOT      build directory root (default: <repo>/build)
#   RFS_SITL_PORT   UDP port (default: a random free-looking port, so runs can overlap)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_ROOT="${BUILD_ROOT:-$REPO_ROOT/build}"

usage() {
    sed -n '2,/^$/p' "${BASH_SOURCE[0]}" | sed -e 's/^# \{0,1\}//'
}

build=1
check=0
sim_args=()
while [[ $# -gt 0 ]]; do
    case "$1" in
    --no-build) build=0 ;;
    --check) check=1 ;;
    -h | --help)
        usage
        exit 0
        ;;
    --)
        shift
        sim_args=("$@")
        break
        ;;
    *)
        echo "sitl.sh: unknown option '$1' (see --help)" >&2
        exit 2
        ;;
    esac
    shift
done

if [[ $build -eq 1 ]]; then
    "$REPO_ROOT/scripts/build.sh" --host simulator
    "$REPO_ROOT/scripts/build.sh" --sitl
fi

fc="$BUILD_ROOT/sitl/flight-controller/flight_controller"
sim="$BUILD_ROOT/host/simulator/rfs_sim"
for exe in "$fc" "$sim"; do
    if [[ ! -x "$exe" ]]; then
        echo "sitl.sh: $exe not found; build with: scripts/build.sh --host simulator && scripts/build.sh --sitl" >&2
        exit 1
    fi
done

out_dir="$BUILD_ROOT/sitl"
mkdir -p "$out_dir"
port="${RFS_SITL_PORT:-$((20000 + RANDOM % 40000))}"

RFS_SITL_PORT="$port" "$fc" 2>"$out_dir/flight_controller.log" &
fc_pid=$!
trap 'kill "$fc_pid" 2>/dev/null || true; wait "$fc_pid" 2>/dev/null || true' EXIT

sleep 0.2
if ! kill -0 "$fc_pid" 2>/dev/null; then
    echo "sitl.sh: the firmware exited at startup:" >&2
    cat "$out_dir/flight_controller.log" >&2
    exit 1
fi

echo "sitl.sh: firmware pid $fc_pid on udp 127.0.0.1:$port" >&2
"$sim" --udp "127.0.0.1:$port" "${sim_args[@]}" >"$out_dir/trace.csv"
echo "sitl.sh: SITL trace in $out_dir/trace.csv" >&2

if [[ $check -eq 1 ]]; then
    "$sim" "${sim_args[@]}" >"$out_dir/trace_in_process.csv"
    if ! cmp -s "$out_dir/trace.csv" "$out_dir/trace_in_process.csv"; then
        echo "sitl.sh: FAILED: SITL and in-process traces differ (first difference):" >&2
        diff "$out_dir/trace_in_process.csv" "$out_dir/trace.csv" | head -5 >&2 || true
        exit 1
    fi
    echo "sitl.sh: OK: SITL trace identical to the in-process algorithm loop ($(wc -l <"$out_dir/trace.csv") lines)" >&2
fi
