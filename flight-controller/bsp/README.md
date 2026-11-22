# 板级支持包（BSP）

每块飞控板对应 `bsp/` 下的一个子目录，构建时用 `-DFC_BOARD=<目录名>` 选择。

```
bsp/
├── include/fc_bsp.h     # 应用层使用的硬件接口（所有板子都要实现）
├── _template/           # 新板子的模板
└── <board>/
    ├── board.cmake      # CPU 参数、FreeRTOS port、fc_bsp 库目标
    ├── linker.ld        # 链接脚本
    ├── startup.s        # 向量表、Reset_Handler
    ├── system.c         # 时钟配置
    └── bsp.c            # fc_bsp.h 的实现（IMU、PWM/DShot、UART …）
```

## 新增一块板子

1. `cp -r bsp/_template bsp/<board>`
2. 放入芯片厂商提供的启动文件、链接脚本和 HAL/LL 驱动（例如 STM32CubeMX 生成的代码）
3. 在 `bsp.c` 中实现 [include/fc_bsp.h](include/fc_bsp.h)
4. 按芯片修改 `board.cmake` 中的 `-mcpu` 等参数和 `FC_FREERTOS_PORT`

常用的 FreeRTOS port：

| 内核 | `FC_FREERTOS_PORT` |
|------|--------------------|
| Cortex-M4F | `GCC_ARM_CM4F` |
| Cortex-M7 | `GCC_ARM_CM7` |
| Cortex-M33 | `GCC_ARM_CM33_NTZ_NONSECURE` |
