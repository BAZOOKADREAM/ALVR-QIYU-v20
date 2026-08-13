# ALVR v20.14.1 for QIYU 3 / QIYU Dream

本目录是把已验证的 [ALVR-QIYU v19.1.1](https://github.com/hallychou/ALVR-QIYU)（官方
ALVR v19.1.1 + 9 个 Qiyu 适配提交）迁移到官方 **ALVR v20.14.1** 版本线的适配工程。

## 1. 版本基线

- 上游基线：`alvr-org/ALVR` tag `v20.14.1`（v20 分支 `a9f6542`）
- 参照实现：`hallychou/ALVR-QIYU` 的 `QIYU` 分支（基于官方 v19 `967f456`，即 v19.1.1）
- 设备 SDK：`hallychou/QiyuNativeSDK`（AAR 已内置在本仓库 `QiyuNativeSDK/` 目录）

## 2. 为什么这样迁移

ALVR v20 官方 Android 客户端全面转向 OpenXR（`alvr/client_openxr` + `cargo apk`），但
QIYU 3 / QIYU Dream 设备没有 OpenXR runtime，只有 VrApi 兼容的 Qiyu Native SDK。因此本工程
沿用 v19 的路径：

- 继续使用 `alvr/client_core` 的 **C API**（`alvr_client_core.h`）驱动连接、解码、渲染；
- 保留 v19 QIYU 的 `android/` C++/Java 工程（Qiyu SDK 渲染、跟踪、手柄输入、振动手感、
  手柄轨迹预测），仅把与 client_core 交互的部分改写为 v20 API；
- 流式端（PC Streamer）与官方 v20.14.1 完全协议兼容，直接使用官方发布包即可。

## 3. v19.1.1 → v20.14.1 主要 API 变化

| v19.1.1 (ALVR-QIYU) | v20.14.1 (本工程) | 说明 |
| --- | --- | --- |
| `alvr_path_string_to_hash` | `alvr_path_string_to_id` | 输入路径字符串 ID |
| `alvr_initialize(vm, ctx, w, h, rates, n, ext_decoder)` | `alvr_initialize_logging()` + `alvr_initialize_android_context(vm, ctx)` + `alvr_initialize(AlvrClientCapabilities)` | 初始化拆分，能力声明改为结构体 |
| `AlvrEvent::CreateDecoder { codec }` | `AlvrEvent::DecoderConfig { codec }` | 事件改名 |
| `AlvrEvent::NalReady` | 无（解码器全部内置） | v20 解码器完全由 client_core 管理 |
| 事件 `StreamingStarted` 字段 `fps`、`oculus_foveation_level`、`extra_latency`、`controller_prediction_multiplier` | `refresh_rate_hint`、`enable_foveated_encoding`、`enable_hdr`、`encoding_gamma` | 设置类字段移入会话设置 JSON |
| `alvr_send_views_config(fov, ipd)` | `alvr_send_view_params(AlvrViewParams[2])` | FOV+IPD 改为每眼位姿+FOV |
| `alvr_send_tracking(target_ns, motions, n, left_hand, right_hand)` | `alvr_send_tracking(poll_ns, motions, n, hand_skeletons, eye_gazes)` | QIYU 无手部骨架/眼动，传 `nullptr` |
| `alvr_get_frame(out_buf) -> i64` | `alvr_get_frame(out_ts_ns, out_buf) -> bool` | 时间戳与帧指针分开返回 |
| `alvr_report_compositor_start(ts)` | `alvr_report_compositor_start(ts, out_view_params[2])` | 返回服务端协商后的视图参数（含 FOV） |
| `alvr_start_stream_opengl(tex, len)` | `alvr_start_stream_opengl(AlvrStreamConfig)` | 分辨率/foveation/upscale 由配置结构传入 |
| `alvr_render_stream_opengl(buf, indices)` | `alvr_render_stream_opengl(buf, AlvrStreamViewParams[2])` | 每眼带 swapchain index、重投影旋转、FOV |
| `alvr_render_lobby_opengl(eye_inputs, indices)` | `alvr_render_lobby_opengl(AlvrLobbyViewParams[2], background)` | 每眼结构含位姿+FOV |
| `AlvrDeviceMotion { orientation, position }` | `AlvrDeviceMotion { pose: AlvrPose }` | 位姿嵌套一层 |
| 无 | `alvr_create_decoder_auto(codec)` | **本工程新增**：按服务端协商的设置自动建解码器 |
| `alvr_get_prediction_offset_ns`（v19 已有） | `alvr_get_prediction_offset_ns`（**本工程补回**） | v20 官方 C API 未导出，已重新导出 |
| 无 | `alvr_hud_message` / `alvr_update_hud_message_opengl` | 大厅 HUD 提示信息 |

## 4. 代码改动清单

### Rust（仅 `alvr/client_core/src/c_api.rs`）

- 新增导出 `alvr_get_prediction_offset_ns()`；
- 新增导出 `alvr_create_decoder_auto(AlvrCodec)`：从 `alvr_get_settings_json` 缓存的会话设置
  中取出 `force_software_decoder`、`max_buffering_frames`、`buffering_history_weight`、
  `mediacodec_extra_options`（默认含 QTI 低延迟解码选项），与 DecoderConfig 事件里的 codec、
  CSD 数据一起构造解码器，C++ 侧无需解析 JSON；
- `alvr_create_decoder` 的公共逻辑抽为 `create_decoder_from_config()`。

### C++（`android/app/src/main/cpp/cpp_main.cpp`）

- 全部按上表改写 client_core 调用；
- `StreamingStarted_Body` 不再存在，改为 `NativeContext` 中的 `streamViewWidth/Height`、
  `refreshRate`、`enableFoveatedEncoding`、`enableHdr` 字段；
- FOV/IPD 上报改为 `alvr_send_view_params`（眼位姿按 v19 的坐标翻转约定生成：位置取负、
  四元数 w 取负）；
- 解码器生命周期：`DecoderConfig` 事件 → `alvr_create_decoder_auto`；`StreamingStopped` →
  `alvr_destroy_decoder`；
- 流渲染使用 `alvr_report_compositor_start` 返回的 FOV；重投影旋转为恒等（无客户端重投影）；
- 保留 v19 已验证的 QIYU 逻辑：Jerk 估计 + 手柄轨迹预测、坐标翻转、振动手感合并、
  `qiyu_SetFoveation` 硬件注视点、三缓冲、`g_fTrackingOffset` 等；
- 渲染目标级 QCOM foveation 保持关闭（沿用 v19 最后一个提交 "Disable foveated rendering"）。

### Android 工程

- 从 ALVR-QIYU 完整迁移 `android/`（AGP 7.3.0 / Gradle 7.4 / NDK 25.1.8937393 / SDK 33）；
- `versionName` 改为 `20.14.1-qiyu`，`versionCode` 141；
- 增加 aliyun maven 镜像，便于国内网络构建；
- Qiyu SDK 的 6 个 AAR 以内置目录 `QiyuNativeSDK/QiyuNativeSDK/Lib` 提供（等价于原工程的
  git submodule）。

## 5. 构建指南（Windows）

### 5.1 工具链

- JDK 17（本项目验证：`jdk-17.0.4.1`）
- Rust stable（`x86_64-pc-windows-gnu`）+ target `aarch64-linux-android`
- Android SDK：platform-tools、platforms;android-33、build-tools;33.0.0、cmake;3.22.1、
  ndk;25.1.8937393
- `cbindgen`（用于生成 `alvr_client_core.h`）

本机环境若需经代理访问外网，请先保证 git/cargo/gradle 均能联网。

### 5.2 编译 client_core

```powershell
$env:ANDROID_NDK_HOME = "Z:\ALVR_Qiyu3_v20.x\android-sdk\ndk\25.1.8937393"
$BIN = "$env:ANDROID_NDK_HOME\toolchains\llvm\prebuilt\windows-x86_64\bin"
$env:CARGO_TARGET_AARCH64_LINUX_ANDROID_LINKER = "$BIN\aarch64-linux-android26-clang.cmd"
$env:CARGO_TARGET_AARCH64_LINUX_ANDROID_AR = "$BIN\llvm-ar.exe"
$env:CC_aarch64_linux_android = "$BIN\aarch64-linux-android26-clang.cmd"
$env:CXX_aarch64_linux_android = "$BIN\aarch64-linux-android26-clang++.cmd"
$env:AR_aarch64_linux_android = "$BIN\llvm-ar.exe"

cd alvr\alvr\client_core
cargo build --release --no-default-features --target aarch64-linux-android
```

将 `libalvr_client_core.so`（可再拷贝 `.a`）复制到 `alvr/build/alvr_client_core/arm64-v8a/`，
并在 client_core 目录下执行：

```powershell
cbindgen --output ..\..\..\build\alvr_client_core\alvr_client_core.h
```

> 需要先 `git submodule update --init openvr`（或手工检出 openvr 到 `openvr/`），
> `alvr_session` 的 build.rs 依赖 OpenVR 头文件。

### 5.3 编译 APK

```powershell
$env:JAVA_HOME = "C:\Program Files\Java\jdk-17.0.4.1+1"
$env:ANDROID_HOME = "Z:\ALVR_Qiyu3_v20.x\android-sdk"
$env:ANDROID_NDK_HOME = "Z:\ALVR_Qiyu3_v20.x\android-sdk\ndk\25.1.8937393"

cd alvr\android
.\gradlew.bat assembleRelease
```

产物：

- `alvr/android/app/build/outputs/apk/Stable/release/app-Stable-release.apk`
- `alvr/android/app/build/outputs/apk/Nightly/release/app-Nightly-release.apk`

仓库根目录的 `dist/` 已放有本机构建结果：
`dist/alvr_client_qiyu_v20.14.1.apk`（debug keystore 签名，仅供测试）。

## 6. 使用

1. PC 端安装官方 **ALVR v20.14.1** Streamer（协议与本客户端一致，无需修改）：
   https://github.com/alvr-org/ALVR/releases/tag/v20.14.0 或 v20.14.1
2. 用 `adb install` 或 SideQuest 把客户端 APK 装到 QIYU 3 / QIYU Dream（含 Legion VR700）；
3. 首次运行允许麦克风权限，PC 与头显同一网络下在 Streamer 中信任设备即可。

## 7. 已知限制与说明

- 未连接真机做端到端验证：本工程完成了 API 迁移、完整编译与打包、符号/链接核查；
  实际串流建议先在局域网小规模验证（帧率、延迟、手柄预测等）。
- 手柄轨迹预测系数：v19 由服务端下发 `controller_prediction_multiplier`，v20 已移除该事件
  字段，当前 C++ 侧固定为 1.0。如需精细调优，可在 `eventsThread()` 中
  `controllerDisplayTimeS` 处调整。
- QIYU 无手部骨架/眼动数据，`alvr_send_tracking` 传空；服务端手追踪相关功能不可用。
- `eventsThread` 与渲染线程共享 `CTX.streaming` 标志，沿用 v19 的实现（未加锁）。
  在官方 v19.1.1 QIYU 版本上已长期运行稳定，如需严格化可改为 `std::atomic<bool>`。
- 硬件注视点渲染（`qiyu_SetFoveation`）当前在服务端开启 foveated encoding 时使用 `FL_High`，
  否则 `FL_None`。

## 8. 目录结构

```text
alvr/                         # 官方 v20.14.1 + 上述改动（qiyu-v20 分支）
├── alvr/client_core/         # C API 扩展
├── android/                  # QIYU Android 客户端（C++/Java）
├── QiyuNativeSDK/            # Qiyu Native SDK AAR（内置）
└── openvr/                   # OpenVR 头文件子模块（仅构建期使用）
dist/                         # 构建好的客户端 APK
```
