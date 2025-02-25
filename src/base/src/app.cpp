#include "mksync/base/app.hpp"

#include <spdlog/sinks/callback_sink.h>
#include <spdlog/spdlog.h>
#include <csignal>

namespace mks::base
{
    template <typename T>
    using Task = ::ilias::Task<T>;
    template <typename T>
    using IoTask      = ::ilias::IoTask<T>;
    using IPEndpoint  = ::ilias::IPEndpoint;
    using TcpClient   = ::ilias::TcpClient;
    using TcpListener = ::ilias::TcpListener;

    App::App(::ilias::IoContext *ctx) : _ctx(ctx), _commandParser(this)
    {
        using CallbackType   = std::string (App::*)(const CommandParser::ArgsType &,
                                                  const CommandParser::OptionsType &);
        auto serverInstaller = command_installer("Server");
        // 注册启动服务命令
        serverInstaller({
            {"start", "s"},
            "start server, e.g. start 127.0.0.1 12345",
            std::bind(static_cast<CallbackType>(&App::start_server), this, std::placeholders::_1,
                      std::placeholders::_2)
        });
        serverInstaller({{"stopserver"},
                         "stop the server",
                         std::bind(static_cast<CallbackType>(&App::stop_server), this,
                                   std::placeholders::_1, std::placeholders::_2)});
        // 注册开始捕获键鼠事件命令
        serverInstaller({
            {"capture", "c"},
            "start keyboard/mouse capture",
            std::bind(static_cast<CallbackType>(&App::start_capture), this, std::placeholders::_1,
                      std::placeholders::_2)
        });
        serverInstaller({{"stopcapture"},
                         "stop keyboard/mouse capture",
                         std::bind(static_cast<CallbackType>(&App::stop_capture), this,
                                   std::placeholders::_1, std::placeholders::_2)});
        // 注册退出程序命令
        _commandParser.install_cmd(
            {
                {"exit", "quit", "q"},
                "exit the program",
                std::bind(static_cast<std::string (App::*)(const CommandParser::ArgsType &,
                                                           const CommandParser::OptionsType &)>(
                              &App::stop),
                          this, std::placeholders::_1, std::placeholders::_2)
        },
            "Core");
        auto clientInstaller = command_installer("Client");
        // 注册连接到服务器命令
        clientInstaller({{"connect"},
                         "connect to server, e.g. connect 127.0.0.1 12345",
                         std::bind(static_cast<CallbackType>(&App::connect_to), this,
                                   std::placeholders::_1, std::placeholders::_2)});
        clientInstaller({
            {"disconnect", "dcon"},
            "disconnect from server",
            std::bind(static_cast<CallbackType>(&App::disconnect), this, std::placeholders::_1,
                      std::placeholders::_2)
        });
        auto communicationInstaller = command_installer("Communication");
        communicationInstaller({
            {"thread", "t"},
            "<enable/disable> to enable independent thread for serialization",
            std::bind(static_cast<CallbackType>(&App::set_communication_options), this,
                      std::placeholders::_1, std::placeholders::_2)
        });
        auto statusInstaller = command_installer("Status");
        statusInstaller({
            {"status", "st"},
            "show the status of the program",
            std::bind(static_cast<CallbackType>(&App::show_status), this, std::placeholders::_1,
                      std::placeholders::_2)
        });

#if defined(SIGPIPE)
        ::signal(SIGPIPE, SIG_IGN); // 忽略SIGPIPE信号 防止多跑一秒就会爆炸
#endif // defined(SIGPIPE)
        auto sink = std::make_shared<spdlog::sinks::callback_sink_st>([this](const auto &msg) {
            if (_statusList.size() == _statusListMaxSize) {
                _statusList.pop_front();
            }
            auto &payload = msg.payload;
            _statusList.emplace_back(std::string(payload.data(), payload.size()));
        });
        spdlog::default_logger()->sinks().push_back(sink);

    }

