#!/usr/bin/env bash
# Helper for the development container (docker/Dockerfile).
#
#   scripts/docker.sh build          build (or rebuild) the image
#   scripts/docker.sh shell          interactive shell, repo mounted at /workspace
#   scripts/docker.sh run CMD...     run one command in the container, e.g.
#                                    scripts/docker.sh run scripts/build_all.sh
#
# Environment:
#   UAV_IMAGE        image name (default: uav-flight-dev:ubuntu26.04)
#   UAV_BASE_IMAGE   base image override (default: ubuntu:26.04 from Docker Hub)
#   UAV_DOCKER_ARGS  extra `docker run` arguments, e.g. "--privileged -v /dev/bus/usb:/dev/bus/usb"

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${UAV_IMAGE:-uav-flight-dev:ubuntu26.04}"

# Git Bash on Windows rewrites /workspace-style arguments into Windows paths.
export MSYS_NO_PATHCONV=1

host_path() {
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -w "$1"
    else
        echo "$1"
    fi
}

build_args=()
if [[ "$(uname -s)" == Linux ]]; then
    # Match the host user so files created in /workspace keep the right owner.
    build_args+=(--build-arg "USER_UID=$(id -u)" --build-arg "USER_GID=$(id -g)")
fi
if [[ -n "${UAV_BASE_IMAGE:-}" ]]; then
    build_args+=(--build-arg "BASE_IMAGE=$UAV_BASE_IMAGE")
fi

run_container() {
    local tty=()
    if [[ -t 0 && -t 1 ]]; then
        tty=(-it)
    fi
    # shellcheck disable=SC2086
    docker run --rm "${tty[@]}" \
        --cap-add=SYS_PTRACE --security-opt seccomp=unconfined \
        --add-host=host.docker.internal:host-gateway \
        -v "$(host_path "$REPO_ROOT"):/workspace" \
        -w /workspace \
        ${UAV_DOCKER_ARGS:-} \
        "$IMAGE" "$@"
}

case "${1:-}" in
build)
    docker build "${build_args[@]}" -t "$IMAGE" "$(host_path "$REPO_ROOT/docker")"
    ;;
shell)
    run_container bash
    ;;
run)
    shift
    [[ $# -gt 0 ]] || { echo "usage: $0 run CMD..." >&2; exit 2; }
    run_container bash -c "$*"
    ;;
*)
    sed -n '2,13p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
    exit 2
    ;;
esac
