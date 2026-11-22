# simulator — 仿真器

在电脑上调试飞控算法和整套飞控固件用的仿真器。**目前只定义好了接口和锁步调度**，动力学模型和各种连接方式还没有实现。

## 锁步（lockstep）

```
   +--------------+     truth     +-------------+
   | VehicleModel | ------------> | SensorModel |
   +--------------+               +-------------+
          ^                              |
          |                              | (1) SIM_SENSORS
          |                              v
          |                       +-------------+  (2) send, wait  +-------------------+
          |                       |   FcLink    | ---------------> | flight controller |
          |                       |             | <--------------- |   (under test)    |
          |                       +-------------+      reply       +-------------------+
          |                              |
          |   (3) step(actuators, dt)    | SIM_ACTUATORS
          +------------------------------+
```

每一步：传感器模型对当前真值采样 → 发给飞控并**等待**电机输出 → 物理模型推进 dt → 仿真时间 +dt。

飞控在仿真里拿到的时间就是仿真时间（`SimBoard::time_us()`），永远看不到墙上时钟。所以结果是确定的、可复现的，跑得比实时快或慢都可以。

## 接口

| 接口 | 文件 | 职责 | 计划的实现 |
|------|------|------|------------|
| `VehicleModel` | [vehicle_model.hpp](include/sim/vehicle_model.hpp) | 电机指令 → 刚体状态 | 内置四旋翼模型；Gazebo 等外部引擎桥接 |
| `SensorModel` | [sensor_model.hpp](include/sim/sensor_model.hpp) | 真值 → 带噪声、零偏、延迟的传感器读数 | IMU/气压计/磁力计/GNSS 模型 |
| `FcLink` | [fc_link.hpp](include/sim/fc_link.hpp) | 一次"发传感器、收电机输出" | 见下表 |
| `Simulation` | [simulation.hpp](include/sim/simulation.hpp) | 锁步调度（**已实现**） | — |

## 三种调试方式

| 方式 | `FcLink` 实现 | 飞控侧 | 用途 |
|------|---------------|--------|------|
| 算法闭环 | `InProcessLink`：在进程内直接调用 `rfs::fc::FlightControl::update()` | 只有 `flight-controller/core`，不带 RTOS | 调算法、调参数、在 CI 里做回归测试。最快，可以单步调试 |
| SITL | `UdpLink` | 完整固件 + FreeRTOS POSIX port + `bsp/sim`（UDP 版的 `SimBackend`） | 验证任务调度、通信协议，和感知、定位模块联调 |
| HITL | `SerialLink` | 真实飞控板 + `bsp/sim`（串口版的 `SimBackend`） | 上真机之前的最后一道验证 |

SITL 和 HITL 走的是同一组协议消息（`rfs::common` 里的 `MsgId::kSimSensors` 和 `MsgId::kSimActuators`），所以仿真器这边的代码是同一套。

## 构建

```bash
cmake -S simulator -B build/sim
cmake --build build/sim
ctest --test-dir build/sim --output-on-failure
```
