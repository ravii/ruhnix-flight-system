# esc — 电调固件

运行在电调 MCU 上的 FreeRTOS 固件（C++17），用 FOC 驱动无刷电机，接收飞控发来的油门指令。

## 实时性设计

| 层 | 运行方式 | 频率 | 内容 |
|----|----------|------|------|
| 电流环 | ADC/PWM 中断里的回调，**不是 RTOS 任务** | 20–40 kHz | `esc::CurrentController`：Clarke/Park → d/q 电流 PI → 电压指令 |
| 监控任务 | FreeRTOS 最高优先级任务 | 1 kHz | 速度环、过流/过温保护、指令解析 |

电流环中断的优先级要**高于** `configMAX_SYSCALL_INTERRUPT_PRIORITY`，这样它不会被内核临界区屏蔽，但相应地，它不能调用任何 FreeRTOS API。

## 为仿真预留的设计

- `esc::CurrentController::update(Input, dt)` 是纯函数（输入相电流和转子角度，输出定子电压），可以直接接到电机电气模型上，在电脑上调电流环
- 硬件通过 [`esc::Board`](bsp/include/esc/bsp/board.hpp) 访问。电流环回调由板子调用：真机上在 PWM 中断里调用，仿真时在电机模型每步之后调用

## 目录

```
core/     FOC 变换、电流环（主机可测）
app/      FreeRTOS 任务 + 电流环回调 + 运行时支持
bsp/      板级支持，实现 esc::Board
config/   FreeRTOSConfig.h
tests/    主机端单元测试
```

## 构建

```bash
# 主机单元测试
cmake -S esc -B build/esc-host && cmake --build build/esc-host
ctest --test-dir build/esc-host --output-on-failure

# 固件
cmake -S esc -B build/esc-fw \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchains/arm-none-eabi.cmake \
      -DESC_BOARD=stm32g4
cmake --build build/esc-fw
```

新增板子的方法和飞控相同（见 [../flight-controller/bsp/README.md](../flight-controller/bsp/README.md)），区别是库目标名为 `esc_bsp`，port 变量名为 `ESC_FREERTOS_PORT`。
