# flight-controller — 飞控固件

运行在 MCU 上的 FreeRTOS 固件（C++17），负责传感器采集、姿态控制和电机输出。同一份固件也能编译成 Linux 程序（SITL，`bsp/sitl`），和模拟器一起在电脑上运行。

## 分层

```
app/        FreeRTOS 任务（ControlTask、TelemetryTask）、运行时支持（new/delete → FreeRTOS 堆）
  │
  ├── core/        纯算法：FlightControl、Pid、QuadXMixer、esc/ 电调协议层（不包含 FreeRTOS 或硬件头文件）
  └── bsp/
      ├── include/rfs/fc/bsp/board.hpp     硬件抽象接口 rfs::fc::Board
      ├── include/rfs/fc/bsp/esc_bus.hpp   电调信号传输接口 rfs::fc::EscBus
      ├── include/rfs/fc/bsp/rtos_time.hpp 超时换算 timeout_ticks()（tick 只有 50 Hz）
      ├── sim/                             仿真板：SimBoard + SimBackend
      ├── sitl/                            SITL：SimBoard + UDP，FreeRTOS POSIX port，Linux 进程
      ├── _template/                       新板子模板
      └── <board>/                         真实硬件
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
| 换飞控板 | 新板子实现 `rfs::fc::EscBus`（只负责产生波形） |
| 飞控算法 | 不用改，永远只输出 0~1 |

- 上锁时所有电机发"停转"（DShot 0 / 最小脉宽）；解锁后油门 0 对应 DShot 48，也就是怠速，电机会转
- DShot 特殊命令（蜂鸣找机、改转向等）用 `EscOutput::send_command()`，只能在上锁时发，设置类命令要连续发 `kDshotCommandRepeat` 次
- 双向 DShot（读回 eRPM，用于转速陷波滤波）：接口已经预留（`EscBus::read_erpm()` / `EscOutput::read_erpm()`），板级实现之后再做
- 仿真板 `SimBoard` 直接把 0~1 发给模拟器，不经过协议层

## 为仿真预留的设计

1. **算法是纯函数**：`rfs::fc::FlightControl::update(Input, dt) → ActuatorOutputs` 不读时钟、不碰硬件、不调用 RTOS。仿真器可以在进程内直接调用它（算法闭环），单元测试也是这样测的。中间信号通过 `debug()` 暴露出来，方便画图。
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
scripts/build.sh flight-controller -t
# 构建目录：build/host/flight-controller
```

SITL（主机编译器 + FreeRTOS POSIX port，需要 FreeRTOS-Kernel 子模块）：

```bash
scripts/build.sh --sitl         # 产物：build/sitl/flight-controller/flight_controller（Linux 程序）
scripts/sitl.sh --check         # 和模拟器锁步飞一段，并和进程内算法闭环比对轨迹
```

固件（需要 `arm-none-eabi-gcc`、FreeRTOS-Kernel 子模块，以及 `bsp/<board>/`）：

```bash
scripts/build.sh --fw stm32f4
# 产物：build/fw-stm32f4/flight-controller/flight_controller.{elf,bin,hex,map}
```

在 VS Code 里，CMake Tools 会读取 [CMakePresets.json](CMakePresets.json)，预设的构建目录和 `build.sh` 相同。

不设置 `FC_BOARD` 就是主机构建；设置了就构建固件。

## 任务

| 任务 | 触发方式 | 优先级 | 说明 |
|------|----------|--------|------|
| `control` | `Board::wait_imu()`（约 1 kHz） | 最高 | IMU + 遥控 → `FlightControl::update()` → 电机；IMU 超时就关电机 |
| `telemetry` | 100 ms | 低 | 向机载计算机发送心跳 |

### RTOS tick：50 Hz

`configTICK_RATE_HZ` 是 50（每个 tick 20 ms）。控制环不依赖 tick，它由 `wait_imu()` 驱动（IMU 中断 / 锁步数据包），所以 tick 只影响 `vTaskDelay()`、软件定时器、阻塞超时和同优先级任务的时间片，分辨率都是 20 ms。

- `pdMS_TO_TICKS()` 向下取整：`pdMS_TO_TICKS(10) == 0`，也就是"不等待"。**超时一律用 `rfs::fc::timeout_ticks(ms)`**（[rtos_time.hpp](bsp/include/rfs/fc/bsp/rtos_time.hpp)）：向上取整再加 1 个 tick，保证至少等够 `ms`
- 所以 IMU 超时（`kImuTimeoutMs` = 10 ms）实际要 20～40 ms 才会触发
- 周期性任务的周期要是 20 ms 的整数倍（`telemetry` 里有 `static_assert`）

### SITL 的运行方式

`bsp/sitl` 复用 `SimBoard`，传输层是 UDP（[udp_sim_backend.hpp](bsp/sitl/udp_sim_backend.hpp)），每个 UDP 包是一个 `rfs::common` 帧。POSIX port 下每个任务是一个 pthread，任务阻塞在系统调用里时 FreeRTOS 仍然认为它在运行，会饿死低优先级任务，所以控制任务不直接 `recv()`：

```
   rfs_sim --udp                     flight_controller (SITL)
   +-----------+   SIM_SENSORS   +----------------------------------+
   | UdpLink   | --------------> | IDLE task: poll() on the socket  |  相当于 MCU 上的 WFI
   |           |                 |   |  datagram arrived            |
   |           |                 |   v  xTaskNotifyGive()           |  相当于 IMU 中断
   |           |                 | control task: wait_imu() returns |
   |           |  SIM_ACTUATORS  |   FlightControl::update()        |
   |           | <-------------- |   write_motors()                 |
   +-----------+                 +----------------------------------+
```

固件的时间就是数据包里的仿真时间，所以 SITL 和进程内算法闭环（`rfs_sim` 不加 `--udp`）飞出来的轨迹逐字节相同，`scripts/sitl.sh --check`（CI 里也跑）检查的就是这一点。
