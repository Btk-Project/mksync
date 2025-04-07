*职责 (Responsibility):*

IApp 是应用程序的主接口，负责初始化系统资源、启动核心服务（如命令处理、节点管理）、协调主要流程以及处理应用的生命周期（启动、停止）,提供各模块访问软件主要模块和功能的上下文。

*关键属性/方法 (Key Attributes/Methods):*

`auto  app_name() -> const char *`

    获取应用程序的名称。

`auto  app_version() -> const char *`

    获取应用程序的版本。

`auto get_io_context() const -> ::ilias::IoContext *`

    获取ilias的协程 I/O 上下文。

`auto get_screen_info() const -> VirtualScreenInfo`

    获取虚拟屏幕配置信息

`auto settings() -> Settings &`

    获取应用程序的配置管理器。

`auto node_manager() -> NodeManager &`

    获取节点管理器实例。

`auto communication() -> ICommunication *`

    获取通信接口实例。

`auto command_invoker() -> CommandInvoker &`

    获取命令调用器实例。

`auto command_installer(NodeBase *module) -> std::function<bool(std::unique_ptr<Command>)>`

    安装命令模块，返回一个函数，用于创建命令实例。

`auto command_uninstaller(NodeBase *module) -> void`

    卸载命令模块。

`auto push_event(NekoProto::IProto &&event) -> ::ilias::Task<::ilias::Error>`

    推送事件到事件循环。

`auto push_event(NekoProto::IProto &&event, NodeBase *node) -> ::ilias::Task<::ilias::Error>`

    推送事件到指定节点的事件循环。

`auto exec(int argc = 0, const char *const *argv = nullptr) -> ilias::Task<void>`

    执行应用程序，处理命令行参数。

`auto stop() -> ilias::Task<void>`

    停止应用程序。


关系 (Relationships): 它与其他哪些类交互？如何交互？（参考 Mermaid 图）
你需要填充: (例如：IApp 使用 CommandInvoker 来执行命令，使用 NodeManager 来管理节点，使用 Settings 来获取配置。)
实现说明 (Implementation Notes - 可选): 如果有特别的设计决策、注意事项或具体实现类的例子，可以在这里说明。
你需要填充: (例如：是否有 App 类实现了 IApp 接口？它如何初始化 CommandInvoker 和 NodeManager？)