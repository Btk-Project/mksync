#include "mksync/base/communication.hpp"

#include <spdlog/spdlog.h>
#include <ilias/net/tcp.hpp>

#include "mksync/proto/proto.hpp"
#include "mksync/base/app.hpp"

namespace mks::base
{
    using ::ilias::Error;
    using ::ilias::IoTask;
    using ::ilias::IPEndpoint;
    using ::ilias::Task;
    using ::ilias::TcpClient;
    using ::ilias::TcpListener;
    using ::ilias::Unexpected;

    class MKS_BASE_API ServerCommand : public Command {
    public:
        enum Operation
        {
            eNone,
            eStart,
            eStop,
            eRestart
        };

    public:
        ServerCommand(MKCommunication *self) : _self(self) {}
        virtual ~ServerCommand() = default;

        auto execute() -> Task<void> override;
        auto help(std::string_view indentation) const -> std::string override;
        auto name() const -> std::string_view override;
        auto alias_names() const -> std::vector<std::string_view> override;
        void set_option(std::string_view option, std::string_view value) override;
        void set_options(const NekoProto::IProto &proto) override;
        auto get_option(std::string_view option) const -> std::string override;
        auto get_options() const -> NekoProto::IProto override;

    protected:
        MKCommunication *_self         = nullptr;
        IPEndpoint       _ipendpoint   = {"127.0.0.1:12345"};
        Operation        _operation    = eNone;
        Operation        _oldOperation = eNone;
    };

    class MKS_BASE_API ClientCommand : public ServerCommand {
    public:
        ClientCommand(MKCommunication *self) : ServerCommand(self) {}

        auto execute() -> Task<void> override;
        auto name() const -> std::string_view override;
        auto alias_names() const -> std::vector<std::string_view> override;
    };

    auto ServerCommand::execute() -> Task<void>
    {
        switch (_operation) {
        case eStart:
            if (_oldOperation != eStart && _oldOperation != eRestart) {
                co_await _self->start_server(_ipendpoint);
            }
            break;
        case eStop:
            _self->stop_server();
            break;
        case eRestart:
            _self->stop_server();
            co_await _self->start_server(_ipendpoint);
            break;
        default:
            SPDLOG_ERROR("Unknown server operation");
            break;
        }
        _oldOperation = _operation;
        co_return;
    }

    auto ServerCommand::help(std::string_view indentation) const -> std::string
    {
        std::string ret;
        for (auto alias : alias_names()) {
            ret += alias;
            ret += ", ";
        }
        if (!ret.empty()) {
            ret.pop_back();
            ret.pop_back();
        }
        return std::format(
            "{0}{1}{2}{3}{4}:\n{0}{0}{1} <start/stop/restart> [options], e.g. server start "
            "-address=127.0.0.1 "
            "-port=12345\n{0}{0}-address=$(String) server address\n{0}{0}-port=$(String) server "
            "port\n",
            indentation, name(), ret.empty() ? "" : "(", ret, ret.empty() ? "" : ")");
    }

    auto ServerCommand::name() const -> std::string_view
    {
        return "server";
    }

    auto ServerCommand::alias_names() const -> std::vector<std::string_view>
    {
        return {"s"};
    }

    void ServerCommand::set_option(std::string_view option, std::string_view value)
    {
        if (option == "address") {
            if (value.find(':') != std::string_view::npos) {
                _ipendpoint = IPEndpoint(std::format("[{}]:{}", value, _ipendpoint.port()));
            }
            else {
                _ipendpoint = IPEndpoint(std::format("{}:{}", value, _ipendpoint.port()));
            }
        }
        else if (option == "port") {
            int val;
            if (std::from_chars(value.data(), value.data() + value.size(), val).ec == std::errc() &&
                val >= 0 && val <= 65535) {
                if (_ipendpoint.family() == AF_INET) {
                    _ipendpoint =
                        IPEndpoint(std::format("{}:{}", _ipendpoint.address().toString(), val));
                }
                else {
                    _ipendpoint =
                        IPEndpoint(std::format("[{}]:{}", _ipendpoint.address().toString(), val));
                    SPDLOG_ERROR("{}", _ipendpoint.address().toString());
                }
            }
            else {
                SPDLOG_ERROR("Invalid port number: {}", value);
            }
        }
        else if (option.empty() && value == "start") {
            _operation = eStart;
        }
        else if (option.empty() && value == "stop") {
            _operation = eStop;
        }
        else if (option.empty() && value == "restart") {
            _operation = eRestart;
        }
        else {
            SPDLOG_ERROR("Unknown option/command: {}{}{}", option, option.empty() ? "" : "=",
                         value);
        }
    }

