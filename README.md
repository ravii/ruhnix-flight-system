# Ruhnix Flight System (RFS)

一个自研多旋翼无人机项目，包含 **飞控**、**感知**、**定位** 三个模块，外加一个用于调试的 **仿真器**。电调使用成品（BLHeli_32 / AM32 等），由飞控通过 DShot 驱动，本仓库不包含电调固件。

每个模块都是**独立编译、独立部署**的工程，可以运行在不同的板子上。它们放在同一个仓库里，共享通信协议和消息定义，这样跨模块的接口改动可以在一次提交里完成。

## 模块总览

| 模块 | 目录 | 运行平台 | 操作系统 | 语言 | 工具链 |
|------|------|----------|----------|------|--------|
| 飞控 Flight Controller | [flight-controller/](flight-controller/) | MCU (Cortex-M4F/M7) | FreeRTOS | C++17 | `arm-none-eabi-g++` |
| 感知 Perception | [perception/](perception/) | 机载计算机 (ARM64) | Linux | C++17 | `aarch64-linux-gnu-g++` |
| 定位 Localization | [localization/](localization/) | 机载计算机 (ARM64) | Linux | C++17 | `aarch64-linux-gnu-g++` |
| 仿真器 Simulator | [simulator/](simulator/) | 开发电脑 | Linux / Windows | C++17 | 主机编译器 |
| 公共协议 Common | [common/](common/) | 所有平台 | 与平台无关 | C++17 | 随使用方 |

## 目录结构

```
ruhnix-flight-system/
├── common/               # 跨模块共享：通信协议、消息定义、CRC（rfs::common）
├── flight-controller/    # 飞控固件（FreeRTOS）
│   ├── core/             #   纯算法：FlightControl、PID、混控器，可在主机上测试
│   ├── app/              #   FreeRTOS 任务与应用入口
│   ├── bsp/              #   硬件抽象 rfs::fc::Board；sim/ 是仿真板，其他子目录是真实板子
│   ├── config/           #   FreeRTOSConfig.h
│   └── tests/            #   主机端单元测试
├── perception/           # 感知应用（Linux）
├── localization/         # 定位应用（Linux）
├── simulator/            # 仿真器：动力学/传感器模型接口、锁步调度
├── cmake/                # 公共 CMake 脚本与交叉编译工具链文件
├── third_party/          # 第三方依赖（FreeRTOS-Kernel 等，用 git submodule 引入）
├── docker/               # 开发容器镜像（Ubuntu 26.04）
├── .devcontainer/        # VS Code Dev Container 配置
├── docs/                 # 设计文档
├── tools/                # 上位机/调试工具
└── scripts/              # build.sh、build_all.sh、docker.sh、format.sh、git hook
```

## 开发环境（WSL2 + Docker）

- **日常开发在 WSL2（Ubuntu 26.04）里进行**：用 VS Code 的 WSL 扩展连进去编辑、构建、调试
- **Docker 镜像**（同样是 Ubuntu 26.04）用于 CI 和"在干净环境里复现"
- 两边的工具链都由同一个脚本 [docker/setup_ubuntu.sh](docker/setup_ubuntu.sh) 安装，版本一致

```bash
# WSL 里，第一次
git clone https://github.com/ravii/ruhnix-flight-system.git ~/ruhnix-flight-system && cd ~/ruhnix-flight-system
sudo bash docker/setup_ubuntu.sh
git config core.hooksPath scripts/git-hooks

# 日常
scripts/build.sh                           # 主机 Debug 构建所有模块（开发调试用，不调用芯片工具链）
scripts/build.sh -t                        # 构建并运行单元测试（等同于 scripts/build_all.sh）
scripts/build.sh --arm64 / --fw <board>    # 交叉编译 ARM64 应用 / 飞控固件，详见 scripts/build.sh -h
scripts/docker.sh run scripts/build_all.sh # 在和 CI 完全一样的容器里再跑一遍
```

完整说明见 [docs/dev-environment.md](docs/dev-environment.md)，包括 VS Code、Claude Code、Docker Hub 连不上时的处理办法，以及用 gdb、qemu、OpenOCD 和 usbipd 调试的方法。

## 构建

以下命令在 WSL 或容器里执行。所有模块都使用 CMake（≥ 3.21），**各模块单独构建**。

