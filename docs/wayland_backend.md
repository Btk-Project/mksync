# Wayland 后端

## 后端注册与选择

平台后端通过 `BackendRegistration` 自注册，注册项包含稳定名称、显示名称、顺序、
`check()` 和 `create()`。自动模式按 `order` 从小到大检查，并选择第一个满足运行模式要求的
后端：

- server：需要屏幕枚举和全局输入捕获；
- client：需要屏幕枚举和输入注入；
- `check-platform`：需要屏幕枚举、捕获和注入。

显式选择后端时不会回退到其他实现。注册器和结构化检查结果位于
`src/platform/backend.*`，CLI 与 GUI 使用同一份数据。

```bash
mksync backend --list
mksync backend --list --checked
mksync server 0.0.0.0:1234 --backend x11
mksync server 0.0.0.0:1234 --backend wayland-portal
mksync client 192.0.2.10:1234 --backend wayland-portal
mksync check-platform --backend wayland-wlr
```

`--backend` 也可以通过 `MKSYNC_BACKEND` 设置；未设置或设为 `auto` 时自动选择。

## `wayland-wlr` 的实现范围

该后端只使用 Wayland 客户端协议，不调用 Xlib/XCB、libinput、uinput 或 XTest：

| 能力 | 协议 | 当前状态 |
| --- | --- | --- |
| 输出枚举 | `wl_output` + `zxdg_output_v1` | 已实现 |
| 指针注入 | `zwlr_virtual_pointer_v1` | compositor 暴露扩展时可用 |
| 键盘注入 | `zwp_virtual_keyboard_v1` | compositor 暴露扩展时可用 |
| 全局输入捕获 | 无合适的通用 Wayland 协议 | 不支持 |
| 主显示器标志 | Wayland 无标准属性 | 固定选择首个输出 |

输出坐标优先采用 `xdg-output` 的逻辑坐标和逻辑尺寸，避免整数/分数缩放下把物理像素
误当作 compositor 坐标。键盘事件把项目使用的 USB HID usage 映射为
`zwp_virtual_keyboard_v1` 所需的 Linux evdev key code；不会转发来源机器的
`nativeCode`。

Wayland socket 的写入通过 ilias `Poller` 融合：后端借用 `wl_display` 的 fd，
`wl_display_flush()` 返回 `EAGAIN` 时异步等待 `POLLOUT`。关闭时先取消并注销 Poller，
再销毁协议对象和 display，避免悬挂协程继续访问已关闭 fd。

## `wayland-portal` 的实现范围

`wayland-portal` 是与 `wayland-wlr` 完全独立的后端，使用 XDG Desktop Portal 和 libei，
不调用 Xlib/XCB，也不复用 wlroots 虚拟输入对象。它的注册顺序优先于 `wayland-wlr`：

| 能力 | 接口 | 当前状态 |
| --- | --- | --- |
| 输出枚举 | InputCapture Zones / libei Regions | 授权会话建立后可用 |
| 全局输入捕获 | InputCapture + `ConnectToEIS` | 指针跨越外边界 barrier 后激活 |
| 输入注入 | RemoteDesktop v2 + `ConnectToEIS` | libei 提供所需设备后可用 |
| 捕获释放/光标返回 | InputCapture `Release(cursor_position)` | 已实现 |

`check()` 是无交互的：只读取 portal 的 `version`、`SupportedCapabilities` 和
`AvailableDeviceTypes` 属性，因此 CLI 列表和 GUI 自动刷新不会弹授权窗口。真正启动 server
或 client 时才会分别请求 InputCapture 或 RemoteDesktop 权限；拒绝授权会以
`permission_denied` 返回，显式选择不会回退到另一后端。

libportal 的回调运行在独立 GLib main context；异步结果通过 ilias oneshot channel 恢复调用
协程。EIS fd 由 ilias `Poller` 等待，事件不会通过阻塞循环占用 ilias 运行线程。捕获侧把
Linux evdev key code 转回项目的 USB HID usage，注入侧执行相反映射。

Linux 构建依赖提供 `libportal.pc` 和 `libei-1.0.pc` 的开发包。例如 Debian/Ubuntu 系通常
为 `libportal-dev`、`libei-dev`，不需要 `liboeffis`。

## 为什么当前不能捕获全局输入

核心 `wl_seat` 只把键盘/指针事件发给获得焦点的 surface，且客户端不能像 X11 那样取得
显式全局 grab。因此，`wayland-wlr` 的 capture `initialize()` 明确返回
`operation_not_supported`，不会伪装成功，也不会悄悄打开 evdev/uinput。

跨 compositor 的方案由上述独立 `wayland-portal` 后端实现。它仍取决于当前桌面环境的
portal 后端是否实现 InputCapture，以及 RemoteDesktop 是否提供 v2 `ConnectToEIS`；开发
包存在并不代表 compositor 的 portal 实现一定具备这些运行时能力，实际结果以
`mksync backend --list --checked` 为准。

参考：

- Wayland 输入模型：<https://wayland.freedesktop.org/docs/book/html/sect-Wayland-Protocol-wl_seat.html>
- XDG InputCapture Portal：<https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.InputCapture.html>
- libei：<https://libinput.pages.freedesktop.org/libei/api/>

## X11 与 XWayland 的边界

现有 `x11` 注册项仍复用旧 X11 实现，本轮没有改写其内部 Xlib/XInput2/XTest 代码。在
Wayland 会话中，`x11.check()` 会直接报告不适用，不再尝试连接 `DISPLAY`，因为 XWayland
不能捕获或注入原生 Wayland 窗口。

如果后续需要 XWayland 回退，应新增独立的 `xwayland` 注册项并明确它只能覆盖 XWayland
窗口，不能把它混入 `wayland-wlr`，也不能把它报告为系统级 Wayland 输入能力。

## GUI

配置页提供后端下拉框（`auto` 或当前平台已注册的名称）和“重新检查”按钮。检查区显示每个
后端的连接、屏幕、捕获和注入结果；运行页显示实际选中的后端。GUI 导入/导出的 TOML 与
CLI 共用 `CommonConfig.backend` 字段。
