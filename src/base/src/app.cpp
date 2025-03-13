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

    App::App(::ilias::IoContext *ctx)
        : _ctx(ctx), _commandInvoker(this), _nodeManager(this), _settings("./config.json")
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
            "log [option] show the logs of the program.",
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
        coreInstaller(std::make_unique<CommonCommand>(CommandInvoker::CommandsData{
            {"cofig"},
            "config [option] show the config of the program.",
            [this]([[maybe_unused]] auto args, auto options) -> std::string {
                options.find("dir") != options.end()
                    ? _settings.load(std::get<std::string>(options["dir"]))
                    : _settings.load();
                return "";
            },
            {{"dir", CommandInvoker::OptionsData::eString, "set the config.json dir"}}}));

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

    App::~App()
    {
        _settings.save();
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
        _nodeManager.add_node(MKCommunication::make(*this));
        _nodeManager.add_node(MKCapture::make(*this));
        _nodeManager.add_node(MKSender::make(*this));

        // start all node
        co_await _nodeManager.start_node();
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
        _nodeManager.stop_node();
    }

    auto App::settings() -> Settings &
    {
        return _settings;
    }

    auto App::node_manager() -> NodeManager &
    {
        return _nodeManager;
    }

} // namespace mks::base