# perception — 感知

运行在机载 Linux 计算机上的感知服务：采集相机/深度数据，检测障碍物，并把结果提供给定位模块和飞控。

## 当前状态

- `ObstacleDetector`：在深度图中心区域内找最近的有效回波，作为基线实现
- 待做：相机驱动接入（V4L2 / 厂商 SDK）、点云处理、目标检测模型推理

## 构建

```bash
# 主机
scripts/build.sh perception -t

# 交叉编译到 ARM64（测试在 qemu 下运行）
scripts/build.sh --arm64 perception -t
```

构建目录是 `build/host/perception`、`build/arm64/perception`。

## 依赖

目前只用到 C++17 标准库和 `common/`。以后接入 OpenCV、ONNX Runtime 等库时，用 `find_package` 从目标 sysroot 中查找。
