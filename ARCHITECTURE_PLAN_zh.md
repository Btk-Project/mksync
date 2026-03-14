# mksync 架构分析与重构计划书

## 1. 目标与结论摘要

### 1.1 项目目标理解
根据 `README.md`，项目目标是做一个类似 Input Leap / Barrier / Synergy 的跨设备输入共享软件，并在后续加入剪贴板、文件传输、拖拽与插件化能力。

当前代码已经具备了非常早期的基础雏形：
- 有跨平台抽象层 `platform`
- 有输入事件统一模型 `InputEvent`
- 有网络协议草案 `proto`
- 有服务端骨架 `server`
- 有基于 Win32 Hook 的鼠标采集原型

但从代码成熟度看，项目目前仍处于“验证平台采集可行性”的阶段，离“可用的 Input-Leap 类产品”还有较大距离。

### 1.2 核心结论
当前代码最大的问题不是“某个模块实现不完整”，而是“产品级架构主干还没有真正建立起来”。

目前项目更像：
1. 一个平台输入捕获实验
2. 一个未落地的协议草案
3. 一个未接通的服务端雏形

而不是一个已经形成“输入采集 -> 会话路由 -> 网络同步 -> 远端注入 -> 屏幕切换状态机”的闭环系统。

因此，最优策略不是直接继续堆功能，而是先把项目重构成稳定的分层架构，再分阶段补齐：
- 设备采集/注入
- 会话与屏幕拓扑
- 事件总线与状态机
- 协议编解码
- 服务端/客户端角色
- 插件扩展边界

---

## 2. 当前代码架构分析

## 2.1 目录结构
当前 `src` 目录结构如下：

- `src/main.cpp`：程序入口，当前仅测试输入捕获
- `src/core/`：跨平台键鼠基础类型
- `src/platform/`：平台抽象接口与 Win32 实现
- `src/proto/`：协议消息类型草案
- `src/server/`：服务端骨架
- `src/net/`：空目录
- `src/ui/`：空目录

### 架构现状判断
从命名上看，项目原本打算走“core / proto / net / platform / server / ui”的分层路线，这个方向是对的；
但实际上：
- `net` 尚未成型
- `server` 没有和 `main` 串起来
- `proto` 只有类型枚举，没有编解码
- `platform` 只实现了 Win32 鼠标采集的一部分
- `ui` 还不存在

因此当前是“目录上有分层、运行时没有分层闭环”。

---

## 2.2 当前运行路径
实际入口在 `src/main.cpp`。

当前运行流程是：
1. 创建 `Platform`
2. 创建 `InputCapture`
3. 初始化捕获器
4. 循环读取事件
5. 打日志输出
6. Ctrl+C 时退出

这说明当前主程序并没有进入“服务端模式”或“客户端模式”，只是一个本地 Hook 事件查看器。

### 影响
这意味着以下核心产品能力都还未接通：
- 主机/从机角色划分
- 网络连接建立
- 协议握手
- 输入事件转发
- 远端输入注入
- 焦点切换/屏幕切换

---

## 2.3 模块职责分析

### 2.3.1 core 层
`src/core/key.hpp` 和 `src/core/mouse.hpp` 提供了跨平台的键鼠枚举，方向正确。

优点：
- 试图统一平台无关输入表示
- 以 USB HID 风格描述键值，利于协议传输

问题：
- 只解决了“键值定义”，没有解决“按键状态模型”
- 缺少组合键、重复键、系统键、IME/布局映射策略
- 没有定义输入时间戳、设备来源、注入标记等关键元数据

结论：
`core` 适合作为“领域模型层”的起点，但目前还不够支撑产品级输入同步。

### 2.3.2 platform 层
`src/platform/platform.hpp` 定义了：
- `InputEvent`
- `InputCapture`
- `InputInjector`
- `Platform`

这是当前项目里最接近“正确抽象”的一层。

优点：
- 已经把平台依赖放在抽象接口后面
- 已经意识到 Capture / Injector 是两个不同职责
- 未来可以适配 Windows / Linux / macOS

问题：
- `Platform` 只暴露工厂，没有系统能力查询接口
- `ScreenInfo` 没真正接入
- `InputEvent` 设计过于原始，扩展性不足
- 事件抽象没有区分“本地原始事件”和“可同步网络事件”

结论：
`platform` 层方向正确，但还缺少一个更上层的“会话/路由层”来消化平台事件。

### 2.3.3 Win32 实现层
`src/platform/win32.cpp` 是当前最完整的实现部分，主要职责是：
- 创建消息线程
- 枚举显示器
- 安装鼠标/键盘低级 Hook
- 将事件写入 mpsc channel

