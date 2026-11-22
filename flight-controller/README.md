# flight-controller — 飞控固件

运行在 MCU 上的 FreeRTOS 固件，负责传感器采集、姿态估计、姿态/角速度控制和电机输出。

## 分层

```
app/      FreeRTOS 任务：控制环（1 kHz）、遥测（10 Hz）…
  │
  ├── core/    平台无关算法：PID、混控器 …（不得包含 FreeRTOS/硬件头文件）
  └── bsp/     板级支持：实现 bsp/include/fc_bsp.h
config/   FreeRTOSConfig.h
tests/    core/ 的主机端单元测试
```

`core/` 保持与平台无关，所以算法可以直接在 PC 上测试，以后也能复用到 SITL 仿真中。

## 构建

主机端单元测试：

```bash
cmake -S flight-controller -B build/fc-host
cmake --build build/fc-host
ctest --test-dir build/fc-host --output-on-failure
```

固件（需要 `arm-none-eabi-gcc`、FreeRTOS-Kernel 子模块，以及 `bsp/<board>/`）：

```bash
cmake -S flight-controller -B build/fc-fw \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-none-eabi.cmake \
      -DFC_BOARD=stm32f4
cmake --build build/fc-fw
# 产物：build/fc-fw/flight_controller.{elf,bin,hex,map}
```

## 任务与优先级

| 任务 | 周期 | 优先级 | 说明 |
|------|------|--------|------|
| `control` | 1 ms | 最高 | IMU 读取 → 角速度 PID → 混控 → 电机 |
| `telemetry` | 100 ms | 低 | 向机载计算机发送心跳 |
