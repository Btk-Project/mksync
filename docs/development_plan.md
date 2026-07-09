# mksync 开发计划与实现方案

本文档基于 `docs/framework.md` 中已经确定的框架编写。后续实现应保持以下边界：

- 使用 ilias 作为异步运行时，异步接口返回 `Task<T>` / `IoTask<T>`。
- 普通错误使用 `Result<T, E>` / `IoResult<T>`，只在致命错误中使用异常。
- 跨平台能力通过 `Platform`、`InputCapture`、`InputInjector` 抽象。
- RPC 消息继续由 `RpcMessage` 统一承载。
- 新增可回归的行为时补测试，优先建立 Mock Platform。
- Linux/X11 后端独立设计见 `docs/xcb_backend.md`。

## 总体目标

v0.1 的目标是跑通最小可用的跨机器输入链路：

1. Client 连接 Server 并上报屏幕。
2. Server 建立全局屏幕拓扑。
3. Server 根据鼠标位置和拓扑切换当前活动屏幕。
4. 当活动屏幕属于远端 Client 时，Server 将键鼠事件转发给该 Client。
5. Client 将收到的事件注入到本机系统。

## 当前进度

最后更新：2026-07-08。

当前 checklist：

- [x] 建立 `ScreenTopology` 拓扑模块。
- [x] 支持拓扑屏幕注册和删除。
- [x] 支持重复 screen key / cell 冲突检测。
- [x] 支持按正方形网格查询上下左右邻居。
- [x] 支持基于真实 screen rect 判断鼠标是否在边缘。
- [x] 支持不同分辨率屏幕之间的入口点映射。
- [x] Server 启动时注册本机屏幕。
- [x] Server 收到 `ScreensMessage` 后注册远端屏幕。
- [x] Server 支持同 endpoint 重新上报屏幕后替换旧屏幕。
- [x] Server 内部可根据鼠标右边缘切换 active screen。
- [x] 增加测试用 `MockPlatform`。
- [x] 增加测试用 `MockInputCapture`。
- [x] 增加测试用 `MockInputInjector`。
- [x] `tests/xmake.lua` 只自动扫描 `test_*.cpp`。
- [x] `test_xxx.lua` 可接管特殊测试目标。
- [x] `test_topology` 覆盖拓扑核心。
- [x] `test_server` 覆盖屏幕注册、右边缘切换、远端回本机和多个远端屏幕切换。
- [x] `test_mock_platform` 覆盖 Mock Platform。
- [x] `test_rpc_transport` 覆盖现有 RPC transport roundtrip。
- [x] `test_refl` 覆盖反射 formatter 和错误格式化。
- [x] 定义最小 `InputMessage` 协议。
- [x] 给 `RpcTransport` 增加 `InputMessage` roundtrip 测试。
- [x] `InputMessage` 可格式化。
- [x] Server 切入远端屏幕时发送入口 `MouseMoveEvent`。
- [x] Server 在远端 active screen 上转发非 `MouseMoveEvent` 输入。
- [x] Client 初始化 `InputInjector`。
- [x] Client 收到输入消息后调用 `InputInjector::inject`。
- [x] `test_client` 覆盖 `InputMessage` 到 `MockInputInjector` 的注入。
- [x] 用 Mock Platform 跑通 Server -> Client -> MockInjector 测试。
- [x] 用 Mock Platform 覆盖完整操作流程：跨到远端、键盘传递、回本机、键盘停止转发。
- [x] 实现 Win32 `InputInjector`。
- [x] 实现 XCB/Linux `InputInjector`。
- [x] XCB/XInput2 capture 改为使用 ilias poll，不再使用独立线程。
- [x] 修复 `xmake f --stdcxx=26` 时 `stdcxx` 传给 `ilias` 包配置的类型问题。
- [x] 新增 `mksync --check-platform` 平台后端 smoke check 模式。
- [x] 当前 GCC 14.2 / C++23 环境下，Linux/XCB 主目标已完成编译和链接检查。
- [x] Linux/X11 真机 `mksync --check-platform` 初始化检查通过，确认 XInput 2.0 和 XTest 2.2 可用。
- [x] Linux/X11 真机捕获到连续 `MouseMoveEvent`，确认 XInput2 事件解码可用。
- [x] 远端 active screen 上按本机 `MouseMoveEvent` delta 连续发送远端鼠标移动。
- [x] 处理远端屏幕切回本机屏幕。
- [x] 处理跨多个远端屏幕。
- [x] 定义 `AppConfig` 配置模型。
- [x] 配置文件可保存 `machineId` 和屏幕网格位置。
- [x] Server 注册屏幕时可优先使用配置中的网格位置。
- [x] CLI/运行入口加载配置文件并传给 Server。
- [x] 配置可信 Client 名称白名单。
- [x] `HelloMessage` 携带 `machineId`。
- [x] Client 握手发送配置中的本机 `machineId`。
- [x] Server 使用远端 `HelloMessage.machineId` 作为屏幕 owner id，缺失时回退 endpoint。
- [x] 引入稳定 `machineId`，替换 endpoint 字符串作为长期 owner id。
- [x] Server 注册屏幕后回写 `AppConfig.screens`。
- [x] 运行入口传入配置路径时，Server 将当前屏幕布局保存到配置文件。
- [x] 重启加载配置后，屏幕布局按 `machineId` 恢复。

