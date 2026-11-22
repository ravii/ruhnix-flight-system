#!/usr/bin/env bash
# Installs the Ruhnix Flight System (RFS) toolchain on Ubuntu 26.04.
#
# Single source of truth for the development environment, used by:
#   - docker/Dockerfile               (dev / CI image)
#   - WSL2 or a native Ubuntu machine: sudo bash docker/setup_ubuntu.sh
#
# Keep CLANG_FORMAT_VERSION in sync with scripts/format.sh.

set -euo pipefail

CLANG_FORMAT_VERSION="${CLANG_FORMAT_VERSION:-20.1.8}"
MARKDOWN_VERSION="${MARKDOWN_VERSION:-3.11.3}"
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
    ca-certificates curl gh git less python3 python3-venv sudo
)

apt-get update
apt-get install -y --no-install-recommends "${packages[@]}"

# Pinned clang-format in a venv (Ubuntu's packaged clang-format is a different
# version and would format differently from CI).
if [[ ! -x "$VENV/bin/python" ]]; then
    python3 -m venv "$VENV"
fi
"$VENV/bin/pip" install --no-cache-dir --upgrade "clang-format==${CLANG_FORMAT_VERSION}"
# scripts/confluence_sync.py (Markdown -> Confluence storage format)
"$VENV/bin/pip" install --no-cache-dir --upgrade "markdown==${MARKDOWN_VERSION}"

# The project used to be called uav-flight: drop its environment file and hooks.
rm -f /etc/profile.d/uav-flight.sh
for rc in /etc/zsh/zshenv /etc/bash.bashrc; do
    if [[ -f "$rc" ]]; then
        sed -i '/uav-flight\.sh.*# uav-flight$/d' "$rc"
    fi
done

# Environment for login shells (WSL / native). The Docker image sets the same via ENV.
ENV_FILE=/etc/profile.d/rfs.sh
cat > "$ENV_FILE" <<EOF
# Added by rfs docker/setup_ubuntu.sh
case ":\$PATH:" in *":$VENV/bin:"*) ;; *) export PATH="$VENV/bin:\$PATH" ;; esac
export QEMU_LD_PREFIX=/usr/aarch64-linux-gnu
EOF

# /etc/profile.d is only read by login bash. Also load it from every zsh
# (zshenv) and from interactive non-login bash (VS Code terminals).
hook_line="[ -r $ENV_FILE ] && . $ENV_FILE  # rfs"
for rc in /etc/zsh/zshenv /etc/bash.bashrc; do
    if [[ -f "$rc" ]] && ! grep -qF "$hook_line" "$rc"; then
        printf '\n%s\n' "$hook_line" >> "$rc"
    fi
done

echo "RFS toolchain installed:"
gcc --version | head -1
arm-none-eabi-gcc --version | head -1
aarch64-linux-gnu-gcc --version | head -1
cmake --version | head -1
"$VENV/bin/clang-format" --version