优点：
- 采用了专门 UI 线程管理 Hook 与消息循环
- 通过 channel 把平台线程与消费协程解耦
- 为后续的平台扩展打下了基础

问题很多，且有几个是高优先级问题：
- 只实现了鼠标事件，键盘 Hook 为空
- 没有实现注入器 `InputInjector`
- `WM_DISPLAYCHANGE` 只打日志，不刷新屏幕拓扑
- Hook 回调中使用 `blockingSend`，存在阻塞系统输入链路的风险
- 全局 `thread_local` Hook 状态意味着当前设计天然只支持单实例捕获器
- 监视器变化没有向上游发送 `ScreenChange`
- 对注入事件没有做过滤，未来会有“本机注入再被重新捕获”的回环风险

结论：
Win32 层更像 PoC，而不是产品级输入后端。

### 2.3.4 proto 层
`src/proto/message.hpp` 目前定义了消息类型：
- Hello / HelloAck
- Ping / Pong
- ScreenInfo
- Mouse / Key 事件
- Error

优点：
- 已经意识到需要协议层解耦
- 消息类型命名清晰，适合作为第一版协议骨架

问题：
- 没有真正的序列化/反序列化实现
- 协议头定义过于简单，缺少版本、flags、seq、session、payload type length 等信息
- 没有鉴权、能力协商、压缩/扩展字段
- 没有区分控制面与数据面

结论：
现在不是“协议模块”，而是“协议枚举草稿”。

### 2.3.5 server 层
`src/server/server.cpp` 已经表达出未来思路：
- 一边收集本地输入事件
- 一边监听网络连接
- 为每个连接生成处理协程

优点：
- 采用协程模型，利于后续扩展并发连接
- 已经具备 service skeleton

问题：
- `collectEvents()` 为空
- `networkHandle()` 为空循环
- 没有客户端状态、没有屏幕映射、没有连接注册表
- 没有会话管理、没有主从切换状态机
- `main.cpp` 没有使用 `Server`

结论：
当前 server 还不是服务端，只是类名叫 Server。

---

## 3. 与 Input-Leap 类软件相比，当前缺失的核心架构能力

如果目标是做“类似 Input Leap 的软件”，最少要有以下 6 条主线能力：

1. **本地输入采集**
   - 鼠标移动、按键、滚轮、修饰键、热键
2. **本地输入注入**
   - 远端事件落地到本机 OS
3. **屏幕拓扑与焦点切换**
   - 鼠标越界切换、屏幕布局、DPI/坐标转换
4. **会话与连接管理**
   - 主从注册、重连、心跳、能力协商
5. **输入路由状态机**
   - 当前输入属于本机还是远端，何时切换、何时锁定
6. **协议与扩展能力**
   - 剪贴板、文件、插件、权限控制

当前代码只触及了第 1 条的一小部分。

---

## 4. 代码级关键问题清单

以下问题会直接影响架构演进，建议优先修正。

### 4.1 启动死锁风险
在 `Win32Platform` 构造函数中，主线程等待 `latch.wait()`；但平台线程只有在初始化全部成功后才 `count_down()`。

这意味着如果 `IsGUIThread`、DPI 设置、窗口创建、显示器枚举任一步失败，构造函数会永久卡死。

这是当前最严重的问题之一。

### 4.2 输入事件模型不安全且不完整
`InputEvent` 使用裸 `union` 表达不同事件类型，问题包括：
- 不易扩展滚轮、文本输入、热键、原始扫描码等字段
- 缺少时间戳、事件序号、来源设备、是否注入等元数据
- 鼠标事件中的 `screenIndex` 在 Win32 转换里没有被赋值

建议改成 `std::variant` 或显式 tagged struct。

### 4.3 Hook 回调阻塞风险
`mouseHookProc()` 中使用 `blockingSend()` 发送事件。如果消费者阻塞、队列满、线程调度延迟，就可能拖慢甚至影响系统输入链路。

对 Input-Leap 类产品而言，输入路径必须是“低延迟且不阻塞”的。

### 4.4 平台线程与业务线程边界不清晰
当前 `Win32Platform` 同时承担：
- 线程管理
- 窗口管理
- 监视器管理
- Hook 生命周期
- 事件翻译
- 异步桥接

职责过重，后续加 Linux/macOS 实现时会迅速变复杂。

### 4.5 缺少核心领域层
项目少了一层最关键的“领域服务层”，它应该位于 `platform` 和 `server/client` 之间，负责：
- 屏幕拓扑管理
- 焦点切换状态机
- 输入路由决策
- 热键处理
- 连接与屏幕关系映射