    App::~App() {}

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
        -> std::function<bool(CommandParser::CommandsData &&)>
    {
        return std::bind(
            static_cast<bool (CommandParser::*)(CommandParser::CommandsData &&, std::string_view)>(
                &CommandParser::install_cmd),
            &_commandParser, std::placeholders::_1, module);
    }

    auto App::show_status([[maybe_unused]] const CommandParser::ArgsType    &args,
                          [[maybe_unused]] const CommandParser::OptionsType &options) -> std::string
    {
        ::fprintf(stdout, "status: %d items\n", int(_statusList.size()));
        for (const auto &msg : _statusList) {
            ::fprintf(stdout, "%s\n", msg.c_str());
        }
        ::fflush(stdout);
        return "";
    }
    auto App::set_communication_options(const CommandParser::ArgsType                     &args,
                                        [[maybe_unused]] const CommandParser::OptionsType &options)
        -> std::string
    {
        if (args.empty()) {
            return "please usage: thread <enabled/disabled>";
        }
        if (args[0] == "enabled") {
            set_option("communication-enable-thread", true);
            _streamflags = _streamflags | NekoProto::StreamFlag::SerializerInThread;
        }
        else {
            set_option("communication-enable-thread", false);
            _streamflags = (NekoProto::StreamFlag)(
                (uint32_t)_streamflags & (~(uint32_t)NekoProto::StreamFlag::SerializerInThread));
        }
        return "";
    }

    auto App::connect_to(IPEndpoint endpoint) -> Task<void>
    {
        if (_isServering) { // 确保没有启动服务模式
            spdlog::error("please quit server mode first");
            co_return;
        }
        if (_eventSender) { // 确保当前没有正在作为客户端运行
            co_return;
        }
        _eventSender = MKSender::make(*this);
        if (!_eventSender) {
            spdlog::error("MKSender make failed, this platform may not support in this feature");
            co_return;
        }
        if (auto res = co_await _eventSender->start(); res != 0) {
            spdlog::error("MKSender start failed with {}", res);
            co_return;
        }
        auto ret = co_await TcpClient::make(endpoint.family());
        if (!ret) {
            spdlog::error("TcpClient::make failed {}", ret.error().message());
            co_return;
        }
        auto tcpClient = std::move(ret.value());
        _isClienting   = true;
        if (auto res = co_await tcpClient.connect(endpoint); !res) {
            spdlog::error("TcpClient::connect failed {}", res.error().message());
            co_return;
        }
        _protoStreamClient = // 构造用于传输协议的壳
            std::make_unique<NekoProto::ProtoStreamClient<>>(_protofactory, std::move(tcpClient));
        _isClienting = true;
        while (_isClienting) {
            auto res = co_await _protoStreamClient->recv(_streamflags);
            if (!res) {
                spdlog::error("ProtoStreamClient::recv failed {}", res.error().message());
                disconnect();
                co_return;
            }
            auto msg = std::move(res.value());
            if (_eventSender) {
                if (msg.type() == NekoProto::ProtoFactory::protoType<mks::KeyEvent>()) {
                    _eventSender->send_keyboard_event(*msg.cast<mks::KeyEvent>());
                }
                else if (msg.type() ==
                         NekoProto::ProtoFactory::protoType<mks::MouseButtonEvent>()) {
                    _eventSender->send_button_event(*msg.cast<mks::MouseButtonEvent>());
                }
                else if (msg.type() ==
                         NekoProto::ProtoFactory::protoType<mks::MouseMotionEvent>()) {
                    _eventSender->send_motion_event(*msg.cast<mks::MouseMotionEvent>());
                }
                else if (msg.type() == NekoProto::ProtoFactory::protoType<mks::MouseWheelEvent>()) {
                    _eventSender->send_wheel_event(*msg.cast<mks::MouseWheelEvent>());
                }
                else {
                    spdlog::error("unused message type {}", msg.protoName());
                }
            }
        }
        co_return;
    }

