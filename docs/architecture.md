# 系统架构

## 硬件与模块部署

```
   camera / depth                     GNSS / VIO
         |                                |
         v                                v
   +------------+     obstacles     +--------------+
   | perception | ----------------> | localization |          机载计算机（ARM64 Linux）
   +------------+                   +--------------+
                                       |        ^
             pose estimate,            |        |    IMU, attitude,
             setpoints                 |  UART  |    heartbeat
                                       v        |
   IMU -----------+           +---------------------------+
                  +---------> |     flight controller     |    飞控 MCU（FreeRTOS）
   RC receiver ---+           |  control task @ 1 kHz     |
                              |  FlightControl -> mixer   |
                              |  -> EscOutput -> EscBus   |
                              +---------------------------+
                                            |
                                            | DShot
          +----------------+----------------+----------------+
          |                |                |                |
          v                v                v                v
      +-------+        +-------+        +-------+        +-------+
      | ESC 0 |        | ESC 1 |        | ESC 2 |        | ESC 3 |      成品电调
      +-------+        +-------+        +-------+        +-------+
          |                |                |                |
          v                v                v                v
       motor 0          motor 1          motor 2          motor 3
```

## 坐标系约定

- 机体系：**FRD**（前-右-下）
- 世界系：**NED**（北-东-地）
- 四元数：`w, x, y, z`，表示机体系 → 世界系的旋转
- 单位：SI（m、m/s、rad、rad/s），时间戳为微秒

## 通信

| 链路 | 物理层 | 协议 | 定义位置 |
|------|--------|------|----------|
| 飞控 ↔ 机载计算机 | UART（≥ 921600 bps） | `rfs::common` 帧 | [frame.hpp](../common/include/rfs/common/frame.hpp) |
| 飞控 → 电调 | 定时器 + DMA 输出 | DShot / PWM（成品电调），在 `config/esc_config.hpp` 中选择 | 协议层 [esc_output.hpp](../flight-controller/core/include/fc/esc/esc_output.hpp)，传输层 [esc_bus.hpp](../flight-controller/bsp/include/fc/bsp/esc_bus.hpp) |
| 感知 ↔ 定位 | 同机进程间通信 | 待定（共享内存 / Unix socket） | — |
| 模拟器 ↔ 飞控 | 进程内 / UDP / UART | `MsgId::kSimSensors`、`MsgId::kSimActuators`，锁步 | [messages.hpp](../common/include/rfs/common/messages.hpp) |

## 仿真

飞控软件分三层，每一层都能单独接到模拟器上：

```
                    +--------------------------------+
                    | app/  ControlTask (FreeRTOS)   |
                    |       blocks on wait_imu()     |
                    +--------------------------------+
                          |                     |
                update()  |                     |  wait_imu() / write_motors()
                          v                     v
   +--------------------------+     +-----------------------+
   | core/  FlightControl     |     |    rfs::fc::Board     |
   |        pure function     |     +-----------------------+
   +--------------------------+          |             |
                ^                        v             v
                |              +-------------+   +--------------+
                |              | bsp/<board> |   | bsp/sim      |
                |              | SPI, timers |   | SimBoard     |
                |              | EscBus      |   | + SimBackend |
                |              +-------------+   +--------------+
                |                                       ^
                |                                       |
   +--------------------------+         +--------------------------------+
   | simulator, in-process    |         | simulator over UDP (SITL)      |
   | (1) algorithm loop       |         | or serial (HITL)   (2) (3)     |
   +--------------------------+         +--------------------------------+
```

(1) 算法闭环：只用 `core/`，不带 RTOS；(2) SITL：整套固件跑在电脑上；(3) HITL：真实飞控板。

关键约定：

- **控制环由数据驱动，不由定时器驱动**：控制任务等待 IMU 数据。模拟器每发一帧传感器数据，飞控就走一步并回复电机输出，模拟器收到回复后才推进物理模型。这就是锁步，不需要改 FreeRTOS 的 tick。
- **时间只来自 `Board::time_us()`**：仿真时它就是仿真时间，所以每次运行的结果都一样。
- **SITL 和 HITL 共用同一套消息**：模拟器这边只需要换 `FcLink` 的实现。

详见 [simulator/README.md](../simulator/README.md)。

## 设计原则

1. **模块独立构建**：每个模块都有自己的 `CMakeLists.txt`，可以单独构建、测试和发布
2. **接口集中定义**：跨模块的数据结构只在 `common/` 中定义一份
3. **算法与平台分离**：固件中的 `core/` 不依赖 RTOS 和硬件，能在主机上测试，也能直接接到模拟器上
4. **硬件只通过接口访问**：`rfs::fc::Board`，真机和仿真各实现一份
5. **实时路径无动态分配**：固件只在初始化阶段分配内存，Linux 应用的实时循环中避免分配
