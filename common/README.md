# common — 跨模块共享库

所有模块共用的代码，**纯 C11、无操作系统依赖、无动态内存分配**，因此既能编进 FreeRTOS 固件，也能给 Linux 应用使用。

## 内容

| 路径 | 说明 |
|------|------|
| `protocol/include/uav/protocol/messages.h` | 消息 ID、节点 ID、各消息的 payload 结构体 |
| `protocol/include/uav/protocol/frame.h` | 串口二进制帧格式：编码、逐字节解析、CRC16 |

## 在模块中使用

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/UavCommon.cmake)
uav_use_common()
target_link_libraries(my_target PRIVATE uav::common)
```

## 单独构建与测试

```bash
cmake -S common -B build/common
cmake --build build/common
ctest --test-dir build/common --output-on-failure
```

## 修改协议的规则

1. 任何 payload 布局变化都要递增 `UAV_PROTOCOL_VERSION`
2. 新字段只能追加在结构体末尾
3. 协议改动和所有受影响模块的适配放在同一个提交 / PR 里