没有这一层，未来逻辑会散落在平台层和网络层中，最终很难维护。

### 4.6 协议层不可演进
当前协议没有：
- 编解码器
- 版本协商
- flags / seq / ack
- 能力集声明
- 错误码体系

后续一旦要支持剪贴板、拖放、文件、插件，协议会非常难演进。

### 4.7 构建产物与源码结构存在漂移
当前源码只有一个 `mksync` 二进制目标；但 `bin/`、`build/` 中能看到历史上曾出现 `MksyncCore`、`MksyncProto`、`MksyncBackend` 等更细的拆分产物。

这说明项目在模块边界上可能经历过变化，但源码与当前构建系统已经没有保持一致的模块表达。

建议重新明确模块边界，不要让“目录分层”和“构建分层”长期不一致。

---

## 5. 推荐目标架构

## 5.1 总体分层
建议重构为以下分层：

### A. platform 层
职责：仅处理与操作系统直接相关的事情。
- 采集：keyboard/mouse/screen
- 注入：keyboard/mouse
- 系统信息：显示器、DPI、权限能力
- 原始事件转换

建议子模块：
- `platform/common/`
- `platform/win32/`
- `platform/x11/`
- `platform/wayland/`（后续）

### B. domain 层
职责：产品核心逻辑，不依赖 OS API。
- `InputRouter`
- `FocusController`
- `ScreenTopology`
- `SessionState`
- `HotkeyManager`
- `ClipboardDomain`（后续）

这是整个项目最应该新增的一层。

### C. transport 层
职责：连接与协议实现。
- 握手
- 心跳
- 编解码
- 会话恢复
- 背压控制
- 重连策略

建议拆成：
- `proto/`：纯消息模型 + 编解码
- `transport/`：连接、会话、帧收发

### D. app/service 层
职责：把 domain 和 transport 组合成具体角色。
- `host`：主机端
- `client`：受控端
- `discovery`：自动发现
- `config`：配置加载

### E. plugin 层
职责：后续扩展功能。
- clipboard
- file-transfer
- drag-drop
- ui-bridge

### F. ui 层
职责：配置界面、屏幕布局编辑、日志展示。

---

## 5.2 推荐运行时数据流

### Host 模式
1. PlatformCapture 捕获本地输入
2. Domain Router 判断当前焦点是否仍在本机
3. 若鼠标越过边界，则切换到某个远端 Session
4. 事件转换为网络协议帧
5. Transport 发给目标 Client
6. Client 端 PlatformInjector 注入事件

### Client 模式
1. 接收网络输入事件
2. Session 校验来源与顺序
3. Domain 处理焦点/锁定状态
4. PlatformInjector 落地到 OS
5. 回传心跳、屏幕信息、能力状态

---

## 5.3 推荐核心对象模型
建议引入以下核心对象：

- `InputFrame`
  - 一条已标准化的输入事件，包含时间戳、seq、source、payload
- `SessionId`
- `ScreenId`
- `NodeId`
- `TopologyGraph`
- `FocusTarget`
- `CapabilitySet`
- `RouteDecision`

这样做的好处是：
- 协议层、平台层、业务层使用同一套稳定模型
- 后续支持插件能力时不需要反复改底层接口

---

## 6. 优化建议（按优先级）

## P0：必须先做，否则架构会越写越乱

### 6.1 建立真正的模块边界
建议把源码重构成如下结构：

- `src/app/`
- `src/domain/`
- `src/platform/`
- `src/proto/`
- `src/transport/`
- `src/plugins/`
- `src/common/`

并在 `xmake.lua` 中拆成多个 target：
- `mksync_common`
- `mksync_platform`
- `mksync_proto`
- `mksync_transport`
- `mksync_domain`
- `mksync_app_host`
- `mksync_app_client`

### 6.2 定义统一事件模型
新增统一输入模型，至少包含：
- event type
- timestamp
- sequence id
- source node
- target node（可选）
- mouse/keyboard payload
- injected flag
- screen id
- modifiers

### 6.3 引入焦点切换状态机
Input-Leap 类软件的核心不是“转发事件”，而是“正确切换控制权”。

至少需要以下状态：
- `LocalActive`
- `SwitchingOut`
- `RemoteActive`
- `SwitchingBack`
- `Locked`
- `DisconnectedFallback`

没有状态机，未来屏幕切换逻辑会非常脆弱。

### 6.4 把 Hook/注入与业务路由彻底分离
平台层只负责：
- 采集
- 注入
- 屏幕信息

业务层负责：
- 过滤本地热键
- 判断是否切屏
- 决定是否发网络
- 决定是否吞掉本地事件

---

## P1：尽快做，形成最小可用闭环

