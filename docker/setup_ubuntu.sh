#!/usr/bin/env bash
# Installs the uav-flight toolchain on Ubuntu 26.04.
#
# Single source of truth for the development environment, used by:
#   - docker/Dockerfile               (dev / CI image)
#   - WSL2 or a native Ubuntu machine: sudo bash docker/setup_ubuntu.sh
#
# Keep CLANG_FORMAT_VERSION in sync with scripts/format.sh.

set -euo pipefail

CLANG_FORMAT_VERSION="${CLANG_FORMAT_VERSION:-20.1.8}"
VENV=/opt/venv

if [[ $EUID -ne 0 ]]; then
    echo "run as root: sudo bash $0" >&2
    exit 1
fi

export DEBIAN_FRONTEND=noninteractive

packages=(
    # host build
    build-essential cmake ninja-build gdb pkg-config
    # firmware (Cortex-M)
    gcc-arm-none-eabi binutils-arm-none-eabi
    libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
    # companion computer (ARM64 Linux)
    gcc-aarch64-linux-gnu g++-aarch64-linux-gnu qemu-user
    # debugging and flashing
    gdb-multiarch openocd
    # tooling
    ca-certificates curl git less python3 python3-venv sudo
)

apt-get update
apt-get install -y --no-install-recommends "${packages[@]}"

# Pinned clang-format in a venv (Ubuntu's packaged clang-format is a different
# version and would format differently from CI).
if [[ ! -x "$VENV/bin/python" ]]; then
    python3 -m venv "$VENV"
fi
"$VENV/bin/pip" install --no-cache-dir --upgrade "clang-format==${CLANG_FORMAT_VERSION}"

# Environment for login shells (WSL / native). The Docker image sets the same via ENV.
cat > /etc/profile.d/uav-flight.sh <<EOF
# Added by uav-flight docker/setup_ubuntu.sh
case ":\$PATH:" in *":$VENV/bin:"*) ;; *) export PATH="$VENV/bin:\$PATH" ;; esac
export QEMU_LD_PREFIX=/usr/aarch64-linux-gnu
EOF

echo "uav-flight toolchain installed:"
gcc --version | head -1
arm-none-eabi-gcc --version | head -1
aarch64-linux-gnu-gcc --version | head -1
cmake --version | head -1
"$VENV/bin/clang-format" --version
