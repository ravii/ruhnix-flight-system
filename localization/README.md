# localization — 定位

运行在机载 Linux 计算机上的定位服务。它融合飞控转发的 IMU 数据和外部位置观测（GNSS、视觉里程计、动捕），输出 `MsgId::kPoseEstimate`。

## 当前状态

- `PoseEstimator`：世界系（NED）下按轴解耦的 [位置, 速度] 卡尔曼滤波
- 待做：姿态估计、IMU 偏置估计、升级为完整的 EKF/ESKF

## 构建

```bash
# 主机
scripts/build.sh localization -t
./build/host/localization/localization

# 交叉编译到 ARM64（测试在 qemu 下运行）
scripts/build.sh --arm64 localization -t
```

构建目录是 `build/host/localization`、`build/arm64/localization`。

## 依赖

目前只用到 C++17 标准库和 `common/`。如果以后要引入 Eigen 等第三方库，用 `find_package` 从目标 sysroot 中查找，不要把库文件提交进仓库。