### 6.5 实现 Host / Client 双角色
当前只有 `Server` 雏形，建议明确分成：
- `HostApp`
- `ClientApp`

原因：
- Host 负责采集与路由
- Client 负责接收与注入
- 两端生命周期与状态不同

### 6.6 设计可演进协议 V1
建议协议帧结构：
- magic
- version
- frame type
- flags
- session id
- sequence
- payload length
- payload
- checksum（可选）

并引入消息类别：
- control：握手、心跳、错误、重连、能力协商
- input：鼠标、键盘、滚轮、焦点切换
- sync：屏幕信息、拓扑变更
- extension：剪贴板、文件、插件消息

### 6.7 完成最小注入闭环
Windows 上至少要补齐：
- 键盘事件采集
- 滚轮事件采集
- 鼠标注入
- 键盘注入
- injected 事件过滤

做到这一步，才能形成真正的“本机采集 -> 网络 -> 远端注入”的 MVP。

### 6.8 建立配置模型
类似 Input Leap 的产品必须有配置模型，而不仅是代码逻辑。

建议抽象：
- 节点列表
- 节点名称 / ID
- 屏幕拓扑关系
- 热键配置
- 安全策略
- 自动发现开关

---

## P2：进入产品化阶段

### 6.9 插件边界不要过早侵入主链路
README 里提到后续会做插件化，这个方向是对的，但不建议过早把主功能也插件化。

建议策略：
- 核心输入同步能力内建
- 剪贴板、文件传输、拖拽做插件
- UI 可以走插件/桥接

不要让插件系统成为 v0.1 的前置条件，否则主链路会被拖慢。

### 6.10 增加自动发现与零配置接入
这是超越 Input Leap 体验的关键机会。

建议后续支持：
- 局域网发现
- 首次配对确认
- 自动屏幕注册
- 临时会话码

### 6.11 增加可观测性
至少要补：
- 结构化日志
- 会话事件日志
- 输入延迟指标
- 丢包/重连统计
- 平台错误码映射

---

## 7. 分阶段实施计划

## Phase 0：收敛基础架构（1~2 周）
目标：把项目从 PoC 收敛为可持续开发骨架。

交付物：
- 重新整理模块目录
- xmake 多 target 化
- 新的统一事件模型
- 修复 Win32 启动死锁和 Hook 阻塞风险
- 建立基础单元测试框架

验收标准：
- 平台初始化失败不会卡死
- 事件模型可表达鼠标/键盘/滚轮/屏幕变化
- 代码中平台层与业务层边界清晰

## Phase 1：单平台 MVP 闭环（2~4 周）
目标：先在 Windows 上跑通最小闭环。

交付物：
- Win32 Capture 完整实现
- Win32 Injector 完整实现
- Host / Client 双角色
- TCP 协议 V1
- 固定拓扑配置
- 鼠标越界切屏

验收标准：
- 2 台 Windows 机器可稳定共享键鼠
- 支持主屏/副屏边界切换
- 键盘修饰键与鼠标按键正确同步

## Phase 2：产品核心能力完善（3~6 周）
目标：把 MVP 做到可持续使用。

交付物：
- 重连/心跳/错误恢复
- 多显示器拓扑
- 热键切换
- 注入回环过滤
- 基础配置文件与加载器
- 日志与诊断工具

验收标准：
- 网络抖动下能自动恢复
- 多显示器坐标映射稳定
- 异常断开时自动回退本地控制

## Phase 3：跨平台扩展（4~8 周）
目标：补 Linux 支持。

交付物：
- X11 capture/injector
- Linux 屏幕枚举
- 跨平台协议一致性测试

说明：
Wayland 支持难度显著更高，建议作为独立里程碑，不要与 X11 同批次承诺。

## Phase 4：扩展能力与插件（持续迭代）
目标：在主链路稳定后再开放高级能力。

交付物：
- Clipboard plugin
- File transfer plugin
- Drag-drop plugin
- GUI / 拓扑编辑器

---

## 8. 推荐的近期任务清单

建议下一批开发任务按以下顺序推进：

1. 修复 `Win32Platform` 初始化死锁问题
2. 重构 `InputEvent` 为可扩展结构
3. 补齐键盘采集与滚轮采集
4. 实现 Win32 `InputInjector`
5. 抽出 `ScreenTopology` 与 `FocusController`
6. 为 `proto` 增加编解码器
7. 把 `main.cpp` 改为真正的 Host / Client 启动器
8. 让 `server` 不再只是骨架，而是完整会话服务

---

## 9. 风险评估

