# 开发环境

日常开发在 **WSL2（Ubuntu 26.04）** 里进行。**Docker 镜像**（同样是 Ubuntu 26.04）用于 CI 和"在干净环境里复现"。

两边的工具链都由同一个脚本 [docker/setup_ubuntu.sh](../docker/setup_ubuntu.sh) 安装，所以版本完全一致：

| 用途 | 工具 |
|------|------|
| 主机构建 | gcc/g++ 15、CMake 4.2、Ninja、gdb |
| 固件（Cortex-M） | arm-none-eabi-gcc 14.2、newlib、ARM 版 libstdc++ |
| 机载计算机（ARM64 Linux） | aarch64-linux-gnu-g++、qemu-user |
| 调试与烧录 | gdb-multiarch、OpenOCD |
| 格式化 | clang-format 20.1.8（安装在 `/opt/venv`，版本和 `scripts/format.sh` 一致） |
| GitHub | gh |

## WSL2（日常开发）

### 第一次配置

```powershell
# Windows PowerShell：安装发行版（已装好就跳过）
wsl --install -d Ubuntu-26.04
```

```bash
# WSL 里：仓库要放在 WSL 自己的文件系统里（不要放在 /mnt/c、/mnt/d 下，那里读写很慢）
git clone https://github.com/ravii/uav-flight.git ~/uav-flight
cd ~/uav-flight
sudo bash docker/setup_ubuntu.sh            # 安装工具链
git config core.hooksPath scripts/git-hooks # 提交前检查格式
exec $SHELL                                 # 重新加载 PATH（/opt/venv/bin）和 QEMU_LD_PREFIX
scripts/build.sh -t                         # 构建并测试所有模块
```

日常用 `scripts/build.sh`：不加选项时用主机 gcc 以 Debug 构建所有模块，**不会调用任何芯片工具链**，产物在 `build/host/<模块>/`。`-t` 运行测试，`-r` 构建 Release，`-c` 先清理再构建，后面跟模块名可以只构建这几个模块。`--arm64` 交叉编译感知和定位（`build/arm64/`），`--fw <board>` 编译飞控固件（`build/fw-<board>/`）。完整说明见 `scripts/build.sh -h`。

`setup_ubuntu.sh` 把环境变量写在 `/etc/profile.d/uav-flight.sh`，并且让 `/etc/zsh/zshenv` 和 `/etc/bash.bashrc` 都去加载它，所以 zsh、bash 和 VS Code 终端都能拿到。即使没拿到，`scripts/format.sh` 也会自己去 `/opt/venv/bin` 下找 clang-format。

### 从 Windows 迁移过来时

- **git 身份和凭据**：WSL 里的 `~/.gitconfig` 是新的。身份照抄 Windows 上的配置；凭据复用 Windows 的 Git Credential Manager，这样不用重新登录 GitHub：

  ```bash
  git config --global user.name  "<name>"
  git config --global user.email "<email>"
  git config --global credential.helper "/mnt/c/Program\ Files/Git/mingw64/bin/git-credential-manager.exe"
  ```

