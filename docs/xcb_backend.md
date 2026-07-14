# Linux/X11 XCB 后端

`src/platform/xcb.cpp` 是纯 XCB 后端：连接、事件队列、XInput2、RandR、XTest、grab、
光标和按键映射均使用 XCB API。代码只从 `X11/keysym.h` 与 `X11/XF86keysym.h` 读取
keysym 常量，不链接或调用 Xlib、libXi、libXrandr、libXtst，也不存在一个连接同时由
Xlib/XCB 消费事件的情况。

## API 与所有权边界

`src/platform/xcb_connection.*` 是连接的 RAII 边界：

- `XcbConnection::connect()` 建立并检查连接；
- 析构函数是唯一调用 `xcb_disconnect()` 的位置；
- `check()` 处理 checked void request；
- `flush()` 统一检查连接错误；
- 持有者是对应 reply/event queue 的唯一消费者。

后端有三条互不共享的连接：

| 所有者 | 职责 | 是否消费事件 |
| --- | --- | --- |
| `XcbPlatform` | 查询 XI2/RandR、枚举屏幕 | 否，只读取同步 reply |
| `XcbInputCapture` | 选择并解析 XI2 raw/core grab 事件 | 是，唯一调用 `xcb_poll_for_event` |
| `XcbInputInjector` | XTest 注入、注入后位置检查 | 否，只读取自己的同步 reply |

这个拆分避免两类风险：不同线程/对象竞争同一 reply queue，以及 Xlib 内部队列已经读取
socket 数据、但 XCB/ilias 仍在等待 fd 的混合队列死锁。

## 捕获

初始化流程：

1. 打开 capture 专用 XCB connection；
2. 用 `xcb_input_xi_query_version` 协商 XI 2.1；
3. 用 `xcb_input_xi_select_events_checked` 选择 raw key/button/motion；
4. 把 `xcb_get_file_descriptor()` 返回的借用 fd 注册到 ilias `Poller`；
5. `nextEvent()` 先清空 `xcb_poll_for_event()` 队列，再异步等待 `POLLIN`。

远端控制时使用 XCB core grab。XI 2.1 可在 grab 期间继续提供 raw event；旧服务器回退到
grab window 的 core event。关闭时先 cancel/close poller，再释放 grab/cursor/key symbols，
最后由 `XcbConnection` 关闭 socket。

## 注入与屏幕

- 屏幕优先通过 RandR 1.5 `GetMonitors` 枚举，旧服务器回退到 X setup roots；
- 鼠标移动、按键、按钮和滚轮通过 `xcb_test_fake_input_checked` 注入；
- keysym/keycode 转换使用 `xcb-keysyms`；
- 每个 request 的协议错误通过 `IoResult` 返回，创建平台失败等致命初始化错误才抛异常；
- Wayland 会话不会把 XWayland 误报成系统级捕获/注入后端。

## 构建边界

`--enable_backend_x11=y` 才会添加 `xcb.cpp`、`xcb_connection.cpp` 以及下列系统
`pkg-config` 依赖：

- `xcb`
- `xcb-keysyms`
- `xcb-randr`
- `xcb-xinput`
- `xcb-xtest`

这些依赖在 `lua/backends.lua` 中全部标记为 `system = true`。关闭选项后不会查找上述包，
也不会编译任何 X11 后端源文件。

## 验证状态

- [x] C++23 下 CLI 与 Qt GUI 都完成纯 XCB 源码的全量编译和链接；
- [x] 全部 Linux 后端同时启用时构建通过；
- [x] X11/Wayland/portal 全关闭时，目标不含对应源文件和库；
- [x] Xpack 归档中包含 CLI、GUI 与项目共享库；
- [ ] 纯 XCB 改造后仍需在真实 Xorg 会话运行 `mksync --check-platform --backend x11`；
- [ ] 仍需做两台真实设备的捕获、grab、注入和多屏坐标验收；
- [ ] GCC 16.1/C++26 由 Docker/GitHub CI 独立验证，不作为默认构建要求。
