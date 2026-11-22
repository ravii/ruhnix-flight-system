# esc — 电调固件

运行在电调 MCU 上的 FreeRTOS 固件，用 FOC 驱动无刷电机，接收飞控发来的油门指令。

## 实时性设计

| 层 | 运行方式 | 频率 | 内容 |
|----|----------|------|------|
| 电流环 | ADC/PWM 中断，**不在 RTOS 任务里** | 20–40 kHz | Clarke/Park → d/q 电流 PI → SVPWM |
| 监控任务 | FreeRTOS 最高优先级任务 | 1 kHz | 速度环、过流/过温保护、指令解析 |

电流环中断的优先级要**高于** `configMAX_SYSCALL_INTERRUPT_PRIORITY`，这样它不会被内核临界区屏蔽，但相应地，它不能调用任何 FreeRTOS API。

## 目录

```
core/     FOC 变换等平台无关算法（主机可测）
app/      FreeRTOS 任务 + 电流环回调
bsp/      板级支持，实现 bsp/include/esc_bsp.h
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
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
      -DESC_BOARD=stm32g4
cmake --build build/esc-fw
```

新增板子的方法和飞控相同（见 [../flight-controller/bsp/README.md](../flight-controller/bsp/README.md)），区别是库目标名为 `esc_bsp`，port 变量名为 `ESC_FREERTOS_PORT`。