    auto App::connect_to([[maybe_unused]] const CommandParser::ArgsType    &args,
                         [[maybe_unused]] const CommandParser::OptionsType &options) -> std::string
    {
        if (args.size() == 2) {
            ::ilias::spawn(*_ctx, connect_to(IPEndpoint(args[0] + ":" + args[1])));
        }
        else if (args.size() == 1) {
            ::ilias::spawn(*_ctx, connect_to(IPEndpoint(args[0])));
        }
        else {
            auto it      = options.find("address");
            auto address = it == options.end() ? "127.0.0.1" : it->second;
            it           = options.find("port");
            auto port    = it == options.end() ? "12345" : it->second;
            ::ilias::spawn(*_ctx, connect_to(IPEndpoint(address + ":" + port)));
        }
        return "";
    }

    auto App::disconnect([[maybe_unused]] const CommandParser::ArgsType    &args,
                         [[maybe_unused]] const CommandParser::OptionsType &options) -> std::string
    {
        disconnect();
        return "";
    }

    auto App::disconnect() -> void
    {
        _isClienting = false;
        _eventSender.reset();
        if (_protoStreamClient) {
            _protoStreamClient->close().wait();
        }
        _eventSender.reset();
    }

    auto App::start_server(const CommandParser::ArgsType    &args,
                           const CommandParser::OptionsType &options) -> std::string
    {
        IPEndpoint ipendpoint("127.0.0.1:12345");
        if (args.size() == 2) {
            if (auto ret = IPEndpoint::fromString(args[0] + ":" + args[1]); ret) {
                ipendpoint = ret.value();
            }
            else {
                spdlog::error("ip endpoint error {}", ret.error().message());
            }
        }
        else if (args.size() == 1) {
            if (auto ret = IPEndpoint::fromString(args[0]); ret) {
                ipendpoint = ret.value();
            }
            else {
                spdlog::error("ip endpoint error {}", ret.error().message());
            }
        }
        else {
            auto it      = options.find("address");
            auto address = it == options.end() ? "127.0.0.1" : it->second;
            it           = options.find("port");
            auto port    = it == options.end() ? "12345" : it->second;
            auto ret     = IPEndpoint::fromString(address + ":" + port);
            if (ret) {
                ipendpoint = ret.value();
            }
            else {
                spdlog::error("ip endpoint error {}", ret.error().message());
            }
        }
        ::ilias::spawn(*_ctx, start_server(ipendpoint));
        return "";
    }

    auto App::start_server(IPEndpoint endpoint) -> Task<void>
    {
        if (_isClienting) { // 确保没有作为客户端模式运行
            spdlog::error("please quit client mode first");
            co_return;
        }
        if (_isServering) { // 确保没有正在作为服务端运行
            spdlog::error("server is already running");
            co_return;
        }
        auto ret = co_await TcpListener::make(endpoint.family());
        if (!ret) {
            spdlog::error("TcpListener::make failed {}", ret.error().message());
            co_return;
        }
        spdlog::info("start server with {}", endpoint.toString());
        _server   = std::move(ret.value());
        if (auto res = _server.bind(endpoint); !res) {
            spdlog::error("TcpListener::bind failed {}", res.error().message());
        }
        _isServering = true;
        while (_isServering) {
            auto ret1 = co_await _server.accept();
            if (!ret1) {
                spdlog::error("TcpListener::accept failed {}", ret1.error().message());
                stop_server();
                co_return;
            }
            auto tcpClient = std::move(ret1.value());
            ::ilias::spawn(*_ctx, _accept_client(std::move(tcpClient.first)));
        }
    }

    auto App::stop_server([[maybe_unused]] const CommandParser::ArgsType    &args,
                          [[maybe_unused]] const CommandParser::OptionsType &options) -> std::string
    {
        stop_server();
        return "";
    }

