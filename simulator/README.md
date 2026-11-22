# simulator — 仿真器

在电脑上调试飞控算法和整套飞控固件用的仿真器。**目前已经实现锁步调度、内置四旋翼模型、IMU 模型、进程内算法闭环和 SITL（UDP）连接**；HITL（串口）还没有实现。

## 锁步（lockstep）

<!-- diagram: sim-lockstep -->
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

| 接口 | 文件 | 职责 | 实现 |
|---|---|---|---|
| `VehicleModel` | [vehicle_model.hpp](include/rfs/sim/vehicle_model.hpp) | 电机指令 → 刚体状态 | `QuadrotorModel`（**已实现**）；Gazebo 等外部引擎桥接（计划） |
| `SensorModel` | [sensor_model.hpp](include/rfs/sim/sensor_model.hpp) | 真值 → 带噪声、零偏、延迟的传感器读数 | `ImuSensorModel`（**已实现**）；气压计/磁力计/GNSS（计划） |
| `FcLink` | [fc_link.hpp](include/rfs/sim/fc_link.hpp) | 一次"发传感器、收电机输出" | `InProcessLink`、`UdpLink`（**已实现**），其余见下表 |
| `Simulation` | [simulation.hpp](include/rfs/sim/simulation.hpp) | 锁步调度（**已实现**） | — |

### 四旋翼模型 `QuadrotorModel`

[quadrotor_model.hpp](include/rfs/sim/quadrotor_model.hpp)，参数在 `QuadrotorParams` 里（默认是 1 kg、250 mm 轴距的机架，推重比约 3）：

- 电机编号和转向与飞控的 `QuadXMixer` 一致：0 右前 CCW、1 右后 CW、2 左后 CCW、3 左前 CW
- 每个电机：指令 u ∈ [0, 1] 经过一阶惯性环节得到归一化转速 s，推力 = `max_thrust_n` × s²，反扭矩 = `yaw_moment_per_thrust_m` × 推力
- 刚体：六自由度，包含陀螺耦合 ω × Iω、线性阻力、可选的角阻尼
- 地面：NED z = 0 处是平地，推力小于重力时飞机静止在地面上，加速度计读到 1 g
- 没有建模：地面翻倒和摩擦、桨叶挥舞、电池压降、风

### 算法闭环 `InProcessLink`

[in_process_link.hpp](include/rfs/sim/in_process_link.hpp) 在进程内直接调用 `rfs::fc::FlightControl::update()`，dt 的计算和限幅与固件的 `ControlTask` 相同，没有 IMU 数据时同样会切断电机。simulator 通过 `rfs_use_fc_core()`（[cmake/RfsCommon.cmake](../cmake/RfsCommon.cmake)）直接编译飞控的 `core/`，不带 RTOS 和 bsp。

```cpp
rfs::sim::QuadrotorModel model;
rfs::sim::ImuSensorModel sensors;     // 默认是理想 IMU，可以设置噪声和零偏（固定随机种子）
rfs::fc::FlightControl control;
rfs::sim::InProcessLink link(control);
rfs::sim::Simulation sim(model, sensors, link);

sim.set_rc(channels);                 // 摇杆输入
while (sim.step()) { /* sim.state()、control.debug() */ }
```

闭环回归测试在 [tests/test_closed_loop.cpp](tests/test_closed_loop.cpp)：起飞、悬停、各轴角速度阶跃的方向和收敛、IMU 丢失、相同随机种子的可复现性。混控器、遥控映射或电机布局里任何一处符号写错，这些测试都会失败。

### 画图：`rfs_sim`

`rfs_sim` 按脚本飞一段（地面解锁 → 爬升 → 悬停油门 → 横滚、俯仰、偏航各一个正反脉冲），把轨迹以 CSV 格式输出到 stdout：

```bash
build/host/simulator/rfs_sim > trace.csv                         # 6 s，每 10 ms 一行
build/host/simulator/rfs_sim --duration 3 --decimate 1 > trace.csv
build/host/simulator/rfs_sim --udp 127.0.0.1:14560 > trace.csv   # 驱动 SITL 固件，见 scripts/sitl.sh
```

列：时间、NED 位置和速度、欧拉角、机体角速度、角速度设定值（由摇杆按飞控的默认参数算出，和 `FlightControl::debug()` 一致）、4 路电机指令。

## 三种调试方式

| 方式 | `FcLink` 实现 | 飞控侧 | 用途 |
|---|---|---|---|
| 算法闭环 | `InProcessLink`（已实现）：在进程内直接调用 `rfs::fc::FlightControl::update()` | 只有 `flight-controller/core`，不带 RTOS | 调算法、调参数、在 CI 里做回归测试。最快，可以单步调试 |
| SITL | `UdpLink`（已实现，`rfs_sim --udp`） | 完整固件 + FreeRTOS POSIX port + `bsp/sitl`（`SimBoard` + UDP 版的 `SimBackend`），`scripts/sitl.sh` | 验证任务调度、通信协议，和感知、定位模块联调 |
| HITL | `SerialLink` | 真实飞控板 + `bsp/sim`（串口版的 `SimBackend`） | 上真机之前的最后一道验证 |

SITL 和 HITL 走的是同一组协议消息（`rfs::common` 里的 `MsgId::kSimSensors` 和 `MsgId::kSimActuators`），所以仿真器这边的代码是同一套。

## 构建

```bash
scripts/build.sh simulator -t
```

构建目录是 `build/host/simulator`。
