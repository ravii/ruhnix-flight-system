# third_party

第三方依赖，统一用 git submodule 引入，不直接提交源码。

| 依赖 | 路径 | 使用方 |
|------|------|--------|
| FreeRTOS-Kernel | `FreeRTOS-Kernel/` | flight-controller、esc |

```bash
git submodule add https://github.com/FreeRTOS/FreeRTOS-Kernel.git third_party/FreeRTOS-Kernel
```
