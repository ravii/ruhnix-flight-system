# perception — 感知

运行在机载 Linux 计算机上的感知服务：采集相机/深度数据，检测障碍物，并把结果提供给定位模块和飞控。

## 当前状态

- `ObstacleDetector`：在深度图中心区域内找最近的有效回波，作为基线实现
- 待做：相机驱动接入（V4L2 / 厂商 SDK）、点云处理、目标检测模型推理

## 构建

```bash
# 主机
cmake -S perception -B build/perception-host
cmake --build build/perception-host
ctest --test-dir build/perception-host --output-on-failure

# 交叉编译到 ARM64
cmake -S perception -B build/perception-arm64 \
      -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
      -DCMAKE_SYSROOT=/path/to/sysroot
cmake --build build/perception-arm64
```

## 依赖

目前只用到 C++17 标准库和 `common/`。以后接入 OpenCV、ONNX Runtime 等库时，用 `find_package` 从目标 sysroot 中查找。