当前约束：

- [x] 初期不修改已有消息和事件字段；配置阶段为 `HelloMessage` 追加必要的 `machineId`。
- [x] 当前阶段不实现复杂鼠标手感、DPI/DPS 或速度统一。
- [x] 归一化比例只作为边缘入口映射的临时计算，不作为运行时坐标系统。
- [x] 自动布局只是临时策略，正式布局应来自配置。
- [x] 远端 active screen 上的连续 `MouseMoveEvent` 已按 `targetDelta = sourceDelta` 基线实现。

## 屏幕拓扑决策

推荐采用“正方形网格拓扑 + 客户端真实 rect 坐标”的模型。

也就是说：拓扑层认为每块屏幕占据一个同尺寸网格单元，单元坐标是整数
`(gridX, gridY)`。这个网格只回答“谁在谁的上下左右”，不把不同屏幕的真实
`rect` 拉伸成同一种尺寸。屏幕内部位置、鼠标 delta、速度感和最终注入全部使用
客户端上报的真实坐标系。

这个模型把两个问题拆开：

- 屏幕之间如何相邻：由正方形网格决定。
- 屏幕内部如何移动和注入：由该屏幕自己的 `rect`、DPI/缩放和平台坐标决定。

### 为什么不直接按像素对齐

如果把不同分辨率屏幕放进同一个像素级虚拟大平面，会立刻遇到这些问题：

- 1920x1080 右侧接 2560x1440 时，两块屏幕的边长不一致，边缘会出现部分可穿越、
  部分不可穿越的“死区”。
- DPI、缩放、物理尺寸在不同系统上并不可靠，按物理尺寸对齐会引入更多平台差异。
- 用户希望的是“这块屏幕在那块屏幕右边”，而不是精确模拟桌面管理器的像素几何。

正方形网格能保持拓扑简单：只要右边有屏幕，就认为逻辑上存在右侧邻居。不同分辨率
不参与邻居判断，但会参与边缘入口坐标、后续移动速度和远端注入坐标。

### 坐标与 rect 模型

Client 必须向 Server 上报自己的真实屏幕 `rect`。当前 `ScreenInfo` 已经包含：

- `x/y`：屏幕在该客户端本机桌面坐标系里的位置。
- `width/height`：屏幕真实像素宽高。
- `dpi`：屏幕 DPI，后续可扩展为 scale factor 或 device pixel ratio。
- `name/primary`：调试和默认布局使用。

捕获层当前继续产生本机屏幕内像素坐标，不修改现有事件字段：

```cpp
struct MouseMoveEvent {
    int32_t x;
    int32_t y;
    uint32_t screenIndex;
};
```

Server 转发到远端 Client 的鼠标事件也使用目标 Client 自己的真实坐标。也就是说：
Server 不发送“拓扑归一化点”作为运行时坐标，而是发送已经按目标屏幕 rect 计算出的
`screenIndex/x/y`。后续如果必须支持远端活动屏幕上的连续相对运动，再单独讨论最小
协议或事件扩展。