- **Docker**：要先在 Windows 上把 **Docker Desktop 本身**打开（只有 `com.docker.service` 在跑是不够的），WSL 里才会出现 `docker` 命令。见下文 [Docker](#dockerci--干净环境)。
- **VS Code**：本地的 `.vscode/settings.json` 不进版本库。在里面把 `clang-format.executable` 设成 `/opt/venv/bin/clang-format`，保存时格式化就会和 CI 用同一个版本。

### 备选：在 Windows 上编辑，在 WSL 里构建（同一份代码）

> 日常开发用上面那份 WSL 里的 `~/uav-flight`。这一节只适合临时情况：手头只有 Windows 盘上的 checkout（比如 `D:\Dev\...`），又要构建或调试它。

这时不需要在 WSL 里再 clone 一份，WSL 可以通过 `/mnt/d/...` 直接构建这份代码：

```bash
# Windows 的 Git Bash 里，在仓库根目录执行
scripts/wsl.sh scripts/build_all.sh        # 在 WSL 里构建并测试全部模块
scripts/wsl.sh scripts/build_all.sh simulator  # 只构建某个模块
scripts/wsl.sh                             # 进入 WSL shell，当前目录就是这个仓库，可以直接用 gdb
```

- 源码只有一份，WSL 从 `/mnt/<盘符>/...` 读取。构建产物放在 WSL 自己的文件系统里（`BUILD_ROOT` 默认是 `~/uav-build`），所以速度不受影响
- 调试时在 WSL 里执行，比如 `gdb --args ~/uav-build/host/flight-controller/fc_test_core`
- VS Code 的 `Ctrl+Shift+B` 会调用 `scripts/wsl.sh` 构建，编译错误会映射回 Windows 上的文件（配置在本地的 `.vscode/tasks.json` 里）
- Windows 上的 C/C++ 扩展只负责代码跳转（`.vscode/c_cpp_properties.json`），**不要在 Windows 上用 MSVC 构建**
- git 操作在 Windows 上做，不要在 WSL 里对 `/mnt/d/...` 执行 git（两边的权限位和换行符处理不一样）

### VS Code

在 WSL 里执行 `code ~/uav-flight`，VS Code 会通过 **WSL 扩展**（Windows 上要装 `ms-vscode-remote.remote-wsl`）连进 WSL，然后提示安装 [.vscode/extensions.json](../.vscode/extensions.json) 里推荐的扩展（CMake Tools、C/C++、clang-format、Claude Code）。这些扩展装在 WSL 里，只需要装一次。

### Claude Code

在 WSL 里要安装 **Linux 版**。注意 WSL 默认会把 Windows 的 PATH 也加进来，所以 `which claude` 可能找到的是 Windows 那份（`/mnt/c/.../npm/claude`），那份不能用来操作 WSL 里的文件。

```bash
curl -fsSL https://claude.ai/install.sh | bash
claude        # 第一次运行时登录
```

Claude Code 的会话和记忆存在 `~/.claude/projects/<按项目路径生成的目录名>/` 下，所以 WSL 里的 `~/uav-flight` 和 Windows 上的 `D:\...` 是两套独立的记录。

### 网络

如果 Windows 上开了只监听 127.0.0.1 的代理，WSL 默认的 NAT 网络模式用不了它。需要的话在 `C:\Users\<you>\.wslconfig` 里加上下面的配置，然后执行 `wsl --shutdown` 重启 WSL：

```ini
[wsl2]
networkingMode=mirrored
```

## Docker（CI / 干净环境）

Docker Desktop 打开 **Settings → Resources → WSL integration** 里对应发行版的开关后，就能在 WSL 里直接用 `docker`：

```bash
scripts/docker.sh build                    # 构建镜像
scripts/docker.sh run scripts/build_all.sh # 用和 CI 完全一样的环境构建并测试
scripts/docker.sh shell                    # 进入容器
```

**Docker Hub 连不上时**，换成 AWS 上的官方镜像站：

```bash
UAV_BASE_IMAGE=public.ecr.aws/docker/library/ubuntu:26.04 scripts/docker.sh build
```

[.devcontainer/](../.devcontainer/) 也保留着，在 VS Code 里可以用 **Reopen in Container** 进入同一个镜像。不过扩展和 Claude Code 都要在容器里单独装，所以日常开发还是推荐用 WSL。

CI（[.github/workflows/ci.yml](../.github/workflows/ci.yml)）在这个镜像里执行 `scripts/format.sh --check` 和 `scripts/build_all.sh`。

## 调试

### 主机程序（单元测试、仿真器、感知/定位的主机版本）

```bash
gdb --args build/host/flight-controller/fc_test_core
```

在 VS Code 里用 C/C++ 扩展的 `cppdbg` 配置（`"MIMode": "gdb"`）就能下断点。

### ARM64 程序（在 x86 电脑上运行）

交叉编译出来的感知、定位程序可以用 qemu 直接运行，库路径已经通过 `QEMU_LD_PREFIX` 设好：

```bash
cmake -S localization -B build/loc-arm64 -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchains/aarch64-linux-gnu.cmake
cmake --build build/loc-arm64
qemu-aarch64 build/loc-arm64/localization_tests

# 调试：qemu 开 gdb 端口，再用 gdb-multiarch 连上去
qemu-aarch64 -g 1234 build/loc-arm64/localization_tests &
gdb-multiarch build/loc-arm64/localization_tests -ex "target remote :1234"
```

### 固件（真实飞控板）

调试器（ST-Link、J-Link）是 USB 设备，要先用 [usbipd-win](https://github.com/dorssel/usbipd-win) 把它挂到 WSL 里：

```powershell
# Windows PowerShell（管理员）
usbipd list
usbipd bind --busid <BUSID>
usbipd attach --wsl --busid <BUSID>
```

```bash
# WSL 里
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg &
gdb-multiarch build/fc-fw/flight_controller.elf -ex "target extended-remote :3333"
```

在 Docker 容器里调试，要再把 USB 设备传进去：`UAV_DOCKER_ARGS="--privileged -v /dev/bus/usb:/dev/bus/usb" scripts/docker.sh shell`。

### SITL（规划中）

SITL 用 FreeRTOS 的 POSIX port，需要 Linux 环境，WSL 和 Docker 都满足。
