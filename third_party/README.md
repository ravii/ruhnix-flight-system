# third_party

第三方依赖，统一用 git submodule 引入，不直接提交源码，也不做 clang-format。

| 依赖 | 路径 | 版本 | 使用方 |
|------|------|------|--------|
| FreeRTOS-Kernel | `FreeRTOS-Kernel/` | V11.3.3 | flight-controller（MCU 固件和 SITL） |

```bash
git submodule update --init    # clone 之后执行一次；子模块是浅克隆（.gitmodules 里 shallow = true）
```

FreeRTOS-Kernel 自己还带了一些子模块（社区移植），我们用不到，不需要 `--recursive`。

升级版本：

```bash
git -C third_party/FreeRTOS-Kernel fetch --depth 1 origin tag V11.x.y
git -C third_party/FreeRTOS-Kernel checkout V11.x.y
git add third_party/FreeRTOS-Kernel     # 然后改上面表格里的版本号，一起提交
```