    void ServerCommand::set_options([[maybe_unused]] const NekoProto::IProto &proto)
    {
        // TODO: make proto for this command if needed
        SPDLOG_ERROR("Not implemented proto parameter");
    }

    auto ServerCommand::get_option(std::string_view option) const -> std::string
    {
        if (option == "address") {
            return _ipendpoint.address().toString();
        }
        if (option == "port") {
            return std::to_string(_ipendpoint.port());
        }
        return {};
    }

    auto ServerCommand::get_options() const -> NekoProto::IProto
    {
        // TODO: make proto for this command if needed
        return {};
    }

    auto ClientCommand::execute() -> Task<void>
    {
        switch (_operation) {
        case eStart:
            if (_oldOperation != eStart && _oldOperation != eRestart) {
                co_await _self->connect_to(_ipendpoint);
            }
            break;
        case eStop:
            _self->disconnect();
            break;
        case eRestart:
            _self->disconnect();
            co_await _self->connect_to(_ipendpoint);
            break;
        default:
            SPDLOG_ERROR("Unknown server operation");
            break;
        }
        _oldOperation = _operation;
        co_return;
    }

    auto ClientCommand::name() const -> std::string_view
    {
        return "client";
    }

    auto ClientCommand::alias_names() const -> std::vector<std::string_view>
    {
        return {"c"};
    }

    MKCommunication::MKCommunication(::ilias::IoContext *ctx) : _ctx(ctx)
    {
        _currentPeer = _protoStreamClients.end();
        _subscribers.insert({NekoProto::ProtoFactory::protoType<mks::KeyEvent>(),
                             NekoProto::ProtoFactory::protoType<mks::MouseButtonEvent>(),
                             NekoProto::ProtoFactory::protoType<mks::MouseWheelEvent>(),
                             NekoProto::ProtoFactory::protoType<mks::MouseMotionEvent>(),
                             NekoProto::ProtoFactory::protoType<mks::VirtualScreenInfo>()});
    }

    auto MKCommunication::start() -> ::ilias::Task<int>
    {
        if (_status == eDisable) {
            _status = eEnable;
        }
        co_return 0;
    }

    auto MKCommunication::stop() -> ::ilias::Task<int>
    {
        _status = eDisable;
        stop_server();
        disconnect();
        co_return 0;
    }

    auto MKCommunication::name() -> const char *
    {
        return "MKCommunication";
    }

    auto MKCommunication::get_subscribers() -> std::vector<int>
    {
        std::vector<int> ret;
        for (const auto &item : _subscribers) {
            ret.push_back(item);
        }
        return ret;
    }

    auto MKCommunication::set_current_peer(std::string_view currentPeer) -> void
    {
        auto item = _protoStreamClients.find(std::string(currentPeer));
        if (item != _protoStreamClients.end()) {
            _currentPeer = item;
            _syncEvent.set();
        }
        else {
            SPDLOG_WARN("peer {} not found", currentPeer);
            _currentPeer = _protoStreamClients.end();
        }
    }

    auto MKCommunication::current_peer() const -> std::string
    {
        if (_currentPeer != _protoStreamClients.end()) {
            return _currentPeer->first;
        }
        return "";
    }

    auto MKCommunication::peers() const -> std::vector<std::string>
    {
        std::vector<std::string> peers;
        for (const auto &item : _protoStreamClients) {
            peers.push_back(item.first);
        }
        return peers;
    }

    auto MKCommunication::send(NekoProto::IProto &event, std::string_view peer)
        -> ilias::IoTask<void>
    {
        if (_status == eDisable) {
            co_return Unexpected<Error>(Error::Unknown);
        }
        auto item = _protoStreamClients.find(std::string(peer));
        if (item == _protoStreamClients.end()) {
            co_return Unexpected<Error>(Error::Unknown);
        }
        auto ret = co_await item->second.send(event, _flags);
        if (!ret) {
            SPDLOG_ERROR("send event failed {}", ret.error().message());
        }
        co_return ret;
    }

