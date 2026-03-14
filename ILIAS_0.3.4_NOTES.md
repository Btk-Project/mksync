# ilias 0.3.4 使用笔记（mksync 本地知识库）

本文件用于给后续 Codex / 新上下文快速载入当前项目依赖的 `ilias` 语义与使用约定。

## 1. 版本与路径

### mksync 当前实际依赖版本
- `ilias 0.3.4`
- 头文件缓存路径：
  - `C:\Users\HP\AppData\Local\.xmake\packages\i\ilias\0.3.4\431f8be79d244d08affd0a31c31cec13\include\ilias`

### 参考源码（更新版本）
- 本地源码仓库：`D:\CodeProject\Ilias`
- 注意：这里是 **0.4 开发线**，接口可能和 mksync 当前使用的 0.3.4 不完全一致。
- 只能作为语义参考，不能默认按 0.4 API 直接改 mksync。

---

## 2. 本项目里已确认的关键语义

### 2.1 `whenAny`
用户明确说明：

- `whenAny` 会自动取消其他任务
- 并且会等待其他任务真正完成后再返回
- 即它符合结构化并发语义

例子：
```cpp
auto [a, b] = co_await whenAny(taskA(), taskB());
```
当 `whenAny` 返回时：
- `taskA()` 和 `taskB()` 都已经完成
- 未完成的那个会被取消并收尾完成

这点非常重要，后续设计不能把它当成“只拿到第一个结果，其他仍在后台跑”的语义。

### 2.2 `TaskScope`
从头文件可读出：
- `TaskScope` 是结构化生命周期容器
- 析构前必须保证任务都结束，否则会触发 abort
- 推荐通过 `TaskScope::enter(...)` 使用
- `shutdown()` = `stop()` + `waitAll()`

因此在 mksync 中：
- 连接处理、会话任务、后台 watcher 都应尽量挂到 `TaskScope`
- 不要写“裸后台协程”然后忘记回收

### 2.3 `unstoppable(...)`
作用：
- 在不可中断上下文中等待某个 awaitable 完成
- 常用于收尾逻辑、latch 等待、shutdown 路径

当前项目已经这样使用：
- UI 线程 hook/unhook 后，等待 `Latch`
- 避免外部取消把资源释放过程打断

### 2.4 `mpsc`
已确认存在：
- `ilias::mpsc::channel<T>(capacity)`
- `Sender<T>::send(...)`
- `Sender<T>::trySend(...)`
- `Sender<T>::blockingSend(...)`
- `Receiver<T>::recv()`
- `Receiver<T>::blockingRecv()`

重要语义：
- `blockingSend()` 会阻塞线程，不适合用在 async 上下文里
- 但它可用于同步代码
- `trySend()` 满时返回错误，不阻塞

因此对于 Windows Hook 回调这类系统敏感路径：
- **优先 `trySend()`，避免阻塞输入链路**
- 除非业务明确接受 hook 回调阻塞

### 2.5 `Latch`
`ilias::Latch` 语义接近 `std::latch`：
- `countDown()`
- `wait()`（协程等待）
- `blockingWait()`（线程阻塞等待）
- `arriveAndWait()`

当前项目中适合用于：
- UI 线程安装/卸载 hook 的同步点
- 平台线程初始化完成通知

### 2.6 `TcpStream / TcpListener / BufStream`
已确认可用：
- `TcpListener::bind(endpoint)`
- `listener.accept()`
- `TcpStream::connect(endpoint)`
- `tcp.remoteEndpoint()`
- `ilias::BufStream { std::move(tcp) }`

说明当前 transport 层完全可以基于这套 API 自己封装。

---

## 3. 现阶段建议遵守的 ilias 使用规则

### 规则 1：默认使用结构化并发
优先使用：
- `whenAll`
- `whenAny`
- `TaskScope::enter`
- `TaskScope::spawn`

避免：
- 无归属的 detached 任务
- 无法追踪生命周期的后台协程

### 规则 2：收尾逻辑尽量包 `unstoppable`
典型场景：
- shutdown
- unhook
- close socket
- flush / final notify

### 规则 3：系统回调线程不要做可能阻塞的操作
例如：
- Windows hook 回调
- GUI 消息线程
- 未来 X11 event callback

建议只做：
- 轻量翻译
- `trySend`
- 原子计数
- 投递到业务线程

### 规则 4：`IoTask` 错误统一向上返回，不要吞
当前项目已经使用：
- `Err(error_code)`
- `if (!res) { ... }`

应继续保持这个风格。

---

## 4. 对 mksync 的直接影响

### 输入采集链路
- hook 回调 -> `trySend()` -> async consumer
- 不要在 hook 回调里 `co_await`
- 不要在 hook 回调里执行复杂日志/网络/锁竞争

### 网络会话层
- 一个 listener 对应一个 `TaskScope`
- 每个连接 `scope.spawn(networkHandle(...))`
- 断开或 stop 时通过 scope 收尾

### Host / Client 角色
后续如果做：
- `HostApp::run()`
- `ClientApp::run()`

建议都围绕 `whenAny(mainLoop, ctrlC())` 或 `TaskScope::enter(...)` 组织，保持结构化退出。

---

## 5. 需要额外确认时再问用户的点

虽然 0.3.4 头文件大多可直接阅读，但以下语义如果未来代码高度依赖，最好再次确认：

- `whenAll` 在某个子任务失败/取消时的聚合行为
- `TaskScope::spawn` 的异常传播策略
- `IoTask<T>` 与 `Task<T>` 在取消传播上的差异
- socket cancel / close 的平台差异
- 0.3.4 与本地 0.4 在行为语义上是否有刻意变更

---

## 6. 快速结论

后续在 mksync 里可默认采用下面这套心智模型：

- `ilias` 是结构化并发优先的协程库
- `whenAny` 返回时相关任务都已经结束
- `TaskScope` 是任务生命周期边界
- `unstoppable` 适合收尾路径
- `mpsc::trySend` 适合高敏感同步回调线程
- mksync 现在应按 **0.3.4 实际头文件** 开发，不要默认按 `D:\CodeProject\Ilias` 的 0.4 API 写
