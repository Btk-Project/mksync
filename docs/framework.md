# mksync 当前框架提取

本文档记录当前代码框架，作为后续开发计划文档的基础。

## 项目定位

mksync 目标是实现类似 input-leap 的鼠标、键盘跨机器同步。当前代码已经具备
Client/Server、RPC 消息、平台抽象、输入事件模型、屏幕拓扑、配置持久化，
以及 Mock 联调测试链路。

## 构建与基础约定

- 构建系统：Xmake。
- 主要语言：C++26，当前工程选项默认 `stdcxx=23`，可配置到 26。
- 异步运行时：ilias，使用 `Task<T>` / `IoTask<T>` 表示任务。
- 错误处理：`Result<T, E>` 与 `IoResult<T>`，通过 `Err(...)` 返回错误。
- `IoTask<T>` 的实际结果是 `IoResult<T>`，也就是 `std::expected<T, std::error_code>`。
  因此 `IoTask<void>` 成功返回时也不是裸 `void`，需要 `co_return {};` 构造成功值。
  `Task<void>` 才是真正的 `void` 任务，成功返回使用 `co_return;`。
- 如果直接向上传播错误: 使用 `ILIAS_CO_TRY` 和 `ILIAS_CO_TRYV`：
  ```cpp
    auto fn(Buffer buffer) -> IoTask<size_t> {
      ILIAS_CO_TRY(auto bytes, co_await stream.write(buffer));
      ILIAS_CO_TRYV(co_await stream.flush());
      co_return bytes;
    }

    auto ok() -> IoTask<void> {
      co_return {};
    }

    auto background() -> Task<void> {
      co_return;
    }
  ```
- 反射与序列化：依赖 neko-proto-tools，并在 `src/refl/` 中提供项目侧封装。
- 日志：spdlog。
- 风格基准：C++ Core Guidelines + `mMember`、table-driven 扩展点、避免 god class / 巨型文件）。

## 模块划分

### app

位置：`src/app/`

- `Client` 连接 Server，发送携带 `machineId` 的 `HelloMessage`，上报本机屏幕，
  初始化 `InputInjector`，收到 `InputMessage` 后注入本机。
- `Server` 监听连接、维护客户端状态和虚拟屏幕列表、运行拓扑切换、转发输入。
- Server 已完成：连接接入、握手与可信 Client 判断、读写任务拆分、本机/远端屏幕注册、
  配置布局优先、边缘切换、远端虚拟光标连续移动、F12 fail-safe 回本机。
- **结构债务**：`Server` 同时承担连接生命周期、拓扑/布局、输入路由、配置持久化；
  见 `development_plan.md` M8。

### rpc

位置：`src/rpc/`

- `RpcMessage` 是消息总线类型，当前由 `HelloMessage`、`ScreensMessage`、
  `InputMessage`、`PingMessage`、`PongMessage`、`ErrorMessage` 组成。
- `HelloMessage.machineId` 是稳定机器标识，用作屏幕 owner id 和可信 Client 判断。
- `RpcTransport` 定义线格式：
  - `u16 size`
  - `u16 type`
  - `u8[...] payload`
- 写入时通过 `std::visit` 序列化具体消息并写入 header。
- 读取时根据 `MessageId` 选择具体消息类型反序列化。

### core

位置：`src/core/`

- 定义跨平台输入事件、鼠标按键、键盘按键、修饰键和屏幕信息。
- `InputEvent` 是输入事件总线类型，当前包含：
  - `KeyEvent`
  - `MouseButtonEvent`
  - `MouseMoveEvent`（含绝对 `x/y` 与可选相对 `deltaX/deltaY`）
  - `MouseWheelEvent`
- `ScreenTopology`：正方形网格邻接 + 真实 rect 边缘检测与入口点映射。

### config

位置：`src/config/`

- `AppConfig`：`machineId`、屏幕网格布局、可信 Client 白名单。
- JSON 读写：`loadOrCreateConfig` / `saveConfig`。
- CLI：`arg_config.hpp`（server / client / `--check-platform`）。

### platform

位置：`src/platform/`

- `Platform` 抽象：枚举屏幕、创建 `InputCapture` / `InputInjector`。
- `InputCapture`：初始化、关闭、异步 `nextEvent`、远端控制模式、本机光标移动。
- `InputInjector`：初始化、关闭、注入 `InputEvent`。
- **Windows**：`win32.cpp`（UI 线程 + LL hook + 远端锚点回拉 + SendInput 注入）。
  文件体量已接近拆分阈值（约 1k 行），见 M8。
- **Linux/X11**：`xcb.cpp`（XInput2 capture 经 ilias poll；XTest 注入）。
  细节与状态见 `docs/xcb_backend.md`；**本轮 review 不改 xcb**。

### refl

位置：`src/refl/`

- `formatter.hpp` 提供基于反射的 enum、flags、struct、variant 格式化。
- `serde.hpp` 封装序列化与反序列化。
- `this_error.hpp` 提供错误类型与 `std::error_code` 桥接。

### tests

位置：`tests/`

- `test_topology` / `test_server` / `test_client` / `test_input_pipeline` /
  `test_mock_platform` / `test_rpc_transport` / `test_config` / `test_refl`。
- `tests/support/mock_platform.hpp`：Mock capture / injector / platform。
- 构建：`xmake test`（`tests/xmake.lua` 扫描 `test_*.cpp`）。

## 当前运行链路

1. Server 绑定 TCP endpoint。
2. Server 创建平台实例与输入捕获器。
3. Server 注册本机屏幕进 `ScreenTopology` / `VirtualScreen`。
4. Client 连接 Server。
5. Client 发送带 `machineId` 的 `HelloMessage` 完成握手（Server 可按白名单拒绝）。
6. Client 发送 `ScreensMessage` 上报屏幕列表。
7. Server 为每个连接启动读写任务；输入经拓扑切换后以 `InputMessage` 转发。
8. Client 将 `InputMessage` 注入本机。
9. 配置中的屏幕布局在注册成功后回写；重启后按 `machineId` 恢复网格位置。

## 已验证与未验证边界

已验证（Mock / 单机平台检查）：

- 拓扑、边缘入口、多远端切换、回本机、键盘转发停止。
- RPC roundtrip、配置读写、`machineId` owner。
- Linux/X11：`--check-platform` 与 XInput2 事件解码（见 `xcb_backend.md`）。

尚未完成真机联调验收：

- 真实 Server + Client 跨机键鼠生效。
- Win32 / Linux 实际注入手感与边界粘滞场景。
- 非开发模式认证策略（当前空白名单 = 接受全部）。

## 文档入口

- 功能与里程碑：`docs/development_plan.md`
- Linux/X11 后端：`docs/xcb_backend.md`
- 代理侧摘要：`.agent/development_plan.md`