    auto MKCommunication::recv(std::string_view peer) -> ilias::IoTask<NekoProto::IProto>
    {
        if (_status == eDisable) {
            co_return Unexpected<Error>(Error::Unknown);
        }
        auto item = _protoStreamClients.find(std::string(peer));
        if (item == _protoStreamClients.end()) {
            co_return Unexpected<Error>(Error::InvalidArgument);
        }
        auto ret = co_await item->second.recv(_flags);
        if (!ret) {
            SPDLOG_ERROR("recv event failed {}", ret.error().message());
        }
        co_return ret;
    }

    auto MKCommunication::handle_event(NekoProto::IProto &event) -> ::ilias::Task<void>
    {
        if (_currentPeer == _protoStreamClients.end()) {
            co_return;
        }

        SPDLOG_INFO("send {} to {}", event.protoName(), _currentPeer->first);
        if (auto ret = co_await _currentPeer->second.send(event, _flags); !ret) {
            SPDLOG_ERROR("send event failed {}", ret.error().message());
            if (ret.error() == Error::ConnectionReset || ret.error() == Error::ChannelBroken) {
                co_await _currentPeer->second.close();
                _protoStreamClients.erase(_currentPeer);
                _currentPeer = _protoStreamClients.begin();
            }
        }
    }

    auto MKCommunication::get_event() -> ::ilias::IoTask<NekoProto::IProto>
    {
        while (_status != eDisable) {
            if (_currentPeer == _protoStreamClients.end()) {
                _syncEvent.clear();
                if (auto ret = co_await _syncEvent; !ret) {
                    // 取消或其他异常状态。直接退出。
                    co_return Unexpected<Error>(ret.error());
                }
            }
            if (_currentPeer != _protoStreamClients.end()) {
                if (auto ret = co_await _currentPeer->second.recv(_flags); !ret) {
                    // 对端出现异常，关闭对端，但不关闭自己的事件生成。
                    SPDLOG_ERROR("recv event failed {}", ret.error().message());
                    _protoStreamClients.erase(_currentPeer);
                    _currentPeer = _protoStreamClients.end();
                }
                else {
                    // 获得消息生成一个事件。
                    SPDLOG_INFO("recv {} from {}", ret.value().protoName(), _currentPeer->first);
                    co_return ret;
                }
            }
        }
        co_return ilias::Unexpected<ilias::Error>(ilias::Error::Unknown);
    }

    auto MKCommunication::status() -> Status
    {
        return _status;
    }

    auto MKCommunication::set_flags(NekoProto::StreamFlag flags) -> void
    {
        _flags = flags;
    }

    auto MKCommunication::add_subscribers(int type) -> void
    {
        _subscribers.insert(type);
    }

    auto MKCommunication::start_server(ilias::IPEndpoint endpoint) -> ilias::Task<void>
    {
        if (_status == eDisable || _status == eClient) {
            SPDLOG_ERROR("{} is disabled", name());
            co_return;
        }
        if (_status == eServer) { // 确保没有正在作为服务端运行
            SPDLOG_ERROR("server/client is already running");
            co_return;
        }
        TcpListener server;
        if (auto ret = co_await TcpListener::make(endpoint.family()); !ret) {
            SPDLOG_ERROR("TcpListener::make failed {}", ret.error().message());
            co_return;
        }
        else {
            server = std::move(ret.value());
        }
        if (auto res = server.bind(endpoint); !res) {
            SPDLOG_ERROR("TcpListener::bind failed {}", res.error().message());
            server.close();
            co_return;
        }
        else {
            _status = eServer;
            SPDLOG_INFO("server listen {}", endpoint.toString());
        }
        _cancelHandle = ilias::spawn(*_ctx, _server_loop(std::move(server)));
    }

    auto MKCommunication::_server_loop(::ilias::TcpListener tcplistener) -> ::ilias::Task<void>
    {
        while (true) {
            if (auto ret1 = co_await tcplistener.accept(); !ret1) {
                if (ret1.error() != Error::Canceled) {
                    SPDLOG_ERROR("TcpListener::accept failed {}", ret1.error().message());
                }
                tcplistener.close();
                stop_server();
                co_return;
            }
            else {
                // TODO:增加处理多个连接的请求。
                auto        tcpClient = std::move(ret1.value());
                std::string ipstr     = tcpClient.second.toString();
                SPDLOG_INFO("new connection from {}", ipstr);
                _protoStreamClients.emplace(std::make_pair(
                    ipstr,
                    NekoProto::ProtoStreamClient<>(_protofactory, std::move(tcpClient.first))));
#if 1 // 增加处理当前连接的逻辑
                _currentPeer = _protoStreamClients.find(ipstr);
                _syncEvent.set();
#endif
            }
        }
    }