归一化比例只作为“穿越边缘的一瞬间”的临时计算工具，用来确定进入邻居屏幕后落在哪条
边上，而不是长期保存的鼠标坐标：

- 从左到右：保留 `v`，目标 `x = 0`。
- 从右到左：保留 `v`，目标 `x = targetWidth - 1`。
- 从上到下：保留 `u`，目标 `y = 0`。
- 从下到上：保留 `u`，目标 `y = targetHeight - 1`。

进入目标屏幕后，后续移动不再按 `[0, 1]` 更新，而是按真实 delta 和目标屏幕的
DPI/缩放策略更新。

例子：源屏幕是 1920x1080，目标屏幕是 2560x1440。鼠标从源屏幕右边缘的
`y = 810` 穿越，`v` 约为 `0.75`，进入目标屏幕后落在 `y = 1080` 附近。
这一步只决定入口位置。进入目标屏幕后，每一帧移动都按目标屏幕的真实坐标继续计算。

### 速度与 DPI/DPS 策略

不能把鼠标运动简单压成 `[0, 1]`，否则不同分辨率屏幕会产生明显的速度差异。例如：
同样移动 100 像素，在 1920 宽屏幕上是 `0.052`，在 3840 宽屏幕上是 `0.026`。
如果持续用归一化坐标更新，4K 屏会显得鼠标速度变慢或变快，取决于映射方向。

初期只考虑基础功能，不做复杂手感或移速统一。当前策略：

- 事件传输优先保留真实像素 delta。
- Server 维护远端活动屏幕上的真实像素位置。
- 从源屏幕切到目标屏幕时，只用边缘比例决定初始落点。
- v0.1 先采用 `targetDelta = sourceDelta`，因为这是最容易测试的基线。
- DPI/DPS、系统指针速度、原始输入等手感统一策略后续再独立设计。
- 后续可以引入 DPI aware 换算，例如：

```text
targetDelta = sourceDelta * targetDpi / sourceDpi
```

是否使用 DPI、DPS、系统指针速度或原始输入，需要做成策略，而不是写死在拓扑层。

### 屏幕单元模型

建议在 `src/core/` 下新增拓扑模块，例如 `topology.hpp` / `topology.cpp`。

核心数据结构可以先按这个方向设计：

```cpp
struct GridPosition {
    int32_t x = 0;
    int32_t y = 0;
};

struct ScreenKey {
    // 正式路径使用稳定 machineId；machineId 缺失时才回退 endpoint 字符串。
    std::string ownerId;
    uint32_t screenIndex = 0;
};

struct TopologyScreen {
    ScreenKey key;
    GridPosition cell;
    ScreenInfo info;
    bool local = false;
};

struct ScreenPoint {
    ScreenKey key;
    int32_t x = 0;
    int32_t y = 0;
};
```

拓扑对象负责：

- 注册、删除屏幕。
- 检查网格单元是否冲突。
- 根据 `ScreenKey` 查找屏幕。
- 根据当前屏幕和方向查找邻居。
- 根据真实 rect 判断边缘穿越。
- 在穿越边缘时计算目标屏幕入口点。

初期 API 可以类似：

```cpp
class ScreenTopology {
public:
    auto addScreen(TopologyScreen screen) -> IoResult<void>;
    auto removeOwner(std::string_view ownerId) -> void;
    auto findNeighbor(ScreenKey key, Edge edge) const -> std::optional<ScreenKey>;
    auto hitEdge(ScreenPoint point) const -> std::optional<Edge>;
    auto mapEntryPoint(ScreenPoint from, Edge edge) const -> IoResult<ScreenPoint>;
};
```

## 远端活动屏幕的鼠标运动

当前平台层捕获的是本机屏幕内绝对坐标。只靠绝对坐标，可以完成“本机屏幕到远端屏幕
的首次切换”。远端屏幕已经活动后的连续运动，先不修改现有消息和事件结构，等最小链路
跑通后再选择最简单的补充方案。

