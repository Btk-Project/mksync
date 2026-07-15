# mksync 长期开发计划记录

当前详细开发计划文档位于：

- `docs/development_plan.md`（功能里程碑 M1–M8 + 开放问题）
- `docs/framework.md`（当前架构快照）
- `docs/xcb_backend.md`（Linux/X11 纯 XCB 边界与验收状态）

当前关键决策：

- 屏幕拓扑采用正方形网格。
- 拓扑层只管理网格邻接，不把不同屏幕长期抽象成 `[0, 1]`。
- Client 必须上报真实 screen rect，Server 保存这些 rect。
- 平台捕获、Server 转发和 Client 注入都使用目标屏幕真实像素坐标。
- 归一化比例只允许作为边缘入口映射的临时计算，不作为运行时坐标系统。
- `MouseMoveEvent` 可带 `deltaX/deltaY`；远端连续运动优先相对增量。
- 当前阶段不做复杂手感或移速统一；DPI/DPS 策略后置。
- 优先 Mock Platform 自动化测试；真机联调仍是未完成验收。
- X11 已改为纯 XCB；平台、capture、injector 各自独占连接与 reply/event queue。
- 默认语言标准是 C++23，只允许显式切换到 C++26；低于 23 不受支持。
- Linux CI 和安装包基线是 Ubuntu 24.04、Clang 18、libstdc++ 14 与 Qt 6，不为旧 glibc 降低
  C++/Qt 基线。
- Xmake 后端依赖按编译选项启用；Linux 平台库只使用系统 `pkg-config`。
- tag 发布通过 Xpack + GitHub Actions 生成 Linux/Windows 二进制包。
- 后续增加 Flatpak 作为与宿主发行版运行库解耦的 Linux 分发方式。
- 代码风格：Result/IoTask、结构化并发、避免 god class / 巨型 dispatch 面条。

当前进度（摘要）：

- [x] M1–M4：拓扑、协议最小集、Mock Platform、相关测试。
- [x] M5–M6：Client 注入、Server 跨屏与远端连续移动、Win32/XCB 最小 injector（xcb 细节见专文）。
- [x] M7：AppConfig / machineId / 布局持久化 / 信任白名单（空列表仍=开发态全信任）。
- [x] 2026-07-10：完成代码 review；文档与 M8 债务清单同步。
- [ ] M5/M6 真机注入与跨机联调验收。
- [ ] M7 认证策略与“未授权不能注入”。
- [ ] M8 可靠性与结构拆分。

下一步（与 `docs/development_plan.md` M8 对齐）：

1. **可靠性 P0**
   - [ ] 远端 `InputMessage` 队列背压/合并/丢弃策略可测。
   - [ ] Client 单次 inject 失败不断连。
   - [ ] 收紧信任模型；拒绝时回 `ErrorMessage`。
2. **结构 P1（非 xcb）**
   - [ ] 拆 `Server` 职责；去掉 app 层 `void*`。
   - [ ] 收敛 `mScreens` / `mTopology` 双状态；拆 `win32.cpp`。
3. **验收**
   - [ ] 真机 Server/Client 联调。
   - [ ] Linux 注入/边界对照 `xcb_backend.md`（改代码另开任务）。
   - [ ] `xmake test` 全绿；framework / development_plan / 本文件同步。
