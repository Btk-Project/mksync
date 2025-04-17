#include "mksync/core/app.hpp"

#include <spdlog/sinks/callback_sink.h>
#include <spdlog/spdlog.h>
#include <csignal>

#include "mksync/core/communication.hpp"
#include "mksync/core/controller.hpp"
#include "mksync/base/default_configs.hpp"
#include "mksync/core/remote_controller.hpp"

#ifdef _WIN32
    #include <windows.h>
#elif defined(__linux__)
    #include "mksync/core/platform/xcb_window.hpp"
#endif

MKS_BEGIN
using ::ilias::IoTask;
using ::ilias::IPEndpoint;
using ::ilias::Task;
using ::ilias::TcpClient;
using ::ilias::TcpListener;

App::App(::ilias::IoContext *ctx)
    : _ctx(ctx), _commandInvoker(this), _nodeManager(this), _settings("./config.json")
{
    using CallbackType = Task<std::string> (App::*)(const CommandInvoker::ArgsType &,
                                                    const CommandInvoker::OptionsType &);
    auto coreInstaller = command_installer(nullptr);
    // 注册退出程序命令
    coreInstaller(std::make_unique<CommonCommand>(CommandInvoker::CommandsData{
        {"exit", "quit", "q"},
        "exit(quit, q) : exit the program",
        std::bind(static_cast<CallbackType>(&App::stop), this, std::placeholders::_1,
                  std::placeholders::_2),
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
        {"config"},
        "config [option] show the config of the program.",
        [this]([[maybe_unused]]
               auto args,
               auto options) -> ::ilias::Task<std::string> {
            options.find("dir") != options.end()
                ? _settings.load(std::get<std::string>(options["dir"]))
                : true;
            if (options.find("noconsole") != options.end()) {
                _isNoConsole = std::get<bool>(options["noconsole"]);
                if (_isNoConsole) {
                    co_await stop_console();
                }
                else {
                    SPDLOG_ERROR("can't start console in running!");
                }
            }
            co_await reconfigure();
            co_return "";
         },
        {{"dir", CommandInvoker::OptionsData::eString, "set the config.json dir"},
         {"noconsole", CommandInvoker::OptionsData::eBool, "no console input for app"}}
    }));

#if defined(SIGPIPE)
    ::signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号 防止多跑一秒就会爆炸
#endif                          // defined(SIGPIPE)
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

auto App::get_io_context() const -> ::ilias::IoContext *
{
    return _ctx;
}

auto App::get_screen_info() const -> VirtualScreenInfo
{
    std::string screenName =
        _settings.get<std::string>(screen_name_config_name, std::string(screen_name_default_value));
    int screenWidth  = 0;
    int screenHeight = 0;
#ifdef _WIN32
    HWND hd      = GetDesktopWindow();
    int  zoom    = GetDpiForWindow(hd); // 96 is the default DPI
    screenWidth  = GetSystemMetrics(SM_CXSCREEN) * zoom / 96;
    screenHeight = GetSystemMetrics(SM_CYSCREEN) * zoom / 96;
    DWORD dwSize = 255;
    if (wchar_t hostName[255] = {0};
        screenName == "unknow" && GetComputerNameW(hostName, &dwSize) != 0) {
        std::filesystem::path path(hostName);
        screenName = path.string();
    }
#elif defined(__linux__)
    XcbConnect connect(get_io_context());
    if (screenName == "unknow") {
        char name[255] = {0};
        if (auto ret = gethostname(name, 255); ret == 0) {
            screenName = name;
        }
    }
    const auto *display = getenv("DISPLAY");
    if (auto ret = connect.connect(display == nullptr ? ":0" : display).wait(*get_io_context());
        ret) {
        auto defaultWindow = connect.get_default_root_window();
        int  posX;
        int  posY;
        defaultWindow.get_geometry(posX, posY, screenWidth, screenHeight);
    }
#endif
    return VirtualScreenInfo{
        .name      = std::string(screenName),
        .screenId  = 0,
        .width     = (uint32_t)screenWidth,
        .height    = (uint32_t)screenHeight,
        .timestamp = (uint64_t)std::chrono::system_clock::now().time_since_epoch().count()};
}

auto App::command_invoker() -> CommandInvoker &
{
    return _commandInvoker;
}

auto App::command_installer(NodeBase *module) -> std::function<bool(std::unique_ptr<Command>)>
{
    return std::bind(&CommandInvoker::install_cmd, &_commandInvoker, std::placeholders::_1, module);
}
auto App::command_uninstaller(NodeBase *module) -> void
{
    _commandInvoker.remove_cmd(module);
}

auto App::log_handle([[maybe_unused]] const CommandInvoker::ArgsType    &args,
                     [[maybe_unused]] const CommandInvoker::OptionsType &options)
    -> ::ilias::Task<std::string>
{
    if (options.contains("max_log")) {
        int value = std::get<int>(options.at("max_log"));
        if (value > 0) {
            settings().set(max_log_records_config_name, value);
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
        co_return "";
    }
    if (options.contains("level")) {
        const auto &str = std::get<std::string>(options.at("level"));
        settings().set(log_level_config_name, str);
        co_await reconfigure();
        co_return "";
    }
    ::fprintf(stdout, "logs: %d items\n", int(_logList.size()));
    for (const auto &msg : _logList) {
        ::fprintf(stdout, "%s\n", msg.c_str());
    }
    ::fflush(stdout);
    co_return "";
}

auto App::reconfigure() -> ::ilias::Task<void>
{
    const auto &str = settings().get(log_level_config_name, log_level_default_value);
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

    const auto maxLogRecords =
        settings().get(max_log_records_config_name, max_log_records_default_value);
    if (maxLogRecords > 0) {
        _logListMaxSize = maxLogRecords;
    }

    co_await _nodeManager.reconfigure_node();
    co_return;
}

auto App::start_console() -> Task<void>
{ // 监听终端输入
    if (_isNoConsole) {
        co_return;
    }
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
        if (auto ret1 = co_await console->read({strBuffer.get(), 1024}); ret1) {
            auto           *line = reinterpret_cast<char *>(strBuffer.get());
            std::span<char> lineView(line, line + ret1.value());
            while (lineView.size() > 0 && (lineView.back() == '\r' || lineView.back() == '\n')) {
                lineView[lineView.size() - 1] = '\0';
                lineView                      = lineView.subspan(0, lineView.size() - 1);
            }
            co_await _commandInvoker.execute(lineView);
        }
        else {
            SPDLOG_ERROR("Console::read failed {}", ret1.error().message());
            _isConsoleListening = false;
            co_return;
        }
    }
}

auto App::stop_console() -> Task<void>
{
    _isConsoleListening = false;
    co_return;
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
    // 确认是否指定配置文件，如果是就先处理命令行
    if (argc > 2 && (strcmp(argv[1], "config") == 0)) {
        co_await _commandInvoker.execute(std::vector<const char *>(argv + 1, argv + argc));
    }
    // 加载 modules: ["file1.dll", "file2.dll"]
    auto moduleList = _settings.get(module_list_config_name, module_list_default_value);
    for (const auto &moduleFile : moduleList) {
        _nodeManager.load_node(moduleFile);
    }
    // 加载核心模块
    _nodeManager.add_node(
        std::unique_ptr<NodeBase, void (*)(NodeBase *)>(&_commandInvoker, [](NodeBase *) {}));
    auto communication = MKCommunication::make(*this);
    _communication     = communication.get(); // 保存指针
    _nodeManager.add_node(std::move(communication));
    _nodeManager.add_node(Controller::make(*this));
    _nodeManager.add_node({new RemoteController(this), [](NodeBase *ptr) { delete ptr; }});

    co_await reconfigure();

    // start all node
    co_await _nodeManager.setup_node();
    _isRuning = true;
    if (argc > 1) {
        co_await _commandInvoker.execute(std::vector<const char *>(argv + 1, argv + argc));
    }
    while (_isRuning) {
        if (!_isNoConsole && !_isConsoleListening) {
            co_await start_console();
        }
        else {
            _exitEvent.clear();
            co_await _exitEvent;
        }
    }
    co_return;
}

auto App::stop([[maybe_unused]] const CommandInvoker::ArgsType    &args,
               [[maybe_unused]] const CommandInvoker::OptionsType &options)
    -> ::ilias::Task<std::string>
{
    co_await stop();
    co_return "";
}

auto App::stop() -> ilias::Task<void>
{
    _exitEvent.set();
    _isRuning = false;
    co_await stop_console();
    co_await _nodeManager.teardown_node();
}

auto App::settings() -> Settings &
{
    return _settings;
}

auto App::node_manager() -> NodeManager &
{
    return _nodeManager;
}

auto App::communication() -> ICommunication *
{
    ILIAS_ASSERT(_communication != nullptr);
    return _communication->get_communication();
}

[[nodiscard("coroutine function")]]
auto App::push_event(NekoProto::IProto &&event) -> ::ilias::Task<::ilias::Error>
{
    co_return co_await _nodeManager.get_events().push(
        {nullptr, std::forward<NekoProto::IProto>(event)});
}

[[nodiscard("coroutine function")]]
auto App::push_event(NekoProto::IProto &&event, NodeBase *node) -> ::ilias::Task<::ilias::Error>
{
    co_return co_await _nodeManager.get_events().push(
        {node, std::forward<NekoProto::IProto>(event)});
}
MKS_END