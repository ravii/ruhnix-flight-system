# 板级支持包（BSP）

BSP 是飞控软件和硬件之间的抽象层。应用层只通过 [`fc::Board`](include/fc/bsp/board.hpp) 访问硬件，每块板子提供一份实现，构建时用 `-DFC_BOARD=<目录名>` 选择。

```
bsp/
├── include/fc/bsp/board.hpp   fc::Board 接口（所有板子都要实现）
├── sim/                       仿真板：数据和时间都来自模拟器
│   ├── sim_backend.hpp        和模拟器之间的传输接口（UDP 用于 SITL，串口用于 HITL）
│   └── sim_board.{hpp,cpp}    SimBoard : fc::Board
├── _template/                 新板子模板
└── <board>/
    ├── board.cmake            CPU 参数、FreeRTOS port、fc_bsp 库目标
    ├── linker.ld              链接脚本
    ├── board.cpp              fc::Board 的实现 + fc::board()
    └── vendor/                芯片厂商代码（CubeMX/HAL/CMSIS、startup.s、system.c），不做 clang-format
```

## 接口约定

- `wait_imu()` 决定控制环的节奏：真机上等待 IMU 的数据就绪中断（任务通知），仿真里等待下一个传感器包
- `time_us()` 返回单调递增的微秒时间；仿真里是仿真时间
- 可选传感器（气压计、磁力计、GNSS）只在有新数据时返回 `true`
- 接口的析构函数是 protected 的，板子对象都是静态对象，永远不会被 delete

## 新增一块板子

1. `cp -r bsp/_template bsp/<board>`
2. 把芯片厂商提供的启动文件、链接脚本、HAL 驱动放进 `vendor/`（启动文件里必须调用 `__libc_init_array`，这样全局对象的构造函数才会执行）
3. 在 `board.cpp` 里实现各个 TODO
4. 按芯片修改 `board.cmake` 里的 `-mcpu` 等参数和 `FC_FREERTOS_PORT`

| 内核 | `FC_FREERTOS_PORT` |
|------|--------------------|
| Cortex-M4F | `GCC_ARM_CM4F` |
| Cortex-M7 | `GCC_ARM_CM7` |
| Cortex-M33 | `GCC_ARM_CM33_NTZ_NONSECURE` |
| 主机 SITL（Linux） | `GCC_POSIX` |