处理策略：

- 活动屏幕是本机屏幕时，使用 `x/y` 判断是否触达边缘。
- 切换到远端屏幕后，Server 维护目标屏幕上的当前像素位置。
- 当前阶段优先实现入口点切换和绝对位置注入。
- 后续若需要远端连续相对运动，再考虑最小事件或协议扩展。
- delta 是否按 DPI/DPS 缩放由独立策略决定，不放进拓扑邻接模型。
- 如果目标像素位置越过远端屏幕边缘，再通过拓扑查找下一个邻居。

这样拓扑逻辑不依赖真实系统桌面的大坐标系，也不要求本机鼠标真的移动到远端机器。

## RPC 协议扩展

当前协议已有：

- `HelloMessage`
- `ScreensMessage`
- `InputMessage`
- `PingMessage`
- `PongMessage`
- `ErrorMessage`

当前已完成两处必要扩展：

- `HelloMessage` 追加 `machineId`，用于稳定屏幕 owner id 和可信 Client 判断。
- `InputMessage` 用于 Server 向活动 Client 转发输入事件。

后续必要时可以新增：

```cpp
struct CursorEnterMessage {
    static constexpr auto Id = MessageId::CursorEnter;
    uint32_t screenIndex = 0;
    int32_t x = 0;
    int32_t y = 0;
};
```

第一版已经只实现 `InputMessage`，将鼠标移动、按键、滚轮统一发给活动 Client。
`CursorEnterMessage` 可以稍后用于显式通知远端显示/隐藏光标、重置状态或做调试日志。

## Server 侧实现方案

Server 需要新增这些成员：

- `ScreenTopology mTopology`
- `ScreenKey mActiveScreen`
- 当前活动屏幕上的鼠标位置，使用目标屏幕真实像素点缓存。
- `ClientState` 中保存 `HelloMessage` 的 `machineId`、机器名和该 Client 的屏幕 key 列表。

事件处理流程：

1. `handleIncoming` 完成 Hello 握手后保存 `machineId` 和机器名。
2. `handleClientRead` 收到 `ScreensMessage` 后注册远端屏幕。
3. `waitPlatformEvent` 收到本机输入事件。
4. 如果活动屏幕是本机：
   - 鼠标移动使用本机屏幕真实像素坐标判断边缘。
   - 若触达边缘且邻居存在，则按入口映射计算目标屏幕真实像素坐标。
   - 邻居是远端时，将目标 `screenIndex/x/y` 发送给对应 Client。
5. 如果活动屏幕是远端：
   - 鼠标移动使用相对增量和 DPI/DPS 策略更新远端目标坐标。
   - 键盘、按键、滚轮直接发送给远端 Client。
   - 若远端目标坐标越过边缘，则继续按拓扑切换。

本机屏幕之间的切换初期可以交给系统自己的桌面管理器。mksync 重点处理跨机器边界。

## Client 侧实现方案

Client 目前已经能连接 Server 并发送屏幕列表。下一步：

1. 创建并初始化 `InputInjector`。
2. `handleRead` 收到 `InputMessage` 后调用 `inject(event)`。
3. 鼠标移动事件中的 `screenIndex/x/y` 表示目标机器自己的屏幕索引和像素坐标。
4. 键盘事件使用项目内的 HID-like `Key` 和平台层映射注入。

注入层初期只做最小能力：

- 鼠标绝对移动。
- 鼠标按键。
- 滚轮。
- 键盘按下和释放。

## Mock Platform 与测试计划

Mock Platform 应该优先做，因为拓扑、Server 和 Client 的行为都需要不依赖真实系统测试。

建议新增测试能力：

- `MockPlatform`：持有屏幕列表。
- `MockInputCapture`：测试中主动塞入 `InputEvent`。
- `MockInputInjector`：记录被注入的事件。
- 可选 `MemoryRpcTransport`：绕过真实 TCP，测试消息流。

优先测试用例：

