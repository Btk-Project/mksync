#include "mksync/base/app.hpp"

#include <spdlog/sinks/callback_sink.h>
#include <spdlog/spdlog.h>
#include <csignal>

#include "mksync/base/communication.hpp"

namespace mks::base
{
    using ::ilias::IoTask;
    using ::ilias::IPEndpoint;
    using ::ilias::Task;
    using ::ilias::TcpClient;
    using ::ilias::TcpListener;

    App::App(::ilias::IoContext *ctx) : _ctx(ctx), _commandInvoker(this)
    {
        using CallbackType = std::string (App::*)(const CommandInvoker::ArgsType &,
                                                  const CommandInvoker::OptionsType &);
        auto coreInstaller = command_installer("Core");
        // 注册退出程序命令
        coreInstaller(std::make_unique<CommonCommand>(CommandInvoker::CommandsData{
            {"exit", "quit", "q"},
            "exit the program",
            std::bind(
                static_cast<std::string (App::*)(const CommandInvoker::ArgsType &,
                                                 const CommandInvoker::OptionsType &)>(&App::stop),
                this, std::placeholders::_1, std::placeholders::_2),
            {}
        }));
        coreInstaller(std::make_unique<CommonCommand>(CommandInvoker::CommandsData{
            {"log"},
            "log [command] [option] show the logs of the program.",
            std::bind(static_cast<CallbackType>(&App::log_handle), this, std::placeholders::_1,
                      std::placeholders::_2),
            {
             {"clear", CommandInvoker::OptionsData::eBool, "clear the log"},
             {"max_log", CommandInvoker::OptionsData::eInt,
                 "set the max log number, default is 100."},
             {"level", CommandInvoker::OptionsData::eString,
                 "set print log level. vlaue: trace/debug/info/warn/err"},
             }
        }));

#if defined(SIGPIPE)
        ::signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号 防止多跑一秒就会爆炸
#endif                              // defined(SIGPIPE)
        auto sink = std::make_shared<spdlog::sinks::callback_sink_st>([this](const auto &msg) {
            if (_logList.size() == _logListMaxSize) {
                _logList.pop_front();
            }
            auto &payload = msg.payload;
            _logList.emplace_back(std::string(payload.data(), payload.size()));
        });
        spdlog::default_logger()->sinks().push_back(sink);
    }

    App::~App() {}

    auto App::install_node(std::unique_ptr<NodeBase, void (*)(NodeBase *)> &&node) -> void
    {
        if (node == nullptr) {
            return;
        }
        const auto *name = node->name();
        SPDLOG_INFO("install node: {}<{}>", name, (void *)node.get());
        _nodeList.emplace_back(NodeData{std::move(node), NodeStatus::eNodeStatusStopped});
    }

    auto App::start_node(NodeData &node) -> ilias::Task<int>
    {
        SPDLOG_INFO("start node: {}<{}>", node.node->name(), (void *)node.node.get());
        if (node.status == NodeStatus::eNodeStatusStopped) {
            auto ret = co_await node.node->start();
            if (ret != 0) {
                co_return ret;
            }
            node.status    = NodeStatus::eNodeStatusRunning;
            auto *consumer = dynamic_cast<Consumer *>(node.node.get());
            if (consumer != nullptr) {
                for (int type : consumer->get_subscribers()) {
                    _consumerMap[type].push_back(consumer);
                }
            }
            auto *producer = dynamic_cast<Producer *>(node.node.get());
            if (producer != nullptr) {
                _cancelHandleMap[node.node->name()] =
                    ::ilias::spawn(*_ctx, producer_loop(producer));
            }
            co_return 0;
        }
        co_return -2;
    }

    auto App::producer_loop(Producer *producer) -> ::ilias::Task<void>
    {
        while (true) {
            if (auto ret = co_await producer->get_event(); ret) {
                co_await dispatch(std::move(ret.value()), dynamic_cast<NodeBase *>(producer));
            }
            else {
                break;
            }
        }
        co_return;
    }

    auto App::stop_node(NodeData &node) -> ilias::Task<int>
    {
        if (node.status == NodeStatus::eNodeStatusRunning) {
            node.status    = NodeStatus::eNodeStatusStopped;
            auto *consumer = dynamic_cast<Consumer *>(node.node.get());
            if (consumer != nullptr) {
                for (int type : consumer->get_subscribers()) {
                    _consumerMap[type].remove(consumer);
                }
            }
            auto *producer = dynamic_cast<Producer *>(node.node.get());
            if (producer != nullptr) {
                auto handle = _cancelHandleMap.find(node.node->name());
                if (handle != _cancelHandleMap.end() && handle->second) {
                    handle->second.cancel();
                }
                _cancelHandleMap.erase(handle);
            }
            co_return co_await node.node->stop();
        }
        co_return 0;
    }

