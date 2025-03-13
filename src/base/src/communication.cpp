#include "mksync/base/communication.hpp"

#include <spdlog/spdlog.h>
#include <ilias/net/tcp.hpp>
#include <exception>

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
        ServerCommand(MKCommunication *self, const char *name = "server");
        virtual ~ServerCommand() = default;

        auto execute() -> Task<void> override;
        auto help() const -> std::string override;
        auto name() const -> std::string_view override;
        auto alias_names() const -> std::vector<std::string_view> override;
        void set_option(std::string_view option, std::string_view value) override;
        void parser_options(const std::vector<const char *> &args) override;
        auto get_option(std::string_view option) const -> std::string override;
        void set_options(const NekoProto::IProto &proto) override;
        auto get_options() const -> NekoProto::IProto override;

    protected:
        MKCommunication *_self         = nullptr;
        IPEndpoint       _ipendpoint   = {"127.0.0.1:12345"};
        Operation        _operation    = eNone;
        Operation        _oldOperation = eNone;
        cxxopts::Options _options;
    };

    class MKS_BASE_API ClientCommand : public ServerCommand {
    public:
        ClientCommand(MKCommunication *self);

        auto execute() -> Task<void> override;
        auto name() const -> std::string_view override;
        auto alias_names() const -> std::vector<std::string_view> override;
        void set_options(const NekoProto::IProto &proto) override;
        auto get_options() const -> NekoProto::IProto override;
    };

    class MKS_BASE_API ServerCommunication : public IServerCommunication {
    public:
        ServerCommunication(MKCommunication *self);
        auto declare_proto_to_send(int type) -> void override;
        auto send(NekoProto::IProto &event, std::string_view peer) -> ilias::IoTask<void> override;
        auto recv(std::string_view peer) -> ilias::IoTask<NekoProto::IProto> override;
        auto peers() const -> std::vector<std::string> override;

    private:
        MKCommunication *_self = nullptr;
    };

    class MKS_BASE_API ClientCommunication : public IClientCommunication {
    public:
        ClientCommunication(MKCommunication *self);
        auto declare_proto_to_send(int type) -> void override;
        auto send(NekoProto::IProto &event) -> ilias::IoTask<void> override;
        auto recv() -> ilias::IoTask<NekoProto::IProto> override;

    private:
        MKCommunication *_self = nullptr;
    };

    ServerCommunication::ServerCommunication(MKCommunication *self) : _self(self) {}

    auto ServerCommunication::declare_proto_to_send(int type) -> void
    {
        return _self->add_subscribers(type);
    }

    auto ServerCommunication::send(NekoProto::IProto &event, std::string_view peer)
        -> ilias::IoTask<void>
    {
        return _self->send(event, peer);
    }

    auto ServerCommunication::recv(std::string_view peer) -> ilias::IoTask<NekoProto::IProto>
    {
        return _self->recv(peer);
    }

    auto ServerCommunication::peers() const -> std::vector<std::string>
    {
        return _self->peers();
    }

    ClientCommunication::ClientCommunication(MKCommunication *self) : _self(self) {}

    auto ClientCommunication::declare_proto_to_send(int type) -> void
    {
        return _self->add_subscribers(type);
    }

    auto ClientCommunication::send(NekoProto::IProto &event) -> ilias::IoTask<void>
    {
        return _self->send(event, "self");
    }

    auto ClientCommunication::recv() -> ilias::IoTask<NekoProto::IProto>
    {
        return _self->recv("self");
    }

    ServerCommand::ServerCommand(MKCommunication *self, const char *name)
        : _self(self), _options(name)
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
        _options.custom_help(
            fmt::format("{}{}{}{} <start/stop/restart> [options...], e.g. server start",
                        this->name(), ret.empty() ? "" : "(", ret, ret.empty() ? "" : ")"));
        _options.add_options()("a,address", "server address",
                               cxxopts::value<std::string>()->default_value("127.0.0.1"), "[ip]")(
            "p,port", "server port", cxxopts::value<uint16_t>()->default_value("12345"), "[int]");
        _options.allow_unrecognised_options();
    }

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

    auto ServerCommand::help() const -> std::string
    {
        return _options.help({}, false);
    }

    auto ServerCommand::name() const -> std::string_view
    {
        return _options.program();
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
        ILIAS_ASSERT(proto.type() == NekoProto::ProtoFactory::protoType<ServerControl>());
        const auto *control = proto.cast<ServerControl>();
        switch (control->cmd) {
        case ServerControl::eStart:
            _operation = eStart;
            break;
        case ServerControl::eStop:
            _operation = eStop;
            break;
        case ServerControl::eRestart:
            _operation = eRestart;
            break;
        default:
            SPDLOG_ERROR("Unknown server operation");
        }
        std::string address = "127.0.0.1";
        if (!control->ip.empty()) {
            address = control->ip;
        }
        uint16_t port = 12345;
        if (control->port != 0) {
            port = control->port;
        }
        if (address.find(':') != std::string::npos) {
            _ipendpoint = IPEndpoint(std::format("[{}]:{}", address, port));
        }
        else {
            _ipendpoint = IPEndpoint(std::format("{}:{}", address, port));
        }
    }

    void ServerCommand::parser_options(const std::vector<const char *> &args)
    {
        auto results = _options.parse(int(args.size()), args.data());

        if (results.count("address") != 0U) {
            set_option("address", results["address"].as<std::string>());
        }
        if (results.count("port") != 0U) {
            set_option("port", std::to_string(results["port"].as<uint16_t>()));
        }
        for (const auto &result : results.unmatched()) {
            if (result == "start") {
                _operation = eStart;
            }
            else if (result == "stop") {
                _operation = eStop;
            }
            else if (result == "restart") {
                _operation = eRestart;
            }
            else {
                SPDLOG_ERROR("unknow argments {}", result);
            }
        }
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
        return ServerControl::emplaceProto((ServerControl::Command)-1,
                                           _ipendpoint.address().toString(), _ipendpoint.port());
    }

    ClientCommand::ClientCommand(MKCommunication *self) : ServerCommand(self, "client")
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
        _options.custom_help(
            fmt::format("{}{}{}{} <start/stop/restart> [options...], e.g. client start",
                        this->name(), ret.empty() ? "" : "(", ret, ret.empty() ? "" : ")"));
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
        return _options.program();
    }

    auto ClientCommand::alias_names() const -> std::vector<std::string_view>
    {
        return {"c"};
    }

    void ClientCommand::set_options([[maybe_unused]] const NekoProto::IProto &proto)
    {
        ILIAS_ASSERT(proto.type() == NekoProto::ProtoFactory::protoType<ClientControl>());
        const auto *control = proto.cast<ClientControl>();
        switch (control->cmd) {
        case ClientControl::eStart:
            _operation = eStart;
            break;
        case ClientControl::eStop:
            _operation = eStop;
            break;
        case ClientControl::eRestart:
            _operation = eRestart;
            break;
        default:
            SPDLOG_ERROR("Unknown server operation");
        }
        std::string address = "127.0.0.1";
        if (!control->ip.empty()) {
            address = control->ip;
        }
        uint16_t port = 12345;
        if (control->port != 0) {
            port = control->port;
        }
        if (address.find(':') != std::string::npos) {
            _ipendpoint = IPEndpoint(std::format("[{}]:{}", address, port));
        }
        else {
            _ipendpoint = IPEndpoint(std::format("{}:{}", address, port));
        }
    }

    auto ClientCommand::get_options() const -> NekoProto::IProto
    {
        return ClientControl::emplaceProto((ClientControl::Command)-1,
                                           _ipendpoint.address().toString(), _ipendpoint.port());
    }

    MKCommunication::MKCommunication(App *app) : _app(app)
    {
        _currentPeer         = _protoStreamClients.end();
        _communicationWapper = std::make_unique<ClientCommunication>(this);
    }

    auto MKCommunication::enable() -> ::ilias::Task<int>
    {
        if (_status == eDisable) {
            _status = eEnable;
        }
        co_return 0;
    }

    auto MKCommunication::disable() -> ::ilias::Task<int>
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
        return {NekoProto::ProtoFactory::protoType<FocusScreenChanged>()};
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

    auto MKCommunication::handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void>
    {
        if (event.type() == NekoProto::ProtoFactory::protoType<FocusScreenChanged>()) {
            const auto *focusScreenChanged = event.cast<FocusScreenChanged>();
            set_current_peer(focusScreenChanged->peer);
            co_return;
        }
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
        _app->node_manager().subscriber(type, this);
    }

    auto MKCommunication::remove_subscribers(int type) -> void
    {
        _app->node_manager().unsubscribe(type, this);
    }

    auto MKCommunication::get_communication() -> ICommunication *
    {
        return _communicationWapper.get();
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
        if (dynamic_cast<ServerCommunication *>(_communicationWapper.get()) == nullptr) {
            _communicationWapper = std::make_unique<ServerCommunication>(this);
        }
        _cancelHandle = ilias::spawn(*_app->get_io_context(), _server_loop(std::move(server)));
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
        if (dynamic_cast<ClientCommunication *>(_communicationWapper.get()) == nullptr) {
            _communicationWapper = std::make_unique<ClientCommunication>(this);
        }
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
            new MKCommunication(&app),
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