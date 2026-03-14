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

### 2.4 `finally(...)`
用户明确说明：
- `ilias::finally(task, callback)` 中的 callback **不会收到取消信号**
- 因此 callback 里通常 **不需要再包 `ilias::unstoppable(...)`**
- 即使包了通常也没坏处，但语义上是冗余的

推荐心智模型：
- 主任务可被取消
- `finally` 的异步清理回调是 guaranteed cleanup path
- 适合做 async shutdown / unhook / close / flush

这意味着以后写生命周期时可以优先考虑：
```cpp
co_return co_await ilias::finally(
    mainTask(),
    [this]() -> ilias::Task<void> {
        co_await shutdown();
    }
);
```

### 2.5 `mpsc`
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

### 2.6 `Latch`
`ilias::Latch` 语义接近 `std::latch`：
- `countDown()`
- `wait()`（协程等待）
- `blockingWait()`（线程阻塞等待）
- `arriveAndWait()`

当前项目中适合用于：
- UI 线程安装/卸载 hook 的同步点
- 平台线程初始化完成通知

### 2.7 `TcpStream / TcpListener / BufStream`
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
- `finally`

避免：
- 无归属的 detached 任务
- 无法追踪生命周期的后台协程

### 规则 2：async 收尾优先考虑 `finally`
典型场景：
- Host / Client run loop 退出时 cleanup
- unhook
- close socket
- flush / final notify

如果 cleanup 本身是 async，优先 `finally`；
如果 cleanup 只是同步释放，直接用 RAII guard 即可。

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

### Host / Client 生命周期
建议写法：
- `initialize()`
- `finally(eventLoop(), shutdown())`
- 最外层 `whenAny(run, ctrlC())`

这样和 ilias 当前的结构化并发语义最匹配。

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
- `finally` 是 async cleanup 首选
- `unstoppable` 更适合 latch / 特定不可中断等待点，而不是所有 finally callback 都包一层
- `mpsc::trySend` 适合高敏感同步回调线程
- mksync 现在应按 **0.3.4 实际头文件** 开发，不要默认按 `D:\CodeProject\Ilias` 的 0.4 API 写

## 7. 当前 mksync 项目里额外确认过的实践结论

### 7.1 Host / Client 联机约束
- 直接运行 `mksync.exe` 在当前环境里可能因为 `ilias.dll` 不在 PATH 而失败
- 本项目做运行验证时，应优先使用：
  - `xmake run mksync`
  - `xmake run mksync -- client 127.0.0.1:24800`

### 7.2 单个 `BufStream` 的写侧应串行化
- 不要在一个连接上让多个协程直接并发 `writeFrame(...)`
- 更稳妥的方式是：
  - 一个读循环负责解析协议
  - 一个写循环独占 socket 写入
  - 其他逻辑通过 `mpsc::Sender<Frame>` 投递待发送帧
- 这对 `Ping/Pong`、输入事件转发、握手后的控制帧都适用

### 7.3 本地双开测试时要过滤 injected 事件
- Windows 低级 Hook 能识别 `LLMHF_INJECTED / LLKHF_INJECTED`
- Host 在采集链路里应忽略 `event.metadata.injected == true` 的事件
- 否则本机 Host + Client 双开时，Client 注入出来的事件可能再次被 Host 捕获并回传，形成反馈环

### 7.4 `whenAny` + `finally` 的会话收尾模型可直接用
- 会话里让 `readLoop` / `writeLoop` 用 `whenAny(...)` 竞争是安全的
- 按用户确认的 0.3.4 语义，返回时其余任务也已经结束
- 再配合 `finally(..., cleanup)` 做 sender 清理 / topology 回滚，模型是成立的

### 7.5 `BufStream::writeAll(...)` 后别忘了 `flush()`
- 0.3.4 的 `BufStream` 内部是 `BufReader<BufWriter<T>>`
- `write()/writeAll()` 对小包可能只写入 `BufWriter` 的内存缓冲，不会立即到 socket
- 对协议帧这种小消息，如果写完马上期待对端读取，必须显式 `co_await stream.flush()`
- 否则就会出现：TCP 已连接，但握手帧迟迟不到，对端 `readFrame()` 卡住