- Client 上报 rect 后 Server 保存真实屏幕尺寸。
- 1920x1080 到 2560x1440 的右边缘入口映射。
- 2560x1440 到 1920x1080 的左边缘入口映射。
- 上下边缘入口映射只保留横向相对入口位置。
- 当前阶段不通过归一化坐标累积鼠标位置。
- 无邻居时不切换活动屏幕。
- 多屏幕同一网格单元注册失败。
- Client 上报多屏幕后 Server 拥有正确拓扑。
- 活动屏幕为远端时，键盘事件进入远端注入器。

## 分阶段里程碑

### M1：拓扑核心

Todo：

- [x] 新增 `ScreenTopology`。
- [x] 支持注册屏幕。
- [x] 支持删除某个 owner 的屏幕。
- [x] 支持重复 `ScreenKey` 冲突检测。
- [x] 支持重复 `GridPosition` 冲突检测。
- [x] 支持上下左右邻居查询。
- [x] 支持基于真实 rect 的边缘检测。
- [x] 支持不同分辨率屏幕之间的入口点映射。
- [x] 补充拓扑单元测试。

验收：

- [x] 1920x1080 到 2560x1440 的右边缘入口映射测试通过。
- [x] 2560x1440 到 1920x1080 的左边缘入口映射测试通过。
- [x] 上下边缘入口映射测试通过。
- [x] 无邻居时不切换活动屏幕。
- [x] 连续移动不依赖长期保存的归一化坐标。

记录：

- [x] 已新增 `src/core/topology.hpp`。
- [x] 已新增 `src/core/topology.cpp`。
- [x] 已新增 `test_topology`。
- [x] `xmake test` 已覆盖该模块。

### M2：协议扩展

Todo：

- [x] 初期暂不修改已有消息数据；配置阶段仅为 `HelloMessage` 补充 `machineId`。
- [x] 定义最小 `MessageId::Input`。
- [x] 定义最小 `InputMessage`。
- [x] 将 `InputMessage` 加入 `RpcMessage` variant。
- [x] 让 `RpcTransport` 测试覆盖 `InputMessage` roundtrip。
- [x] 确认 `InputMessage` 可格式化，方便日志调试。
- [x] 为 `HelloMessage` 追加 `machineId`。
- [x] 让 `RpcTransport` 测试覆盖 `HelloMessage.machineId` roundtrip。

验收：

- [x] `InputMessage` 可序列化。
- [x] `InputMessage` 可反序列化。
- [x] `InputMessage` 可通过 `RpcTransport` 往返。
- [x] 不修改 `InputEvent`、`MouseMoveEvent`、`KeyEvent` 现有字段。
- [x] `HelloMessage.machineId` 可通过 `RpcTransport` 往返。

记录：

- [x] 已决定输入事件结构保持不变。
- [x] 已添加最小 `InputMessage`。
- [x] 已添加 `HelloMessage.machineId`。

### M3：Server 屏幕注册

Todo：

- [x] Server 保存 Client 的 endpoint。
- [x] Server 保存 Client 的 Hello name。
- [x] Server 处理 `ScreensMessage`。
- [x] Server 将本机屏幕注册进拓扑。
- [x] Server 将远端屏幕注册进拓扑。
- [x] 同 endpoint 重新上报屏幕后替换旧屏幕。
- [x] 本机 primary 默认放在 `(0, 0)`。
- [x] 远端机器按连接顺序放到右侧空列。
- [x] Server 注册屏幕时优先使用配置中的网格位置。
- [x] CLI/运行入口加载配置文件并传给 Server。
- [x] Client 握手发送配置中的本机 `machineId`。
- [x] Server 保存 Client 的 `HelloMessage.machineId`。
- [x] Server 使用稳定 `machineId` 替换 endpoint 字符串作为长期 owner id。
- [x] `machineId` 缺失时回退 endpoint 字符串。

验收：

- [x] Server 可查询当前拓扑屏幕列表。
- [x] 本机屏幕注册测试通过。
- [x] 远端屏幕注册测试通过。
- [x] 重复上报替换测试通过。
- [x] 配置指定屏幕网格位置测试通过。
- [x] 配置指定的 `machineId` 屏幕网格位置测试通过。
- [ ] 一个真实 Server、一个真实 Client 连接后，Server 拥有完整拓扑。

记录：

