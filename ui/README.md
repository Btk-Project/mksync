# mksync Qt GUI

`ui/` 是与 `src/` 命令行实现隔离的 Qt 6/QML 前端。它采用 MVP：

- `model/` 只保存和读写现有的 `mks::AppConfig`；GUI 与 CLI 使用完全相同的配置格式。
- `presentation/` 将模型转换为 QML 属性，处理配置操作，并直接托管
  `mks::Server` / `mks::Client` 的 ilias 任务。
- `qml/` 只负责扁平化的现代界面和用户交互，不直接访问文件或配置结构。

程序在 Qt 的事件循环上安装 `ilias::QIoContext`。运行页会直接创建现有的
`mks::Server` / `mks::Client`，停止时取消其 `run()` 任务；GUI 不会启动或依赖一个外部
`mksync` 子进程。

## 构建

命令行版本仍是默认目标，且不需要 Qt：

```bash
xmake f -m debug
xmake build mksync
```

构建 QML GUI 时显式启用 Qt 目标：

```bash
xmake f -m debug --enable_gui=y
xmake build mksync-gui
xmake run mksync-gui
```

Xmake 使用 `qt.quickapp` 规则发现系统 Qt SDK，并链接 Qt 6 Core、Gui、Qml、Quick、Quick Controls 2 与 Quick Dialogs 2。若 Qt SDK 未被自动发现，可在配置命令中增加 `--qt=/path/to/Qt/6.x/<kit>`。

GUI 启动时会载入或创建当前目录的 `mksync.json`，与 `mksync server`/`mksync client`
的默认配置路径一致；“导入文件”和“另存为”可管理任意本地配置文件。运行模式、服务端
和客户端各自的 IP/端口、日志等级会作为桌面应用偏好保存，运行期间会锁定这些参数。
核心 spdlog 输出会显示在运行页；关闭窗口时先取消运行任务再退出。
