# XCB 后端设计讨论

本文档只讨论 Linux/X11 后端。当前 `src/platform/xcb.cpp` 已经能用 XInput2 捕获输入，
并且捕获循环已经改为直接使用 ilias 的 IO 后端监听 X connection fd，不再创建独立
`std::jthread`。

## 目标

- [x] XCB/XInput2 capture 不再创建独立线程。
- [x] X connection fd 通过 ilias `Poller` 注册到当前运行时。
- [x] `InputCapture::initialize()` 完成 XInput2 事件选择并建立 poller。
- [x] `InputCapture::nextEvent()` 在协程里等待 fd 可读并解析事件。
- [x] `InputCapture::shutdown()` 取消 poller，关闭 display，清理队列。
- [x] 保留 `Platform` / `InputCapture` / `InputInjector` 的跨平台抽象边界。

## 当前注入器状态

- [x] `XcbPlatform::createInjector()` 已返回真实 `XcbInputInjector`。
- [x] `XcbInputInjector` 使用独立 `Display *`，不复用平台枚举屏幕的 display。
- [x] 鼠标移动使用目标 `screenIndex/x/y` 映射到 X root 坐标后 `XWarpPointer`。
- [x] 鼠标按钮、滚轮和键盘事件使用 XTest 注入。
- [x] Xmake Linux 依赖增加 `libxtst`。
- [x] 新增 `mksync --check-platform`，用于真机 smoke check XInput2 capture 与 XTest injector 初始化。
- [x] 当前 GCC 14.2 / C++23 环境下，Linux/XCB 主目标编译和链接通过。
- [x] Linux/X11 真机 `mksync --check-platform` 通过，确认 XInput 2.0、XTest 2.2 可用。
- [x] Linux/X11 真机捕获到连续 `MouseMoveEvent`，确认 XInput2 事件解码可用。
- [ ] 尚未做 GCC 16.1 / C++26 Linux 真机编译和注入验收。

## 已移除的问题

旧实现的核心流程是：

1. `XcbInputCapture::initialize()` 创建 `mpsc` channel。
2. 启动 `std::jthread`。
3. 线程内打开 X display，选择 XInput2 raw events。
4. 线程内用 `poll(XConnectionNumber(display), POLLIN, 100)` 等事件。
5. 收到事件后翻译为 `InputEvent`，再写入 channel。
6. `nextEvent()` 从 channel 读取事件。

这个方案已移除，原因是它和项目框架不完全一致：

- X display connection 本质是 fd/socket，可由 ilias 原生 poll。
- 独立线程绕开了结构化并发，取消和 shutdown 更难统一。
- 线程内 `poll(..., 100)` 存在固定唤醒周期，不如运行时事件驱动。
- channel 变成线程和协程之间的桥，但 capture 本身其实可以直接是协程。

## 当前方案

XCB/XInput2 后端把 X connection fd 当成 pollable fd：

```cpp
const auto fd = ::XConnectionNumber(display);
ILIAS_CO_TRY(auto poller, co_await ilias::Poller::make(fd, ilias::IoDescriptor::Socket));
```

事件读取循环放在 `nextEvent()` 中：

```cpp
while (::XPending(display) == 0) {
    ILIAS_CO_TRY(auto events, co_await mPoller.poll(POLLIN));
    if ((events & POLLIN) == 0) {
        continue;
    }
}

XEvent event {};
::XNextEvent(display, &event);
```

`nextEvent()` 直接循环读取并返回第一个可翻译的 `InputEvent`。这样 capture 不再需要
独立线程，也不需要用 channel 把事件从线程送回运行时。

## fd 所有权

由 `XcbInputCapture` 持有捕获用 `Display *`：

- `initialize()` 打开 display。
- `initialize()` 调用 `XISelectEvents`。
- `initialize()` 用 `XConnectionNumber(display)` 创建 ilias poller。
- `shutdown()` 先 cancel/close poller，再 `XCloseDisplay`。

注意：`Poller::make(fd, Socket)` 只负责把 fd 注册到 ilias，不应接管 Xlib display 的关闭。
X display 的生命周期仍然由 `XCloseDisplay` 管理。

## 取消语义

`nextEvent()` 等待 `mPoller.poll(POLLIN)` 时，应自然接受 ilias coroutine cancellation。
`shutdown()` 应调用：

```cpp
if (mPoller) {
    mPoller.cancel();
    mPoller.close();
}
```

然后关闭 display。`IoTask<void>` 成功返回要 `co_return {};`，`Task<void>` 成功返回才使用
裸 `co_return;`。

## Xlib 与 XCB 的边界

当前实现虽然文件名是 `xcb.cpp`，输入捕获主要走 Xlib/XInput2：

- `XOpenDisplay`
- `XISelectEvents`
- `XPending`
- `XNextEvent`
- `XGetEventData`

屏幕枚举使用了 `xcb_connect` / `xcb_setup_roots_iterator`。这可以暂时保留。
后续如果完全切到 XCB，需要重新评估 XInput2 event cookie 的解析和 key symbol 映射。
短期目标不是“纯 XCB”，而是“X connection fd 由 ilias poll，不再独立线程”。

## 实现步骤

- [x] 移除 `XcbInputCapture::mThread`。
- [x] 移除 `XcbInputCapture::run()`。
- [x] 移除 `std::stop_token` 和 `std::latch` 启动同步。
- [x] `initialize()` 打开 capture display，并保存到成员。
- [x] `initialize()` 选择 XInput2 raw events。
- [x] `initialize()` 创建 `ilias::Poller`。
- [x] `nextEvent()` 循环 `poll(POLLIN)` + `XPending` + `XNextEvent`。
- [x] `shutdown()` 取消 poller 并关闭 display。
- [x] 保留事件翻译函数：`translateMouseMove`、`translateButtonEvent`、`translateKeyEvent`。
- [x] 补当前环境 GCC 14.2 / C++23 Linux 可编译检查。
- [x] 补 Linux/X11 真机事件解码检查。
- [ ] 补 GCC 16.1 / C++26 Linux 可编译检查。

## 暂不处理

- [ ] 不在本次改造中实现 Wayland 后端。
- [ ] 不在本次改造中重写为纯 XCB。
- [ ] 不在本次改造中实现复杂 DPI/DPS 鼠标手感策略。
- [ ] 不在本次改造中处理远端 active screen 的连续鼠标移动。