- [x] 已新增 `test_server` 覆盖注册逻辑。
- [x] 当前测试使用测试 hook 调用注册逻辑。
- [x] `test_server` 已覆盖配置布局优先于自动布局。
- [x] `test_server` 已覆盖远端屏幕 owner id 使用 `machineId`。
- [ ] 当前测试尚未启动真实 listener。

### M4：Mock Platform

Todo：

- [x] 新增测试用 `MockPlatform`。
- [x] `MockPlatform` 可持有屏幕列表。
- [x] 新增测试用 `MockInputCapture`。
- [x] `MockInputCapture` 可主动推事件。
- [x] 新增测试用 `MockInputInjector`。
- [x] `MockInputInjector` 可记录事件。
- [x] 将 Mock Platform 接入最小 Server/Client 联调测试。

验收：

- [x] Mock 屏幕列表测试通过。
- [x] Mock capture 事件读取测试通过。
- [x] Mock injector 事件记录测试通过。
- [x] Server -> Client -> MockInjector 最小链路测试通过。

记录：

- [x] 已新增 `tests/support/mock_platform.hpp`。
- [x] 已新增 `test_mock_platform`。

### M5：远端注入

Todo：

- [x] Client 初始化 `InputInjector`。
- [x] Client 收到 `InputMessage` 后调用 `InputInjector::inject`。
- [x] 鼠标绝对移动可注入（Win32）。
- [x] 鼠标按键可注入（Win32）。
- [x] 鼠标滚轮可注入（Win32）。
- [x] 键盘按下可注入（Win32）。
- [x] 键盘释放可注入（Win32）。
- [x] 实现 Win32 最小 `InputInjector`。
- [x] 实现 XCB/Linux 最小 `InputInjector`。

验收：

- [x] `test_client` 中 Client 能处理输入消息。
- [x] `test_client` 中 Mock injector 能记录注入事件。
- [x] Mock 联调中远端 Client 能收到输入消息。
- [x] Mock 联调中远端 Client 能记录注入事件。
- [ ] Win32 上至少能移动鼠标。
- [ ] Linux/XCB 上至少能移动鼠标。

记录：

- [x] 已在 `src/platform/win32.cpp` 新增 `Win32InputInjector`。
- [x] Win32 鼠标绝对移动使用 `SetCursorPos`。
- [x] Win32 鼠标按钮、滚轮和键盘事件使用 `SendInput`。
- [x] Win32 注入器已接入 `Win32Platform::createInjector()`。
- [x] 已在 `src/platform/xcb.cpp` 新增 `XcbInputInjector`。
- [x] XCB/Linux 注入器使用 XTest/Xlib 注入鼠标移动、按钮、滚轮和键盘事件。
- [x] XCB/Linux 注入器已接入 `XcbPlatform::createInjector()`。
- [x] Linux 构建依赖增加 `libxtst`。
- [x] 修复 `xmake f --stdcxx=26` 命令行配置会把 `stdcxx` 作为字符串传给 `ilias` 的问题。
- [x] 新增 `mksync --check-platform`，可在真机上枚举屏幕并初始化/关闭 capture 与 injector。
- [x] Linux/X11 真机运行 `mksync --check-platform` 已通过，环境报告单屏 3600x1080、XInput 2.0、XTest 2.2。
- [x] Linux/X11 真机捕获到连续 `MouseMoveEvent`，确认 XInput2 事件解码可用。
- [x] 在当前 GCC 14.2 / C++23 环境下，`xmake build mksync` 已通过。
- [x] 在当前 GCC 14.2 / C++23 环境下，`xmake test` 已通过。
- [ ] 当前环境 GCC 14.2 不支持 `-freflection`，尚未完成 GCC 16.1 / C++26 编译验收。
- [ ] 尚未做会实际移动本机鼠标的手工验收。
- [ ] 尚未做 Linux/XCB 真机编译和注入验收。

### M6：跨屏切换

Todo：

