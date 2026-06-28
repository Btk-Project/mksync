# mksync 长期开发计划记录

当前详细开发计划文档位于：

- `docs/development_plan.md`
- `docs/xcb_backend.md`

当前关键决策：

- 屏幕拓扑采用正方形网格。
- 拓扑层只管理网格邻接，不把不同屏幕长期抽象成 `[0, 1]`。
- Client 必须上报真实 screen rect，Server 保存这些 rect。
- 平台捕获、Server 转发和 Client 注入都使用目标屏幕真实像素坐标。
- 归一化比例只允许作为边缘入口映射的临时计算，不作为运行时坐标系统。
- 当前阶段不做复杂手感或移速统一；鼠标 delta/DPI/DPS 策略留到最小链路跑通后。
- 优先实现 Mock Platform，以便拓扑和跨端输入流程可自动化测试。
- XCB/XInput2 capture 后续应使用 ilias poll 监听 X connection fd，不再使用独立线程。

当前进度：

- [x] M1 新增 `ScreenTopology`。
- [x] M1 增加 `test_topology`。
- [x] M3 Server 保存 Client endpoint 和 Hello name。
- [x] M3 Server 处理 `ScreensMessage`。
- [x] M3 Server 注册本机和远端屏幕。
- [x] M3 增加 `test_server`。
- [x] M4 新增 `MockPlatform`。
- [x] M4 新增 `MockInputCapture`。
- [x] M4 新增 `MockInputInjector`。
- [x] M4 增加 `test_mock_platform`。
- [x] M6 Server 内部 active screen 可从本机切到右侧远端屏幕。
- [x] M6 右边缘切换和无邻居保持已有测试覆盖。
- [x] M2 定义最小 `InputMessage`。
- [x] M2 增加 `InputMessage` 的 `RpcTransport` roundtrip 测试。
- [x] M2 增加 `InputMessage` 格式化测试。
- [x] M5 Client 初始化 `InputInjector`。
- [x] M5 Client 收到输入消息后调用 `InputInjector::inject`。
- [x] M5 增加 `test_client` 覆盖 `InputMessage` 到 `MockInputInjector`。
- [x] M6 Server 切入远端屏幕时发送入口 `MouseMoveEvent`。
- [x] M6 Server 在远端 active screen 上转发非 `MouseMoveEvent` 输入。
- [x] M6 增加 `test_input_pipeline` 覆盖 Server -> RpcTransport -> Client -> MockInjector。
- [x] M6 按 `targetDelta = sourceDelta` 处理远端 active screen 上的连续鼠标移动。
- [x] M6 处理远端屏幕切回本机屏幕。
- [x] M6 处理跨多个远端屏幕。
- [x] M5 实现 Win32 最小 `InputInjector`。
- [x] M5 Win32 鼠标移动、按钮、滚轮、键盘按下和释放使用真实平台 API 注入。
- [x] M7 新增 `AppConfig`。
- [x] M7 配置可保存 `machineId`、可信 Client 和屏幕网格布局。
- [x] M7 增加 `test_config` 覆盖配置 JSON 和文件读写。
- [x] M3 Server 注册屏幕时可优先使用配置中的网格位置。
- [x] M7 CLI/运行入口加载或创建配置文件并传给 Server。
- [x] M7 Server 支持可信 Client 名称白名单。
- [x] M2 `HelloMessage` 携带 `machineId`。
- [x] M3 Client 握手发送配置中的本机 `machineId`。
- [x] M3 Server 使用 `HelloMessage.machineId` 作为远端屏幕 owner id，缺失时回退 endpoint。
- [x] M7 Server 支持可信 Client `machineId` 白名单。
- [x] M7 Server 注册屏幕后回写并保存当前屏幕布局到配置文件。
- [x] M7 重启加载配置后，拓扑布局按 `machineId` 恢复且不依赖连接顺序。
- [x] M5 实现 XCB/Linux 最小 `InputInjector`，使用 XTest/Xlib 注入鼠标和键盘事件。
- [ ] M5 XCB/Linux 注入器尚未做 Linux 真机编译和注入验收。

下一步：

- [ ] 将 XCB/XInput2 capture 改为 ilias poll 后端。
- [ ] 完成真实 Server/Client 联调验收。
