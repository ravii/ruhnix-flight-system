# Development / CI image for uav-flight: build, run and debug every module.
#
#   scripts/docker.sh build     # build this image
#   scripts/docker.sh shell     # interactive shell with the repo at /workspace
#
# The toolchain itself is installed by setup_ubuntu.sh, which is also what
# WSL2 / native Ubuntu machines run, so both environments stay identical:
#   host        gcc/g++ 15, cmake, ninja, gdb           common, simulator, unit tests, SITL
#   firmware    arm-none-eabi-gcc 14 + newlib/libstdc++ flight-controller
#   companion   aarch64-linux-gnu-g++ + qemu-user        perception, localization (ARM64)
#   debug       gdb-multiarch, openocd                  on-target firmware debugging
#   format      clang-format (pinned, from PyPI)        must match scripts/format.sh

# Override when Docker Hub is unreachable, e.g.
#   UAV_BASE_IMAGE=public.ecr.aws/docker/library/ubuntu:26.04 scripts/docker.sh build
ARG BASE_IMAGE=ubuntu:26.04
FROM ${BASE_IMAGE}

ARG USERNAME=dev
ARG USER_UID=1000
ARG USER_GID=1000

ENV LANG=C.UTF-8 \
    LC_ALL=C.UTF-8

COPY setup_ubuntu.sh /tmp/setup_ubuntu.sh
RUN bash /tmp/setup_ubuntu.sh && rm -rf /var/lib/apt/lists/* /tmp/setup_ubuntu.sh

# setup_ubuntu.sh writes these to /etc/profile.d for login shells; containers
# usually run non-login shells, so set them here as well.
ENV PATH=/opt/venv/bin:${PATH} \
    QEMU_LD_PREFIX=/usr/aarch64-linux-gnu

# The repository is bind-mounted with the host's ownership; let git use it.
RUN git config --system --add safe.directory '*'

# Non-root user. Ubuntu images ship a default 'ubuntu' user with UID 1000; replace it.
RUN if id -u ubuntu >/dev/null 2>&1; then userdel -r ubuntu; fi \
    && groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m -s /bin/bash ${USERNAME} \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME}

USER ${USERNAME}
WORKDIR /workspace
CMD ["bash"]
