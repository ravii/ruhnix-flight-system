# common — 跨模块共享库

所有模块共用的代码，C++17，命名空间 `rfs::common`。**无操作系统依赖、无动态内存分配、不用异常**，因此既能编进 FreeRTOS 固件，也能给 Linux 应用和模拟器使用。这里只放数据结构定义和编解码。

## 内容

| 路径 | 说明 |
|------|------|
| [include/rfs/common/messages.hpp](include/rfs/common/messages.hpp) | `MsgId`、`NodeId`、各消息的 payload 结构体（`HeartbeatMsg` …），包括模拟器用的 `SimSensorsMsg` / `SimActuatorsMsg` |
| [include/rfs/common/frame.hpp](include/rfs/common/frame.hpp) | 串口二进制帧格式：`frame_encode()`、逐字节解析器 `FrameParser`、`crc16()` |

所有 payload 结构体都是 packed 的，数组字段用 C 数组而不是 `std::array`。使用时要逐个元素复制，不要对它的成员取引用或取指针（否则会产生非对齐访问，GCC 也会报 `-Waddress-of-packed-member`）。

## 在模块中使用

```cmake
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/RfsCommon.cmake)
rfs_use_common()
target_link_libraries(my_target PRIVATE rfs::common)
```

## 单独构建与测试

```bash
cmake -S common -B build/common
cmake --build build/common
ctest --test-dir build/common --output-on-failure
```

## 修改协议的规则

1. 任何 payload 布局变化都要递增 `kProtocolVersion`
2. 新字段只能追加在结构体末尾
3. 每个 payload 的字节数都由 [messages.hpp](include/rfs/common/messages.hpp) 末尾的 `static_assert` 锁定，并且必须能放进一帧（[frame.cpp](src/frame.cpp) 里检查）。改了布局，编译就会失败，提醒你同时更新期望值和 `kProtocolVersion`
4. 协议改动和所有受影响模块的适配放在同一个提交 / PR 里