    auto MKCommunication::stop_server() -> void
    {
        if (_status != eServer) {
            return;
        }
        if (_cancelHandle) {
            _cancelHandle.cancel();
            _cancelHandle = {};
        }
        if (_protoStreamClients.size() != 0U) {
            _protoStreamClients.clear();
            _currentPeer = _protoStreamClients.end();
        }
        _status = eEnable;
    }

    // client
    /**
     * @brief connect to server
     * 如果需要ilias_go参数列表必须复制。
     * @param endpoint
     * @return Task<void>
     */
    auto MKCommunication::connect_to(ilias::IPEndpoint endpoint) -> ilias::Task<void>
    {
        if (_status == eDisable || _status == eServer) {
            SPDLOG_ERROR("Communication is server mode or disabled");
            co_return;
        }
        if (_protoStreamClients.size() > 0) { // 确保没有正在作为服务端运行
            SPDLOG_ERROR("server/client is already running");
            co_return;
        }
        TcpClient tcpClient;
        if (auto ret = co_await TcpClient::make(endpoint.family()); !ret) {
            SPDLOG_ERROR("TcpClient::make failed {}", ret.error().message());
            co_return;
        }
        else {
            tcpClient = std::move(ret.value());
        }
        if (auto res = co_await tcpClient.connect(endpoint); !res) {
            SPDLOG_ERROR("TcpClient::connect failed {}", res.error().message());
            co_return;
        }
        _currentPeer = _protoStreamClients.emplace_hint(
            _protoStreamClients.begin(),
            std::make_pair("self",
                           NekoProto::ProtoStreamClient<>(
                               _protofactory, std::move(tcpClient)))); // 构造用于传输协议的壳
        _syncEvent.set();
        SPDLOG_INFO("connect to {}", endpoint.toString());
        _status = eClient;
        co_return;
    }

    auto MKCommunication::disconnect() -> void
    {
        if (_status != eClient) {
            return;
        }
        if (_protoStreamClients.size() != 0U) {
            _protoStreamClients.clear();
            _currentPeer = _protoStreamClients.end();
        }
        _status = eEnable;
    }

    auto MKCommunication::set_communication_options(
        [[maybe_unused]] const CommandInvoker::ArgsType    &args,
        [[maybe_unused]] const CommandInvoker::OptionsType &options) -> std::string
    {
        auto it = options.find("thread");
        if (it != options.end() && std::get<bool>(it->second)) {
            _flags = _flags | NekoProto::StreamFlag::SerializerInThread;
        }
        else {
            _flags = (NekoProto::StreamFlag)(
                (uint32_t)_flags & (~(uint32_t)NekoProto::StreamFlag::SerializerInThread));
        }
        return "";
    }

    auto MKCommunication::make(App &app) -> std::unique_ptr<MKCommunication, void (*)(NodeBase *)>
    {
        using CallbackType = std::string (MKCommunication::*)(const CommandInvoker::ArgsType &,
                                                              const CommandInvoker::OptionsType &);
        std::unique_ptr<MKCommunication, void (*)(NodeBase *)> communication(
            new MKCommunication(app.get_io_context()),
            [](NodeBase *ptr) { delete static_cast<MKCommunication *>(ptr); });
        auto commandInstaller = app.command_installer(communication->name());
        // 注册服务器监听相关命令
        commandInstaller(std::make_unique<ServerCommand>(communication.get()));
        // 注册连接到服务器命令
        commandInstaller(std::make_unique<ClientCommand>(communication.get()));
        commandInstaller(std::make_unique<CommonCommand>(CommandInvoker::CommandsData{
            {"communication_attributes", "comm_attr"},
            "set communication attributes",
            std::bind(static_cast<CallbackType>(&MKCommunication::set_communication_options),
                      communication.get(), std::placeholders::_1, std::placeholders::_2),
            {
             {"thread", CommandInvoker::OptionsData::eBool,
                 "enable independent thread for serialization"},
             }
        }));
        return communication;
    }

} // namespace mks::base