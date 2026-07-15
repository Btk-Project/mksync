# mksync

## 简介 ([English](/README.md))

mksync 可以在 Windows 和 Linux 系统的多台计算机之间同步鼠标和键盘。（macOS 可能在未来的计划中）

它还支持剪贴板共享、文件传输和拖放文件。（未来将通过插件添加支持）

## 它是什么？
Mksync 是一款免费的开源键盘和鼠标共享库和应用程序。

mksync 中的应用程序模拟了 KVM 切换器的功能，在历史上，KVM 切换器可以让你使用一个键盘和鼠标控制多台计算机，方法是转动盒子上的拨盘，随时切换你要控制的机器。 Mksync 通过软件实现了这一功能，你只需将鼠标移动到屏幕边缘，或通过按键将焦点切换到不同的系统，就能告诉它要控制哪台电脑。

该 app 的基本功能没有图形用户界面，但我们采用插件方式来支持图形用户界面。

该 mks 库是 mksync 的开发库，您可以使用 mksync 的协议来开发类似的应用程序，或者为 mksync 开发插件以获得更多功能。

Mksync 是基于 synergy、barrier 和 input-leap 的灵感而开发的。你也可以说 mksync 是它们的重构版本。

## 有什么不同？

由于在使用 synergy 和 barrier 时发现连接体验不佳。我们需要事先在服务器上设置可能连接的用户的用户名，并设置该用户的屏幕位置。只有做好这些准备工作后，我们才能使用同名客户端进行连接。这非常不方便，而且当某些电脑有多个屏幕时，我们无法像扩展屏幕那样移动它们！

而且它们不支持使用剪贴板跨系统传输文件，也不支持拖放文件。

由于上述问题的存在，我们决定在 synergy 和 barrier 的基础上进行重构。并决定使用插件化解决方案来提供更多内容。

## 项目目标

#### 版本 v0.1.0
在 Windows 和 Linux 系统的多台计算机之间同步鼠标和键盘。（基本功能）

#### 版本 v0.2.0
实现插件管理器。为剪贴板共享、文件传输、文件拖放提供 mks 库支持。

#### 版本 v0.3.0
实现剪贴板共享。

#### 版本 v1.0.0
实现文件传输。（此时，mksync 将发布正式版本）

#### 版本 v1.0.0 之后
只有在正式版本推出后，我们才会根据需要实现文件拖放和对更多系统（如 mocOS）的支持。 当然，图形用户界面也计划在正式版本发布后实现。

## 如何构建?
mksync 使用 Xmake 3.0.9 或更高版本构建。默认且最低支持 C++23；C++26 仅用于
GCC 16.1 兼容性验证，不支持 C++20 或更低标准。Linux 最低构建和安装包基线为
Ubuntu 24.04、GCC 14 与 Qt 6。

Ubuntu 24.04 上完整构建环境的安装示例：

```bash
sudo apt install gcc-14 g++-14 libei-dev libfmt-dev libgmock-dev libgtest-dev \
  libportal-dev libspdlog-dev libwayland-bin libwayland-dev \
  libxcb1-dev libxcb-keysyms1-dev libxcb-randr0-dev libxcb-xinput-dev \
  libxcb-xtest0-dev libxkbcommon-dev pkg-config qt6-base-dev qt6-declarative-dev \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-dialogs \
  qml6-module-qtquick-layouts wayland-protocols x11proto-dev
export CC=gcc-14 CXX=g++-14
```

### 配置

普通 CLI 构建不要求 Qt：

```bash
xmake f -c -m debug --stdcxx=23 --enable_gui=n
xmake build mksync
```

后端是独立的编译选项；关闭后不会探测其系统包，也不会编译相应源文件：

```bash
xmake f -c --stdcxx=23 \
  --enable_backend_x11=y \
  --enable_backend_wayland_wlr=n \
  --enable_backend_wayland_portal=n
```

Windows 使用 `--enable_backend_win32`。Linux 的 XCB、Wayland、libei 和 portal 是
系统强相关依赖，因此构建文件只通过系统 `pkg-config` 查找，不会由 Xmake 下载另一份。

### 编译

运行全部测试：

```bash
xmake test
```

### Qt 6 图形界面

Qt/QML 前端位于独立的 [`ui/`](ui/README.md) 目录，默认不会参与命令行构建。屏幕布局和
可信设备保存在 `AppConfig` JSON 中；启动模式、端点、JSON 路径和日志等级使用 CLI 的
`CliCommand` 数据模型，可直接导入 `--export-toml` 生成的文件并导出给 `--import-toml`。

```bash
xmake f -m debug --enable_gui=y
xmake build mksync-gui
xmake run mksync-gui
```

Linux GUI 构建要求发行版提供完整的 Qt 6 Quick SDK；若 Xmake 未自动选中，请传入
`--qt=/usr`。缺少 Linux SDK 时会直接报告错误，不再回退到 Xmake 的预编译 `qt6quick` 包；
Windows CI 和发布构建使用 Xmake 提供的 SDK。

### 安装包与 Release

`lua/pack.lua` 会把 CLI 和启用后的 GUI 一起交给 Xpack。示例：

```bash
xmake f -c -m release --stdcxx=23 --enable_gui=y --enable_tests=n
xmake pack -f targz -o dist       # Linux 归档
xmake pack -f deb -o dist         # Debian/Ubuntu 安装包
```

Windows 使用 `xmake pack -f nsis,zip -o dist`；Xmake 的 Qt 安装钩子会自动调用
`windeployqt`。推送 `vX.Y.Z` tag 后，GitHub Actions 会自动生成 Linux 和 Windows
安装包并附加到 Release。完整操作与临时 artifact/正式 Release asset 的区别见
[`docs/releasing.md`](docs/releasing.md)。
当前 Linux 安装包以 Ubuntu 24.04 为最低基线；后续计划增加 Flatpak，提供与宿主发行版
运行库解耦的分发方式。

### 输入后端

后端会在支持的平台自行注册并实现能力检查。自动模式按注册顺序选择第一个满足 server 或
client 所需能力的后端，也可以在 CLI/GUI 中显式指定：

```bash
mksync backend --list --checked
mksync server 0.0.0.0:1234 --backend x11
mksync server 0.0.0.0:1234 --backend wayland-portal
mksync client 192.0.2.10:1234 --backend wayland-portal
```

`wayland-portal` 使用 XDG InputCapture/RemoteDesktop Portal + libei 实现捕获与注入；
`wayland-wlr` 仅负责 Wayland/wlroots 协议能力。两者不混用 X11 API，也不彼此交织。
详细能力边界和运行时要求见
[`docs/wayland_backend.md`](docs/wayland_backend.md)。
# FIXME
- [ ] 增加trace日志，跟踪事件流，以便调试复现完整捕获传输流程。
- [ ] 鼠标移动到其他屏幕后其他屏幕未显示鼠标指针，也没有接收到事件的感觉（增加具体event日志以便调试）
- [ ] 鼠标在边缘移动后回到自己的屏幕时会从非边缘的地方出现，回到自己屏幕时应该移动到对应逻辑位置再出现。