    auto App::dispatch(NekoProto::IProto &&proto, NodeBase *nodebase) -> Task<void>
    {
        int type = proto.type();
        for (auto *consumer : _consumerMap[type]) {
            if (proto == nullptr) {
                break;
            }
            if (dynamic_cast<NodeBase *>(consumer) == nodebase) {
                continue;
            }
            SPDLOG_INFO("dispatch {} to {}", proto.protoName(),
                        dynamic_cast<NodeBase *>(consumer)->name());
            co_await consumer->handle_event(proto);
        }
        co_return;
    }

    auto App::app_name() -> const char *
    {
        return "mksync";
    }

    auto App::app_version() -> const char *
    {
        return "0.0.1";
    }

    auto App::get_io_context() const -> ::ilias::IoContext *
    {
        return _ctx;
    }

    auto App::command_installer(std::string_view module)
        -> std::function<bool(std::unique_ptr<Command>)>
    {
        return std::bind(&CommandInvoker::install_cmd, &_commandInvoker, std::placeholders::_1,
                         module);
    }

    auto App::log_handle([[maybe_unused]] const CommandInvoker::ArgsType    &args,
                         [[maybe_unused]] const CommandInvoker::OptionsType &options) -> std::string
    {
        if (options.contains("max_log")) {
            int value = std::get<int>(options.at("max_log"));
            if (value > 0) {
                _logListMaxSize = value;
                while ((int)_logList.size() > value) {
                    _logList.pop_front();
                }
            }
        }
        if (options.contains("clear")) {
            if (std::get<bool>(options.at("clear"))) {
                _logList.clear();
            }
            return "";
        }
        if (options.contains("level")) {
            const auto &str = std::get<std::string>(options.at("level"));
            if (str == "trace") {
                spdlog::set_level(spdlog::level::trace);
            }
            else if (str == "debug") {
                spdlog::set_level(spdlog::level::debug);
            }
            else if (str == "info") {
                spdlog::set_level(spdlog::level::info);
            }
            else if (str == "warn") {
                spdlog::set_level(spdlog::level::warn);
            }
            else if (str == "err") {
                spdlog::set_level(spdlog::level::err);
            }
            else if (str == "critical") {
                spdlog::set_level(spdlog::level::critical);
            }
            return "";
        }
        ::fprintf(stdout, "logs: %d items\n", int(_logList.size()));
        for (const auto &msg : _logList) {
            ::fprintf(stdout, "%s\n", msg.c_str());
        }
        ::fflush(stdout);
        return "";
    }

    auto App::start_console() -> Task<void>
    {                              // 监听终端输入
        if (_isConsoleListening) { // 确保没有正在监听中。
            SPDLOG_ERROR("console is already listening");
            co_return;
        }
        auto console = co_await ilias::Console::fromStdin();
        if (!console) {
            SPDLOG_ERROR("Console::fromStdin failed {}", console.error().message());
            co_return;
        }
        _isConsoleListening                    = true;
        std::unique_ptr<std::byte[]> strBuffer = std::make_unique<std::byte[]>(1024);
        while (_isConsoleListening) {
            memset(strBuffer.get(), 0, 1024);
            printf("%s >>> ", App::app_name());
            fflush(stdout);
            auto ret1 = co_await console->read({strBuffer.get(), 1024});
            if (!ret1) {
                SPDLOG_ERROR("Console::read failed {}", ret1.error().message());
                stop_console();
                co_return;
            }
            auto           *line = reinterpret_cast<char *>(strBuffer.get());
            std::span<char> lineView(line, line + ret1.value());
            while (lineView.size() > 0 && (lineView.back() == '\r' || lineView.back() == '\n')) {
                lineView[lineView.size() - 1] = '\0';
                lineView                      = lineView.subspan(0, lineView.size() - 1);
            }
            co_await _commandInvoker.execute(lineView);
        }
    }

    auto App::stop_console() -> void
    {
        _isConsoleListening = false;
    }

    /**
     * @brief
     * server:
     * 1. start server(启动网络监听)
     * 2. start capture 启动键鼠捕获
     * client
     * 1. connect to server 连接服务端
     * @return Task<void>
     */
    auto App::exec(int argc, const char *const *argv) -> Task<void>
    {
        install_node(MKCommunication::make(*this));
        install_node(MKCapture::make(*this));
        install_node(MKSender::make(*this));

        // start all node
        for (auto &node : _nodeList) {
            auto ret = co_await start_node(node);
            if (ret != 0) {
                SPDLOG_ERROR("start node {} failed", node.node->name());
            }
        }
        _isRuning = true;
        if (argc > 1) {
            _commandInvoker.execute(std::vector<const char *>(argv + 1, argv + argc));
        }
        while (_isRuning) {
            co_await start_console();
        }
        co_return;
    }

    auto App::stop([[maybe_unused]] const CommandInvoker::ArgsType    &args,
                   [[maybe_unused]] const CommandInvoker::OptionsType &options) -> std::string
    {
        stop();
        return "";
    }

    auto App::stop() -> void
    {
        _isRuning = false;
        stop_console();
        for (auto &handle : _cancelHandleMap) {
            if (handle.second) {
                handle.second.cancel();
            }
        }
        _cancelHandleMap.clear();
    }
} // namespace mks::base