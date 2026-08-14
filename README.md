# ALVR for QIYU (v20.14.1)

把已验证的 [ALVR-QIYU v19.1.1](https://github.com/hallychou/ALVR-QIYU) 迁移到官方
[ALVR v20.14.1](https://github.com/alvr-org/ALVR/releases/tag/v20.14.1) 版本线的客户端适配工程，
面向 **QIYU 3 / QIYU Dream 系列（含 Lenovo Legion VR700）**。

## 这是什么

- 基于官方 ALVR v20.14.1 的 `client_core` C API；
- 完整移植 v19.1.1 QIYU 的 C++/Java 客户端（Qiyu Native SDK 渲染、跟踪、手柄输入、
  振动手感、手柄轨迹预测）；
- 流式端（PC Streamer）与官方 v20.14.1 完全协议兼容，直接用官方发布包即可；
- 内置 Qiyu Native SDK AAR（来源：[QiyuNativeSDK](https://github.com/hallychou/QiyuNativeSDK)）。

## 快速开始

1. PC 端安装官方 **ALVR v20.14.1 Streamer**；
2. 在本仓库的 [Releases](../../releases) 下载客户端 APK 安装到头显：
   - `...-nightly.apk`：包名 `alvr.client.quest.nightly`，可与旧版共存，推荐先用它测试；
   - `....apk`（Stable）：包名 `alvr.client.quest`，如已装旧版 ALVR-QIYU 需先卸载；
3. 在 Streamer 的 Video 设置中把转码分辨率设为 **2048（或启用 Scale 100%）**——
   QIYU 硬件解码器不支持默认的 4288px 宽码流，否则会解码失败循环重连；
4. 首次运行允许麦克风权限，同一网络下在 Streamer 中信任设备即可。

## 从源码构建 / 迁移说明

详见 [QIYU_V20.md](QIYU_V20.md)（工具链、构建命令、v19→v20 API 映射、已知限制）。

## 已知限制

- 无手部骨架/眼动数据，服务端手部追踪功能不可用；
- 有线串流需要在 Streamer 的 Connection 设置里把 "Wired client type" 设为 Custom，
  包名填 `alvr.client.quest`（Stable）或 `alvr.client.quest.nightly`（Nightly）。

## 致谢

- 上游：[alvr-org/ALVR](https://github.com/alvr-org/ALVR)（MIT License）
- v19 适配参照：[hallychou/ALVR-QIYU](https://github.com/hallychou/ALVR-QIYU)
- 设备 SDK：[hallychou/QiyuNativeSDK](https://github.com/hallychou/QiyuNativeSDK)