    auto App::stop_server() -> void
    {
        _isServering = false;
        _server.close();
    }

    auto App::start_console() -> Task<void>
    {                              // 监听终端输入
        if (_isConsoleListening) { // 确保没有正在监听中。
            spdlog::error("console is already listening");
            co_return;
        }
        auto console = co_await ilias::Console::fromStdin();
        if (!console) {
            spdlog::error("Console::fromStdin failed {}", console.error().message());
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
                spdlog::error("Console::read failed {}", ret1.error().message());
                stop_console();
                co_return;
            }
            auto            *line = reinterpret_cast<char *>(strBuffer.get());
            std::string_view lineView(line);
            while (lineView.size() > 0 && (lineView.back() == '\r' || lineView.back() == '\n')) {
                lineView.remove_suffix(1);
            }
            if (auto res = _commandParser.parser(lineView); !res.empty()) {
                spdlog::warn("exec {} warn: {}", lineView, res.c_str());
            }
        }
    }

    auto App::stop_console() -> void
    {
        _isConsoleListening = false;
    }

    auto App::_accept_client(TcpClient client) -> Task<void>
    { // 服务端处理客户连接
        spdlog::debug("accept client {}", client.remoteEndpoint()->toString());
        // do some thing for client
        _protoStreamClient =
            std::make_unique<NekoProto::ProtoStreamClient<>>(_protofactory, std::move(client));
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
        _isRuning = true;
        if (argc > 1) {
            _commandParser.parser(std::vector<std::string_view>(argv + 1, argv + argc));
        }
        while (_isRuning) {
            co_await start_console();
        }
        co_return;
    }

    auto App::stop([[maybe_unused]] const CommandParser::ArgsType    &args,
                   [[maybe_unused]] const CommandParser::OptionsType &options) -> std::string
    {
        stop();
        return "";
    }

    auto App::stop() -> void
    {
        _isRuning = false;
        stop_capture();
        disconnect();
        stop_server();
        stop_console();
    }

    auto App::start_capture() -> Task<void>
    {
        if (_isClienting || !_isServering || !_isRuning) { // 确保是作为服务端运行中
            spdlog::error("please start capture in server mode");
            co_return;
        }
        spdlog::debug("start_capture");
        if (_listener && _isCapturing) { // 已经启动了
            co_return;
        }
        _isCapturing = true;
        _listener    = MKCapture::make(*this);
        if (_listener == nullptr) {
            spdlog::error("MKCapture make failed, this platform may not support in this feature");
            co_return;
        }
        auto ret = co_await _listener->start();
        if (ret != 0) {
            spdlog::error("MKCapture start failed with {}", ret);
            co_return;
        }
        while (_isCapturing) {
            NekoProto::IProto proto = (co_await _listener->get_event());
            if (proto == nullptr) {
                spdlog::error("get_event failed");
                stop_capture();
                co_return;
            }
            if (_protoStreamClient) {
                if (auto res = co_await _protoStreamClient->send(proto, _streamflags); !res) {
                    spdlog::error("send proto failed {}", res.error().message());
                    stop_capture();
                    co_return;
                }
            }
        }
    }

    auto App::start_capture([[maybe_unused]] const CommandParser::ArgsType    &args,
                            [[maybe_unused]] const CommandParser::OptionsType &options)
        -> std::string
    {
        ::ilias::spawn(*_ctx, start_capture());
        return "";
    }

    auto App::stop_capture() -> void
    {
        _isCapturing = false;
        if (_listener) {
            _listener->stop().wait();
            _listener->notify();
        }
        _listener.reset();
    }

    auto App::stop_capture([[maybe_unused]] const CommandParser::ArgsType    &args,
                           [[maybe_unused]] const CommandParser::OptionsType &options)
        -> std::string
    {
        stop_capture();
        return "";
    }
} // namespace mks::base