- [x] Server 根据拓扑判断边缘穿越。
- [x] Server 内部 active screen 可从本机切到右侧远端屏幕。
- [x] 无邻居时保持当前 active screen。
- [x] 活动屏幕切到远端后，发送入口 `MouseMoveEvent` 给远端 Client。
- [x] 活动屏幕为远端时，将非 `MouseMoveEvent` 输入发送给远端 Client。
- [x] 按 `targetDelta = sourceDelta` 处理远端 active screen 上的连续鼠标移动。
- [x] 处理远端屏幕切回本机屏幕。
- [x] 处理跨多个远端屏幕。
- [ ] 增加相对鼠标运动或光标固定策略。

验收：

- [x] 右边缘切换测试通过。
- [x] 无邻居保持测试通过。
- [x] 从本机屏幕右边缘移动可进入远端屏幕并发送入口输入消息。
- [x] Server 侧键盘事件可进入远端发送队列。
- [x] Server 侧连续鼠标移动可进入远端发送队列。
- [x] 远端屏幕可通过左边缘切回本机屏幕。
- [x] 远端 active screen 可跨到同 endpoint 的下一块远端屏幕。
- [x] 完整 Mock 流程中，远端键盘按下/释放进入 Client injector。
- [x] 完整 Mock 流程中，鼠标回到本机后键盘事件不再转发给远端。
- [ ] 键盘事件在真实远端系统生效。
- [ ] 鼠标事件在真实远端系统生效。

记录：

- [x] 已新增 `test_server` 覆盖右边缘切换和无邻居保持。
- [x] 已新增 `test_server` 覆盖远端发送队列中的 `InputMessage`。
- [x] 已新增 `test_input_pipeline` 覆盖 Server -> RpcTransport -> Client -> MockInjector。
- [x] `test_input_pipeline` 已覆盖完整 Mock 操作流程。
- [x] `test_server` 和 `test_input_pipeline` 已覆盖远端 active screen 上的连续鼠标移动。
- [x] `test_server` 已覆盖远端屏幕切回本机屏幕。
- [x] `test_server` 已覆盖跨多个远端屏幕。

### M7：配置与安全

Todo：

- [x] 新增 `AppConfig`。
- [x] 配置可保存本机 `machineId`。
- [x] 配置可保存屏幕网格位置。
- [x] 配置可保存可信 Client 列表。
- [x] 配置可读写 JSON 文件。
- [x] CLI/运行入口加载配置文件。
- [x] Server 持久化当前屏幕布局到配置文件。
- [x] 配置屏幕网格位置。
- [x] 配置可信 Client。
- [ ] 配置认证策略。
- [x] 保存机器名。
- [ ] 保存 endpoint。
- [x] 保存屏幕布局。
- [x] 引入稳定 `machineId`。

验收：

- [x] `test_config` 覆盖配置序列化和反序列化。
- [x] `test_config` 覆盖配置文件保存和加载。
- [x] `test_config` 覆盖 `machineId` 缺失时生成。
- [x] `test_config` 覆盖可信 Client 名称白名单。
- [x] `test_server` 覆盖 Server 使用配置中的屏幕网格位置。
- [x] `test_server` 覆盖可信 Client 接入判断。
- [x] `test_server` 覆盖可信 Client `machineId` 判断。
- [x] `test_rpc_transport` 覆盖 `HelloMessage.machineId` 往返。
- [x] `test_config` 覆盖屏幕布局 upsert。
- [x] `test_server` 覆盖屏幕布局写入配置文件。
- [x] 重启后拓扑布局稳定。
- [x] 拓扑布局不依赖连接顺序。
- [ ] 未授权 Client 不能注入输入。

## 当前开放问题

- `ScreenKey.ownerId` 已优先使用稳定 `machineId`；后续需要考虑旧配置和 endpoint 回退数据的迁移。
- 后续仍需决定是否引入本机光标固定/回拉，以解决真实系统边界处绝对坐标不再变化的问题。
- 自动布局只能作为临时策略，正式体验应以配置文件为准。
- 是否需要边缘吸附阈值，例如距离边缘 1-2 像素就触发，而不是必须等于边界。
- 鼠标速度按 DPI、DPS、系统指针速度还是原始输入缩放，需要后续定义策略。
  当前阶段先不实现复杂手感统一。