### 高风险项
- Wayland 注入权限与实现复杂度
- 多显示器 DPI/缩放差异导致坐标漂移
- 注入事件被再次捕获形成回环
- Hook 阻塞导致输入卡顿
- 键盘布局差异导致跨平台键值不一致

### 中风险项
- 插件系统过早设计，拖慢主链路开发
- 协议字段设计不足，后续兼容成本高
- Host / Client 角色边界不清，导致代码复用混乱

### 低风险项
- UI 延后实现
- 构建系统拆分
- 日志与诊断增强

---

## 10. 我的最终建议

如果以“做一个类似 Input Leap，但连接体验更好、未来更易扩展的版本”为目标，我建议：

### 不要继续沿着当前骨架直接堆功能
原因是：
- 平台层职责已经偏重
- 缺少领域层
- 协议层还只是草稿
- 运行主链路没有闭环

### 最优路线是“先重构骨架，再做 MVP”
优先顺序应当是：
1. 重新分层
2. 定义统一事件模型
3. 建立焦点切换状态机
4. 跑通 Windows MVP
5. 再扩 Linux / 插件 / 剪贴板 / 文件传输

### 目标产品定位建议
可以把 mksync 的定位明确成：
- v0.x：更现代、可扩展、连接体验更好的 Input-Leap 替代品
- v1.x：在输入同步稳定后，扩展剪贴板和文件传输
- v2.x：自动发现、零配置、GUI、插件生态

这条路线最稳，也最符合当前代码基础。

---

## 10.5 当前推进状态（2026-03-14）

### 已完成
- `platform` 层已完成 Win32 输入采集/注入基础闭环，补上了：
  - 鼠标移动、按键、滚轮、键盘事件
  - 屏幕枚举与 `screens()`
  - injected 事件标记
- 新增 `app/host`、`app/client`，程序入口已切分为 host/client 两种运行模式
- 新增 `domain/topology`、`domain/focus`，开始把“屏幕拓扑 + 焦点切换”从平台层抽离
- 新增 `proto/codec`、`transport/session`，已经有明确的 framed protocol 基础
- Host/Client 已跑通最小握手：
  - `Hello`
  - `HelloAck`
  - `ScreenInfo` 双向交换
- 已补上最小控制协议雏形：
  - `FocusEnter`
  - `FocusLeave`
- Client 已新增最小 capture/control 闭环：
  - 在 remote focus 激活时观察 injected 鼠标移动
  - 光标离开进入边后再回到该边时，向 Host 回传 `FocusLeave`
- 已确认 `BufStream` 小包写入后需要显式 `flush()`，否则握手会卡住

### 当前仍未闭环的核心缺口
- 远端激活后的“返回本地”路径仍是 MVP
  - 当前 Client 的回传逻辑还只覆盖最小鼠标边界返回
  - 键盘/组合键/异常断线后的焦点恢复策略还不完整
  - 单机脚本很难模拟“非 injected 的真实物理输入”，因此仍需人工双开观察日志辅助验证
- 鼠标跨屏坐标换算仍是 MVP 级别
  - 目前只做了基础的边缘映射和 screenIndex 重写
  - 还没有绝对坐标归一化、DPI/缩放补偿、异构分辨率策略
- 连接模型仍是单活动客户端
  - `mActiveSender` 说明当前只能稳定服务一个远端 Session
  - 未来要升级成 `SessionManager + NodeId + TopologyBinding`

### 下一阶段建议
1. 把 Client 回传从“最小 FocusLeave”扩展为真正的 control plane
2. 增加会话状态同步：remote active / local active / reconnect / disconnect rollback
3. 为单机开发继续保留可观测性：
   - 强日志
   - 可触发的本地调试热键
   - `FocusEnter/FocusLeave` 控制帧
   - host/client 双开 smoke test
4. 然后再进入多客户端 / 配置化拓扑 / 剪贴板 等产品能力
## 11. 附：当前代码成熟度判断

| 维度 | 当前状态 | 判断 |
|---|---|---|
| 平台抽象 | 有接口 | 方向正确，但不完整 |
| Win32 输入采集 | 部分完成 | 仅鼠标，仍是 PoC |
| Win32 输入注入 | 未实现 | 核心缺失 |
| 协议模型 | 有草案 | 未形成可运行协议 |
| 网络传输 | 骨架存在 | 不可用 |
| 主程序组织 | 仅调试入口 | 不是产品入口 |
| 多平台能力 | README 有目标 | 代码未落地 |
| 插件化 | 只有概念 | 不应过早推进 |

### 总体评级
- 架构方向：B-
- 当前完成度：D+
- 可演进性：C-
- 作为 Input-Leap 类产品起点：可以，但必须先做架构收敛





