# uav-flight

一个自研多旋翼无人机项目，包含 **飞控**、**电调**、**感知**、**定位** 四个模块。

每个模块都是**独立编译、独立部署**的工程，可以运行在不同的板子上。它们放在同一个仓库里，共享通信协议和消息定义，这样跨模块的接口改动可以在一次提交里完成。

## 模块总览

| 模块 | 目录 | 运行平台 | 操作系统 | 语言 | 工具链 |
|------|------|----------|----------|------|--------|
| 飞控 Flight Controller | [flight-controller/](flight-controller/) | MCU (Cortex-M4F/M7) | FreeRTOS | C11 | `arm-none-eabi-gcc` |
| 电调 ESC | [esc/](esc/) | MCU (Cortex-M4F) | FreeRTOS | C11 | `arm-none-eabi-gcc` |
| 感知 Perception | [perception/](perception/) | 机载计算机 (ARM64) | Linux | C++17 | `aarch64-linux-gnu-g++` |
| 定位 Localization | [localization/](localization/) | 机载计算机 (ARM64) | Linux | C++17 | `aarch64-linux-gnu-g++` |
| 公共库 Common | [common/](common/) | 所有平台 | 与平台无关 | C11 | 随使用方 |

## 目录结构

```
uav-flight/
├── common/               # 跨模块共享：通信协议、消息定义、CRC（纯 C，无 OS 依赖）
├── flight-controller/    # 飞控固件（FreeRTOS）
│   ├── core/             #   平台无关的算法（姿态控制、PID …），可在主机上做单元测试
│   ├── app/              #   FreeRTOS 任务与应用入口
│   ├── bsp/              #   板级支持包（启动文件、链接脚本、HAL），按板子分子目录
│   ├── config/           #   FreeRTOSConfig.h 等配置
│   └── tests/            #   主机端单元测试
├── esc/                  # 电调固件（FreeRTOS），布局同飞控
├── perception/           # 感知应用（Linux）
├── localization/         # 定位应用（Linux）
├── cmake/                # 公共 CMake 脚本与交叉编译工具链文件
│   └── toolchains/
├── third_party/          # 第三方依赖（FreeRTOS-Kernel 等，用 git submodule 引入）
├── docs/                 # 设计文档
├── tools/                # 上位机/调试工具
└── scripts/              # 构建、烧录、部署脚本
```

## 构建

所有模块都使用 CMake（≥ 3.21）。**各模块单独构建**，在模块目录下执行即可。

### 主机构建（单元测试 / 开发调试）

固件模块中与平台无关的 `core/` 部分可以直接在主机上编译和测试：

```bash
cmake -S flight-controller -B build/fc-host
cmake --build build/fc-host
ctest --test-dir build/fc-host --output-on-failure
```

Linux 应用同理：

```bash
cmake -S perception -B build/perception-host
cmake --build build/perception-host
ctest --test-dir build/perception-host --output-on-failure
```

### 交叉编译固件（FreeRTOS）

先拉取 FreeRTOS 内核：

```bash
git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git third_party/FreeRTOS-Kernel
git submodule update --init --recursive
```

然后在 `flight-controller/bsp/<板子名>/` 下放入板级支持包（详见 [flight-controller/bsp/README.md](flight-controller/bsp/README.md)），再执行：

```bash
cmake -S flight-controller -B build/fc-fw \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
      -DFC_BOARD=stm32f4
cmake --build build/fc-fw
```

### 交叉编译 Linux 应用

```bash
cmake -S localization -B build/loc-arm64 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake
cmake --build build/loc-arm64
```

## 模块间通信

模块之间的接口定义集中放在 [common/protocol/](common/protocol/)，具体见 [docs/architecture.md](docs/architecture.md)。

- 飞控 ↔ 电调：DShot / CAN（待定）
- 飞控 ↔ 机载计算机：UART，使用 `common/protocol` 中定义的二进制帧
- 感知 ↔ 定位：同一台机器上的进程间通信（待定）

## 开发约定

- **代码风格**：仓库根目录的 [.clang-format](.clang-format)，提交前执行 `clang-format -i`
- **换行符**：统一使用 LF（由 [.gitattributes](.gitattributes) 保证）
- **分支**：`main` 保持可构建；功能开发在 `feat/<模块>-<描述>` 分支上进行
- **提交信息**：`<模块>: <描述>`，例如 `fc: add attitude rate controller`
- **固件代码**：`core/` 中禁止依赖 FreeRTOS 和硬件，保证能在主机上测试；只在初始化阶段分配内存，调度器启动后不再动态分配

## 路线图

- [ ] 通信协议 v1 定稿
- [ ] 飞控：IMU 驱动 + 姿态解算 + 角速度环
- [ ] 电调：六步换相 / FOC 电流环
- [ ] 定位：IMU + 视觉/GNSS 融合
- [ ] 感知：相机采集 + 障碍物检测
- [ ] SITL 软件在环仿真

## 许可证

待定。
