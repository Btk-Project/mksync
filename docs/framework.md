# mksync 当前框架提取

本文档记录当前代码框架，作为后续开发计划文档的基础。

## 项目定位

mksync 目标是实现类似 input-leap 的鼠标、键盘跨机器同步。当前代码已经具备
Client/Server、RPC 消息、平台抽象、输入事件模型和基础反射工具链的雏形。

## 构建与基础约定

- 构建系统：Xmake。
- 主要语言：C++26，当前工程选项默认 `stdcxx=23`，可配置到 26。
- 异步运行时：ilias，使用 `Task<T>` / `IoTask<T>` 表示任务。
- 错误处理：`Result<T, E>` 与 `IoResult<T>`，通过 `Err(...)` 返回错误。
- `IoTask<T>` 的实际结果是 `IoResult<T>`，也就是 `std::expected<T, std::error_code>`。
  因此 `IoTask<void>` 成功返回时也不是裸 `void`，需要 `co_return {};` 构造成功值。
  `Task<void>` 才是真正的 `void` 任务，成功返回使用 `co_return;`。
- 如果直接向上传播错误: 使用 `ILIAS_CO_TRY` 和 `ILIAS_CO_TRYV` 这个宏类似 Rust的?
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

## 模块划分

### app

位置：`src/app/`

- `Client` 负责连接服务器，发送携带 `machineId` 的 `HelloMessage`，随后发送本机屏幕信息。
- `Server` 负责监听连接、维护客户端状态和虚拟屏幕列表，并等待本机平台事件。
- 目前 Server 已经完成连接接入、握手、读写任务拆分、本地/远端屏幕注册和基础边界切换。
- 输入事件通过 `InputMessage` 转发给活动远端 Client，Client 收到后调用 `InputInjector` 注入。

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
  - `MouseMoveEvent`
  - `MouseWheelEvent`

### platform

位置：`src/platform/`

- `Platform` 抽象当前系统能力：
  - 枚举屏幕。
  - 创建输入捕获器。
  - 创建输入注入器。
- `InputCapture` 提供初始化、关闭和异步读取输入事件。
- `InputInjector` 提供初始化、关闭和注入输入事件。
- 当前已有 Windows 与 XCB/Linux 最小输入注入实现；XCB capture 已接入 ilias poll 后端。

### refl

位置：`src/refl/`

- `formatter.hpp` 提供基于反射的 enum、flags、struct、variant 格式化。
- `serde.hpp` 封装序列化与反序列化。
- `this_error.hpp` 提供错误类型与 `std::error_code` 桥接。

## 当前运行链路

1. Server 绑定 TCP endpoint。
2. Server 创建平台实例与输入捕获器。
3. Server 注册本机屏幕为 `VirtualScreen`。
4. Client 连接 Server。
5. Client 发送带 `machineId` 的 `HelloMessage` 完成握手。
6. Client 发送 `ScreensMessage` 上报屏幕列表。
7. Server 为每个连接启动读写任务。
8. 后续输入捕获、屏幕切换、远端注入和状态同步仍需补齐。

## 近期开发计划入口

- 做 Linux 真机编译，验证 XCB capture 和 injector。
- 完成真实 Server/Client 联调验收。
