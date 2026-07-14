# mksync Qt GUI

`ui/` 是与 `src/` 命令行实现隔离的 Qt 6/QML 前端。它采用 MVP：

- `model/` 保存和读写屏幕布局使用的 `mks::AppConfig` JSON 文档。
- `presentation/` 将模型转换为 QML 属性，处理配置操作，并直接托管
  `mks::Server` / `mks::Client` 的 ilias 任务。
- `qml/` 只负责扁平化的现代界面和用户交互，不直接访问文件或配置结构。

启动参数直接使用 CLI 解析器产出的 `mks::CliCommand`。GUI 与命令行共用
`makeCliParserConfig()`、TOML 导入/导出和字段校验，不维护另一份启动配置结构。

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

Xmake 通过 `qt6quick 6.8.3` 的 `requires` 自动下载 Qt SDK，并由 `qt.quickapp` 规则链接 Qt 6 Core、Gui、Qml、Quick、Quick Controls 2 与 Quick Dialogs 2，无需预先安装 Qt 或传入 `--qt`。

GUI 启动时会载入或创建当前目录的 `mksync.json`。该 JSON 只保存屏幕布局、设备 ID 和
可信设备。配置页可以导入命令行 `--export-toml` 生成的启动参数，也可以把当前运行模式、
端点、JSON 路径和日志等级导出为 CLI 可直接通过 `--import-toml` 使用的 TOML；格式可参考
[`docs/server.toml`](../docs/server.toml)。核心 spdlog 输出会显示在运行页；关闭窗口时先取消
运行任务再退出。
