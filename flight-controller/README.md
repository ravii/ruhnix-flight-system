# flight-controller — 飞控固件

运行在 MCU 上的 FreeRTOS 固件（C++17），负责传感器采集、姿态控制和电机输出。

## 分层

```
app/        FreeRTOS 任务（ControlTask、TelemetryTask）、运行时支持（new/delete → FreeRTOS 堆）
  │
  ├── core/        纯算法：FlightControl、Pid、QuadXMixer、esc/ 电调协议层（不包含 FreeRTOS 或硬件头文件）
  └── bsp/
      ├── include/fc/bsp/board.hpp   硬件抽象接口 fc::Board
      ├── include/fc/bsp/esc_bus.hpp 电调信号传输接口 fc::EscBus
      ├── sim/                       仿真板：SimBoard + SimBackend
      ├── _template/                 新板子模板
      └── <board>/                   真实硬件
config/     FreeRTOSConfig.h、esc_config.hpp（电调协议和电机接线）
tests/      主机端单元测试
```

## 电机输出（成品电调）

```
   +----------------------------+
   |  FlightControl::update()   |
   +----------------------------+
                 |
                 | ActuatorOutputs: motors[i] in [0, 1], armed
                 v
   +----------------------------+
   |   Board::write_motors()    |
   +----------------------------+
           |               |
           | real board    | simulation
           v               v
   +----------------+   +---------------------+
   | EscOutput      |   | SimBoard            |
   | (core/esc)     |   | sends 0..1 to the   |
   | map + encode   |   | simulator unchanged |
   +----------------+   +---------------------+
           |
           | EscFrame: DShot frames / PWM pulse widths (ns)
           v
   +----------------+
   | EscBus         |
   | (bsp/<board>)  |
   | timers + DMA   |
   +----------------+
           |
           | DShot / PWM signal
           v
      ESC x N --> motors
```

| 要改的东西 | 改哪里 |
|------------|--------|
| 换电调、换协议（PWM / OneShot125 / DShot150/300/600）、电机接线顺序 | [config/esc_config.hpp](config/esc_config.hpp) |
| 换飞控板 | 新板子实现 `fc::EscBus`（只负责产生波形） |
| 飞控算法 | 不用改，永远只输出 0~1 |

- 上锁时所有电机发"停转"（DShot 0 / 最小脉宽）；解锁后油门 0 对应 DShot 48，也就是怠速，电机会转
- DShot 特殊命令（蜂鸣找机、改转向等）用 `EscOutput::send_command()`，只能在上锁时发，设置类命令要连续发 `dshot::kCommandRepeat` 次
- 双向 DShot（读回 eRPM，用于转速陷波滤波）：接口已经预留（`EscBus::read_erpm()` / `EscOutput::read_erpm()`），板级实现之后再做
- 仿真板 `SimBoard` 直接把 0~1 发给模拟器，不经过协议层

## 为仿真预留的设计

1. **算法是纯函数**：`fc::FlightControl::update(Input, dt) → ActuatorOutputs` 不读时钟、不碰硬件、不调用 RTOS。仿真器可以在进程内直接调用它（算法闭环），单元测试也是这样测的。中间信号通过 `debug()` 暴露出来，方便画图。
2. **控制环由 IMU 驱动**：`ControlTask` 阻塞在 `Board::wait_imu()` 上，而不是阻塞在 RTOS 定时器上。真机上它由 IMU 的数据就绪中断唤醒，仿真里由模拟器发来的传感器包唤醒，所以固件不用改就能和模拟器锁步运行。
3. **时间来自 Board**：`Board::time_us()` 在仿真里返回的是仿真时间。

详细说明见 [../simulator/README.md](../simulator/README.md)。

## C++ 使用约定

- C++17，编译选项 `-fno-exceptions -fno-rtti`
- 调度器启动后不做堆分配；不用 `std::vector`、`std::string`、`std::function`、`iostream`
- 用 `std::array`、`constexpr` 和静态对象；`operator new` 已经接到 `pvPortMalloc`（[app/runtime.cpp](app/runtime.cpp)）
- `configASSERT()` 里面不要写有副作用的表达式
- 协议结构体是 packed 的：逐个元素复制，不要对它的成员取引用或取指针

## 构建

主机端（core + SimBoard + 单元测试）：

```bash
cmake -S flight-controller -B build/fc-host
cmake --build build/fc-host
ctest --test-dir build/fc-host --output-on-failure
```

固件（需要 `arm-none-eabi-gcc`、FreeRTOS-Kernel 子模块，以及 `bsp/<board>/`）：

```bash
cmake -S flight-controller -B build/fc-fw \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchains/arm-none-eabi.cmake \
      -DFC_BOARD=stm32f4
cmake --build build/fc-fw
# 产物：build/fc-fw/flight_controller.{elf,bin,hex,map}
```

不设置 `FC_BOARD` 就是主机构建；设置了就构建固件。

## 任务

| 任务 | 触发方式 | 优先级 | 说明 |
|------|----------|--------|------|
| `control` | `Board::wait_imu()`（约 1 kHz） | 最高 | IMU + 遥控 → `FlightControl::update()` → 电机；IMU 超时就关电机 |
| `telemetry` | 100 ms | 低 | 向机载计算机发送心跳 |