### 主机构建（单元测试 / 开发调试）

```bash
cmake -S flight-controller -B build/fc-host
cmake --build build/fc-host
ctest --test-dir build/fc-host --output-on-failure
```

把 `flight-controller` 换成 `common`、`simulator`、`perception` 或 `localization` 即可构建其他模块。

### 交叉编译固件（FreeRTOS）

先拉取 FreeRTOS 内核：

```bash
git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git third_party/FreeRTOS-Kernel
git submodule update --init --recursive
```

然后在 `flight-controller/bsp/<板子名>/` 下放入板级支持包（详见 [flight-controller/bsp/README.md](flight-controller/bsp/README.md)），再执行：

```bash
cmake -S flight-controller -B build/fc-fw \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchains/arm-none-eabi.cmake \
      -DFC_BOARD=stm32f4
cmake --build build/fc-fw
```

### 交叉编译 Linux 应用

```bash
cmake -S localization -B build/loc-arm64 \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchains/aarch64-linux-gnu.cmake
cmake --build build/loc-arm64
```

## 仿真调试

飞控按"能在模拟器里跑"来设计，可以从三个层次接入模拟器（详见 [simulator/README.md](simulator/README.md)）：

| 层次 | 接入点 | 用途 |
|------|--------|------|
| 算法闭环 | `rfs::fc::FlightControl::update()`，纯函数 | 调算法和参数、CI 回归测试 |
| SITL | `bsp/sim` 的 `SimBoard` + FreeRTOS POSIX port | 整套固件在电脑上运行 |
| HITL | 真实飞控板 + 串口版 `SimBackend` | 上真机前的验证 |

模拟器和飞控之间锁步运行，消息定义在 `common` 中（`MsgId::kSimSensors` / `MsgId::kSimActuators`）。

## 代码格式化

C/C++ 代码统一用 clang-format 格式化，风格配置来自 [devkits](https://github.com/ravii/devkits)（[.clang-format](.clang-format)），**clang-format 版本固定为 20.1.8**。

```bash
scripts/format.sh             # 格式化全部 C/C++ 文件
scripts/format.sh a.cpp b.h   # 只格式化指定文件
scripts/format.sh --check     # 只检查不修改（CI 用）
```

- **clang-format 安装**：Docker 镜像里已经装好；在主机上用的话执行 `pip install clang-format==20.1.8`，Visual Studio 自带的版本也会被自动找到
- **提交前检查**：每个 clone 执行一次 `git config core.hooksPath scripts/git-hooks`，之后提交未格式化的代码会被拒绝
- **Claude Code**：[.claude/settings.json](.claude/settings.json) 里配了 hook，Claude 每次改完 C/C++ 文件都会自动格式化
- **CI**：`format` job 会检查格式
- 第三方代码（`third_party/`、`bsp/*/vendor/`）不做格式化；`FreeRTOSConfig.h` 里的配置表用 `clang-format off` 保留了列对齐

## 开发约定

- **语言**：所有模块都用 C++17
- **命名空间**：每个模块的代码都在 `rfs::<模块>` 里（`rfs::common`、`rfs::fc`、`rfs::sim`、`rfs::perception`、`rfs::localization`），下面不再分层；头文件放在 `include/rfs/<模块>/`
- **换行符**：统一使用 LF（由 [.gitattributes](.gitattributes) 保证）
- **分支**：`main` 保持可构建；功能开发在 `feat/<模块>-<描述>` 分支上进行
- **提交信息**：`<模块>: <描述>`，例如 `fc: add attitude rate controller`
- **固件代码**：`core/` 中禁止依赖 FreeRTOS 和硬件；不用异常和 RTTI；调度器启动后不做堆分配

## 路线图

- [ ] 通信协议 v1 定稿
- [ ] 飞控：IMU 驱动 + 姿态解算 + 角速度环
- [ ] 飞控：电机输出（协议层已完成；板级 `EscBus` 的 DShot 定时器/DMA 驱动待实现）
- [ ] 仿真器：四旋翼动力学模型 + 进程内算法闭环
- [ ] SITL：FreeRTOS POSIX port + UDP SimBackend
- [ ] 定位：IMU + 视觉/GNSS 融合
- [ ] 感知：相机采集 + 障碍物检测

## 许可证

待定。
