#include "app.hpp"

#include <spdlog/spdlog.h>

template <typename T>
using Task = ILIAS_NAMESPACE::Task<T>;
template <typename T>
using IoTask      = ILIAS_NAMESPACE::IoTask<T>;
using IPEndpoint  = ILIAS_NAMESPACE::IPEndpoint;
using TcpClient   = ILIAS_NAMESPACE::TcpClient;
using TcpListener = ILIAS_NAMESPACE::TcpListener;

App::App() : _commandParser(this)
{
    // 注册启动服务命令
    _commandParser.install_cmd(
        {
            {"start", "s"},
            "start server, e.g. start 127.0.0.1 12345",
            [this](const std::vector<std::string>           &args,
                   const std::map<std::string, std::string> &options) -> std::string {
                IPEndpoint ipendpoint("127.0.0.1:12345");
                if (args.size() == 2) {
                    auto ret = IPEndpoint::fromString(args[0] + ":" + args[1]);
                    if (ret) {
                        ipendpoint = ret.value();
                    }
                    else {
                        spdlog::error("ip endpoint error {}", ret.error().message());
                    }
                }
                else if (args.size() == 1) {
                    auto ret = IPEndpoint::fromString(args[0]);
                    if (ret) {
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
                ilias_go start_server(ipendpoint);
                return "";
             }
    },
        "Server");
    _commandParser.install_cmd(
        {{"stopserver"},
         "stop the server",
         [this](const std::vector<std::string> & /*unused*/,
                const std::map<std::string, std::string> & /*unused*/) -> std::string {
             stop_server();
             return "";
         }},
        "Server");
    // 注册开始捕获键鼠事件命令
    _commandParser.install_cmd(
        {
            {"capture", "c"},
            "start keyboard/mouse capture",
            [this](const std::vector<std::string> &  /*unused*/,
                   const std::map<std::string, std::string> &  /*unused*/) -> std::string {
                ilias_go start_capture();
                return "";
             }
    },
        "Server");
    _commandParser.install_cmd(
        {{"stopcapture"},
         "stop keyboard/mouse capture",
         [this](const std::vector<std::string> & /*unused*/,
                const std::map<std::string, std::string> & /*unused*/) -> std::string {
             stop_capture();
             return "";
         }},
        "Server");
    // 注册退出程序命令
    _commandParser.install_cmd(
        {
            {"exit", "quit", "q"},
            "exit the program",
            [this](const std::vector<std::string> &  /*unused*/,
                   const std::map<std::string, std::string> &  /*unused*/) -> std::string {
                stop();
                return "";
             }
    },
        "Server");
    // 注册连接到服务器命令
    _commandParser.install_cmd(
        {{"connect"},
         "connect to server, e.g. connect 127.0.0.1 12345",
         [this](const std::vector<std::string>           &args,
                const std::map<std::string, std::string> &options) -> std::string {
             if (args.size() == 2) {
                 ilias_go connect_to(IPEndpoint(args[0] + ":" + args[1]));
             }
             else if (args.size() == 1) {
                 ilias_go connect_to(IPEndpoint(args[0]));
             }
             else {
                 auto it       = options.find("address");
                 auto address  = it == options.end() ? "127.0.0.1" : it->second;
                 it            = options.find("port");
                 auto     port = it == options.end() ? "12345" : it->second;
                 ilias_go connect_to(IPEndpoint(address + ":" + port));
             }
             return "";
         }},
        "Client");
    _commandParser.install_cmd(
        {
            {"disconnect", "dcon"},
            "disconnect from server",
            [this](const std::vector<std::string> &  /*unused*/,
                   const std::map<std::string, std::string> &  /*unused*/) -> std::string {
                disconnect();
                return "";
             }
    },
        "Client");
    _commandParser.install_cmd(
        {
            {"thread", "t"},
            "<enable/disable> to enable independent thread for serialization",
            [this](const std::vector<std::string> &args,
                   const std::map<std::string, std::string> &  /*unused*/) -> std::string {
                if (args.empty()) {
                    return "thread <enabled/disabled>";
                }
                if (args[0] == "enabled") {
                    set_option("communication-enable-thread", true);
                    _streamflags = _streamflags | NekoProto::StreamFlag::SerializerInThread;
                }
                else {
                    set_option("communication-enable-thread", false);
                    _streamflags = (NekoProto::StreamFlag)(
                        (uint32_t)_streamflags &
                        (~(uint32_t)NekoProto::StreamFlag::SerializerInThread));
                }
                return "";
             }
    },
        "Communication");
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

auto App::connect_to(IPEndpoint endpoint) -> Task<void>
{
    if (_isServering) { // 确保没有启动服务模式
        spdlog::error("please quit server mode first");
        co_return;
    }
    if (_eventSender) { // 确保当前没有正在作为客户端运行
        co_return;
    }
    _eventSender = std::make_unique<MKSender>();
    auto ret     = co_await TcpClient::make(AF_INET);
    if (!ret) {
        spdlog::error("TcpClient::make failed {}", ret.error().message());
        co_return;
    }
    auto tcpClient = std::move(ret.value());
    _isClienting   = true;
    auto ret1      = co_await tcpClient.connect(endpoint);
    if (!ret1) {
        spdlog::error("TcpClient::connect failed {}", ret1.error().message());
        co_return;
    }
    _protoStreamClient = // 构造用于传输协议的壳
        std::make_unique<NekoProto::ProtoStreamClient<>>(_protofactory, std::move(tcpClient));
    _isClienting = true;
    while (_isClienting) {
        auto ret2 = co_await _protoStreamClient->recv(_streamflags);
        if (!ret2) {
            spdlog::error("ProtoStreamClient::recv failed {}", ret2.error().message());
            disconnect();
            co_return;
        }
        auto msg = std::move(ret2.value());
        if (_eventSender) {
            if (msg.type() == NekoProto::ProtoFactory::protoType<mks::KeyEvent>()) {
                _eventSender->send_keyboard_event(*msg.cast<mks::KeyEvent>());
            }
            else if (msg.type() == NekoProto::ProtoFactory::protoType<mks::MouseButtonEvent>()) {
                _eventSender->send_button_event(*msg.cast<mks::MouseButtonEvent>());
            }
            else if (msg.type() == NekoProto::ProtoFactory::protoType<mks::MouseMotionEvent>()) {
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

auto App::disconnect() -> void
{
    _isClienting = false;
    _eventSender.reset();
    if (_protoStreamClient) {
        _protoStreamClient->close().wait();
    }
    _eventSender.reset();
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
    auto ret = co_await TcpListener::make(AF_INET);
    if (!ret) {
        spdlog::error("TcpListener::make failed {}", ret.error().message());
        co_return;
    }
    spdlog::info("start server with {}", endpoint.toString());
    _server   = std::move(ret.value());
    auto ret2 = _server.bind(endpoint);
    if (!ret2) {
        spdlog::error("TcpListener::bind failed {}", ret2.error().message());
    }
    _isServering = true;
    while (_isServering) {
        auto ret1 = co_await _server.accept();
        if (!ret1) {
            spdlog::error("TcpListener::accept failed {}", ret1.error().message());
            stop_server();
            co_return;
        }
        auto     tcpClient = std::move(ret1.value());
        ilias_go _accept_client(std::move(tcpClient.first));
    }
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
    auto ret = co_await ILIAS_NAMESPACE::Console::fromStdin();
    if (!ret) {
        spdlog::error("Console::fromStdin failed {}", ret.error().message());
        co_return;
    }
    ILIAS_NAMESPACE::Console console       = std::move(ret.value());
    _isConsoleListening                    = true;
    std::unique_ptr<std::byte[]> strBuffer = std::make_unique<std::byte[]>(1024);
    while (_isConsoleListening) {
        memset(strBuffer.get(), 0, 1024);
        printf("%s >>>", App::app_name());
        auto ret1 = co_await console.read({strBuffer.get(), 1024});
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
        auto ret = _commandParser.parser(lineView);
        if (!ret.empty()) {
            spdlog::warn("exec {} warn: {}", lineView, ret.c_str());
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
    _listener    = std::make_unique<MKCapture>();
    while (_isCapturing) {
        NekoProto::IProto proto = (co_await _listener->get_event());
        if (proto == nullptr) {
            spdlog::error("get_event failed");
            stop_capture();
            co_return;
        }
        if (_protoStreamClient) {
            auto ret2 = co_await _protoStreamClient->send(proto, _streamflags);
            if (!ret2) {
                spdlog::error("send proto failed {}", ret2.error().message());
                stop_capture();
                co_return;
            }
        }
    }
}

auto App::stop_capture() -> void
{
    _isCapturing = false;
    if (_listener) {
        _listener->notify();
    }
    _listener.reset();
}