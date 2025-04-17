#include "mksync/core/communication.hpp"

#include <spdlog/spdlog.h>
#include <ilias/net/tcp.hpp>

#include "mksync/proto/proto.hpp"
#include "mksync/core/app.hpp"
#include "mksync/base/default_configs.hpp"

MKS_BEGIN
using ::ilias::Error;
using ::ilias::IoTask;
using ::ilias::IPEndpoint;
using ::ilias::Task;
using ::ilias::TcpClient;
using ::ilias::TcpListener;
using ::ilias::Unexpected;

/**
 * @brief 通信模块服务命令
 * @note 主要负责解析和执行启动服务模式相关命令
 */
class MKS_CORE_API ServerCommand : public Command {
public:
    enum Operation
    {
        eNone,
        eStart,
        eStop,
        eRestart
    };

public:
    ServerCommand(IApp *app, MKCommunication *self, const char *name = "server");
    virtual ~ServerCommand() = default;

    auto execute() -> Task<std::string> override;
    auto help() const -> std::string override;
    auto name() const -> std::string_view override;
    auto alias_names() const -> std::vector<std::string_view> override;
    void set_option(std::string_view option, std::string_view value) override;
    void parser_options(const std::vector<const char *> &args) override;
    void set_options(const NekoProto::IProto &proto) override;
    auto need_proto_type() const -> int override;

protected:
    IApp            *_app        = nullptr;
    MKCommunication *_self       = nullptr;
    IPEndpoint       _ipendpoint = {};
    Operation        _operation  = eNone;
    cxxopts::Options _options;
};

/**
 * @brief 通信模块客户端命令
 * @note 主要负责解析和执行启动客户端模式相关命令
 */
class MKS_CORE_API ClientCommand : public ServerCommand {
public:
    ClientCommand(IApp *app, MKCommunication *self);

    auto execute() -> Task<std::string> override;
    auto name() const -> std::string_view override;
    auto alias_names() const -> std::vector<std::string_view> override;
    void set_options(const NekoProto::IProto &proto) override;
    auto need_proto_type() const -> int override;
};

/**
 * @brief 通信模块服务器模式下的对外接口
 *
 */
class MKS_CORE_API ServerCommunication : public IServerCommunication {
public:
    ServerCommunication(MKCommunication *self);
    auto subscribes(int type) -> void override;
    auto subscribes(std::vector<int> types) -> void override;
    auto unsubscribes(int type) -> void override;
    auto unsubscribes(std::vector<int> types) -> void override;
    auto send(NekoProto::IProto &event, std::string_view peer) -> ilias::IoTask<void> override;
    auto peers() const -> std::vector<std::string> override;

private:
    MKCommunication *_self = nullptr;
};

/**
 * @brief 通信模块客户端模式下的对外接口
 *
 */
class MKS_CORE_API ClientCommunication : public IClientCommunication {
public:
    ClientCommunication(MKCommunication *self);
    auto subscribes(int type) -> void override;
    auto subscribes(std::vector<int> types) -> void override;
    auto unsubscribes(int type) -> void override;
    auto unsubscribes(std::vector<int> types) -> void override;
    auto send(NekoProto::IProto &event) -> ilias::IoTask<void> override;

private:
    MKCommunication *_self = nullptr;
};

ServerCommunication::ServerCommunication(MKCommunication *self) : _self(self) {}

auto ServerCommunication::subscribes(int type) -> void
{
    return _self->add_subscribe(type);
}

auto ServerCommunication::subscribes(std::vector<int> types) -> void
{
    for (auto type : types) {
        _self->add_subscribe(type);
    }
}

auto ServerCommunication::unsubscribes(int type) -> void
{
    return _self->remove_subscribers(type);
}

auto ServerCommunication::unsubscribes(std::vector<int> types) -> void
{
    for (auto type : types) {
        _self->remove_subscribers(type);
    }
}

auto ServerCommunication::send(NekoProto::IProto &event, std::string_view peer)
    -> ilias::IoTask<void>
{
    return _self->send(event, peer);
}

auto ServerCommunication::peers() const -> std::vector<std::string>
{
    return _self->peers();
}

ClientCommunication::ClientCommunication(MKCommunication *self) : _self(self) {}

auto ClientCommunication::subscribes(int type) -> void
{
    return _self->add_subscribe(type);
}

auto ClientCommunication::subscribes(std::vector<int> types) -> void
{
    for (auto type : types) {
        _self->add_subscribe(type);
    }
}

auto ClientCommunication::unsubscribes(int type) -> void
{
    return _self->remove_subscribers(type);
}

auto ClientCommunication::unsubscribes(std::vector<int> types) -> void
{
    for (auto type : types) {
        _self->remove_subscribers(type);
    }
}

auto ClientCommunication::send(NekoProto::IProto &event) -> ilias::IoTask<void>
{
    return _self->send(event, "self");
}

ServerCommand::ServerCommand(IApp *app, MKCommunication *self, const char *name)
    : _app(app), _self(self), _options(name)
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
    _ipendpoint = IPEndpoint(
        _app->settings().get(server_ipaddress_config_name, server_ipaddress_default_value));
    _options.custom_help(
        fmt::format("{}{}{}{} <start/stop/restart> [options...], e.g. server start", this->name(),
                    ret.empty() ? "" : "(", ret, ret.empty() ? "" : ")"));
    _options.add_options()(
        "a,address", "server address",
        cxxopts::value<std::string>()->default_value(_ipendpoint.address().toString()), "[ip]")(
        "p,port", "server port",
        cxxopts::value<uint16_t>()->default_value(std::to_string(_ipendpoint.port())), "[int]");
    _options.allow_unrecognised_options();
}

auto ServerCommand::execute() -> Task<std::string>
{
    auto makeErrorStr = [this](int ret) -> std::string {
        switch (ret) {
        case -1:
            return fmt::format("{} is disable", _self->name());
        case -2:
            return "server/client is already running";
        case -3:
            return "failed to make TcpListener";
        case -4:
            return "failed to bind";
        default:
            return std::format("Failed to start server, error code: {}", ret);
        }
    };
    switch (_operation) {
    case eStart:
        if (auto ret = co_await _self->listen(_ipendpoint); ret == 0) {
            co_await _app->push_event(AppStatusChanged::emplaceProto(AppStatusChanged::eStarted,
                                                                     AppStatusChanged::eServer),
                                      _self);
        }
        else {
            co_return makeErrorStr(ret);
        }
        break;
    case eStop:
        co_await _self->close();
        co_await _app->push_event(
            AppStatusChanged::emplaceProto(AppStatusChanged::eStopped, AppStatusChanged::eServer),
            _self);
        break;
    case eRestart:
        co_await _self->close();
        if (auto ret = co_await _self->listen(_ipendpoint); ret != 0) {
            co_return makeErrorStr(ret);
        }
        break;
    default:
        SPDLOG_ERROR("Unknown server operation");
        co_return "Unknown server operation";
    }
    _app->settings().set(server_ipaddress_config_name, _ipendpoint.toString());
    co_return "";
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
        SPDLOG_ERROR("Unknown option/command: {}{}{}", option, option.empty() ? "" : "=", value);
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
    std::string address = _ipendpoint.address().toString();
    if (!control->ip.empty()) {
        address = control->ip;
    }
    uint16_t port = _ipendpoint.port();
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

auto ServerCommand::need_proto_type() const -> int
{
    return NekoProto::ProtoFactory::protoType<ServerControl>();
}

ClientCommand::ClientCommand(IApp *app, MKCommunication *self) : ServerCommand(app, self, "client")
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
        fmt::format("{}{}{}{} <start/stop/restart> [options...], e.g. client start", this->name(),
                    ret.empty() ? "" : "(", ret, ret.empty() ? "" : ")"));
}

auto ClientCommand::execute() -> Task<std::string>
{
    auto makeErrorStr = [](int ret) -> std::string {
        switch (ret) {
        case -1:
            return "Communication is server mode or disabled.";
        case -2:
            return "server/client is already running";
        case -3:
            return "Failed to make tcp";
        case -4:
            return "Failed to connect to server";
        case -5:
            return "Failed to handshake with server";
        default:
            return "Failed to connect to server";
        }
    };
    switch (_operation) {
    case eStart:
        if (auto ret = co_await _self->connect(_ipendpoint); ret == 0) {
            co_await _app->push_event(AppStatusChanged::emplaceProto(AppStatusChanged::eStarted,
                                                                     AppStatusChanged::eClient),
                                      _self);
        }
        else {
            co_return makeErrorStr(ret);
        }
        break;
    case eStop:
        co_await _self->close();
        co_await _app->push_event(
            AppStatusChanged::emplaceProto(AppStatusChanged::eStopped, AppStatusChanged::eClient));
        break;
    case eRestart:
        co_await _self->close();
        if (auto ret = co_await _self->connect(_ipendpoint); ret != 0) {
            co_return makeErrorStr(ret);
        }
        break;
    default:
        SPDLOG_ERROR("Unknown server operation");
        co_return "Unknown server operation";
    }
    _app->settings().set(server_ipaddress_config_name, _ipendpoint.toString());
    co_return "";
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
    std::string address = _ipendpoint.address().toString();
    if (!control->ip.empty()) {
        address = control->ip;
    }
    uint16_t port = _ipendpoint.port();
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

auto ClientCommand::need_proto_type() const -> int
{
    return NekoProto::ProtoFactory::protoType<ClientControl>();
}

MKCommunication::MKCommunication(IApp *app)
    : _app(app), _protofactory(1, 0, 0), _taskScope(*_app->get_io_context())
{
    _currentPeer         = _protoStreamClients.end();
    _communicationWapper = std::make_unique<ClientCommunication>(this);
    _taskScope.setAutoCancel(true);
}

MKCommunication::~MKCommunication()
{
    _app->command_uninstaller(this);
}

auto MKCommunication::setup() -> ::ilias::Task<int>
{
    if (_status == eDisable) {
        _status = eEnable;
    }
    using CallbackType = Task<std::string> (MKCommunication::*)(
        const CommandInvoker::ArgsType &, const CommandInvoker::OptionsType &);

    auto commandInstaller = _app->command_installer(this);
    // 注册服务器监听相关命令
    commandInstaller(std::make_unique<ServerCommand>(_app, this));
    // 注册连接到服务器命令
    commandInstaller(std::make_unique<ClientCommand>(_app, this));
    commandInstaller(std::make_unique<CommonCommand>(CommandInvoker::CommandsData{
        {"communication_attributes", "comm_attr"},
        "communication_attributes(comm_attr) [options...], set communication attributes",
        std::bind(static_cast<CallbackType>(&MKCommunication::set_communication_options), this,
                  std::placeholders::_1, std::placeholders::_2),
        {
         {"thread", CommandInvoker::OptionsData::eBool,
             "enable independent thread for serialization"},
         }
    }));
    SPDLOG_INFO("node {}<{}> setup", name(), (void *)this);
    co_return 0;
}

auto MKCommunication::teardown() -> ::ilias::Task<int>
{
    _app->command_uninstaller(this);
    co_await close();
    _status = eDisable;
    SPDLOG_INFO("node {}<{}> teardown", name(), (void *)this);
    co_return 0;
}

auto MKCommunication::name() -> const char *
{
    return "MKCommunication";
}

auto MKCommunication::get_subscribes() -> std::vector<int>
{
    return {NekoProto::ProtoFactory::protoType<FocusScreenChanged>()};
}

auto MKCommunication::set_current_peer(std::string_view currentPeer) -> void
{
    auto item = _protoStreamClients.find(currentPeer);
    if (item != _protoStreamClients.end()) {
        _currentPeer = item;
    }
    else {
        if (currentPeer != "self") {
            SPDLOG_WARN("peer {} not found", currentPeer);
        }
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

auto MKCommunication::send(NekoProto::IProto &event, std::string_view peer) -> ilias::IoTask<void>
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

    if (auto ret = co_await _currentPeer->second.send(event, _flags); !ret) {
        SPDLOG_ERROR("send event failed {}", ret.error().message());
        if (ret.error() == Error::ConnectionReset || ret.error() == Error::ChannelBroken) {
            co_await _currentPeer->second.close();
            _protoStreamClients.erase(_currentPeer);
            _currentPeer = _protoStreamClients.begin();
        }
    }
}

auto MKCommunication::status() -> Status
{
    return _status;
}

auto MKCommunication::set_flags(NekoProto::StreamFlag flags) -> void
{
    _flags = flags;
}

auto MKCommunication::add_subscribe(int type) -> void
{
    _app->node_manager().subscribe(type, this);
}

auto MKCommunication::remove_subscribers(int type) -> void
{
    _app->node_manager().unsubscribe(type, this);
}

auto MKCommunication::get_communication() -> ICommunication *
{
    return _communicationWapper.get();
}

auto MKCommunication::listen(ilias::IPEndpoint endpoint) -> ilias::Task<int>
{
    if (_status == eDisable || _status == eClient) {
        SPDLOG_ERROR("{} is disabled", name());
        co_return -1;
    }
    if (_status == eServer) { // 确保没有正在作为服务端运行
        SPDLOG_ERROR("server/client is already running");
        co_return -2;
    }
    TcpListener server;
    if (auto ret = co_await TcpListener::make(endpoint.family()); !ret) {
        SPDLOG_ERROR("TcpListener make failed {}", ret.error().message());
        co_return -3;
    }
    else {
        server = std::move(ret.value());
    }
    if (auto res = server.bind(endpoint); !res) {
        SPDLOG_ERROR("TcpListener bind {} failed {}", endpoint.toString(), res.error().message());
        server.close();
        co_return -4;
    }
    else {
        _status = eServer;
        SPDLOG_INFO("server listen {}", endpoint.toString());
        _ipEndpoint = endpoint; // 记录服务端监听的地址
        if (dynamic_cast<ServerCommunication *>(_communicationWapper.get()) == nullptr) {
            _communicationWapper = std::make_unique<ServerCommunication>(this);
        }
        _taskScope.spawn(_server_loop(std::move(server)));
        co_return 0;
    }
}

auto MKCommunication::_server_loop(::ilias::TcpListener tcplistener) -> ::ilias::Task<void>
{
    while (_status == eServer) {
        if (auto ret1 = co_await tcplistener.accept(); ret1) {
            auto        tcpClient = std::move(ret1.value());
            std::string ipstr     = tcpClient.second.toString();
            SPDLOG_INFO("new connection from {}", ipstr);
            auto item = _protoStreamClients.emplace(std::make_pair(
                ipstr, NekoProto::ProtoStreamClient<>(_protofactory, std::move(tcpClient.first))));
            if (!item.second) {
                SPDLOG_ERROR("connect {} insert map error!", ipstr);
                continue;
            }
            if (auto ret = co_await _server_handshake(ipstr, &(item.first->second)); ret != 0) {
                _protoStreamClients.erase(item.first);
            }
            _taskScope.spawn(_client_connection_loop(ipstr, item.first->second));
        }
        else {
            if (ret1.error() != Error::Canceled) {
                // TODO: 一旦出现异常就断开，此处应该增加异常状态的识别和重试处理。
                SPDLOG_ERROR("TcpListener::accept failed {}", ret1.error().message());
                co_await _app->push_event(ServerControl::emplaceProto(
                                              ServerControl::eStart,
                                              _ipEndpoint.address().toString(), _ipEndpoint.port()),
                                          this);
            }
            else {
                SPDLOG_INFO("server loop canceled!");
            }
            tcplistener.close();
            break;
        }
    }
    if (_status == eServer) {
        _status = eEnable;
    }
    _protoStreamClients.clear();
}

// 服务端已经与该客户端连接，后续会保持监听其是否掉线。
auto MKCommunication::_client_connection_loop(std::string                     peer,
                                              NekoProto::ProtoStreamClient<> &client)
    -> ::ilias::Task<void>
{
    while (true) {
        if (auto ret = co_await client.recv(_flags); ret) {
            // 获得消息生成一个事件。收到来自客户端的消息。
            SPDLOG_INFO("recv {} from {}", ret.value().protoName(), _currentPeer->first);
            co_await _app->push_event(ClientMessage::emplaceProto(
                peer, std::make_shared<NekoProto::IProto>(std::move(ret.value()))));
        }
        else {
            if (ret.error() != Error::Canceled) {
                SPDLOG_ERROR("recv event failed {}", ret.error().message());
            }
            co_await _app->push_event(
                ClientDisconnected::emplaceProto(peer, ret.error().message()));
            break;
        }
    }
}

// 已客户端启动时，监听来自服务端的消息。
auto MKCommunication::_client_loop(NekoProto::ProtoStreamClient<> &client) -> ::ilias::Task<void>
{
    while (true) {
        // TODO:
        // 仅当为客户端时才可以挂起等待事件，考虑功能后续可能会支持服务端挂起所有客户，但目前这样做消息会混。
        if (auto ret = co_await client.recv(_flags); ret) {
            // 获得消息生成一个事件。
            co_await _app->push_event(std::move(ret.value()));
        }
        else {
            if (ret.error() != Error::Canceled) {
                SPDLOG_ERROR("recv event failed {}", ret.error().message());
                // TODO: 一旦出现异常就断开，此处应该增加异常状态的识别和重试处理。
                co_await _app->push_event(ClientControl::emplaceProto(
                    ClientControl::eStart, _ipEndpoint.address().toString(), _ipEndpoint.port()));
            }
            else {
                SPDLOG_INFO("client loop canceled");
            }
            break;
        }
    }
    if (_status == eClient) {
        _status = eEnable;
    }
    _protoStreamClients.clear();
    co_return;
}

/**
 * @brief 客户端握手流程
 *  - 上报软件信息，校队通信协议版本。 client --> server
 *  - 成功后上报屏幕信息
 */
auto MKCommunication::_client_handshake(std::string_view                peer,
                                        NekoProto::ProtoStreamClient<> *client)
    -> ::ilias::Task<int>
{
    auto hello = HelloEvent::emplaceProto(mks::IApp::app_name(), mks::IApp::app_version());
    if (auto ret = co_await client->send(hello, NekoProto::StreamFlag::VersionVerification); !ret) {
        SPDLOG_ERROR("send hello to {} failed {}", peer, ret.error().message());
        co_return -1;
    }

    auto screenInfo = VirtualScreenInfo::emplaceProto(_app->get_screen_info());
    if (auto ret = co_await client->send(screenInfo); !ret) {
        SPDLOG_ERROR("send screen info to {} failed {}", peer, ret.error().message());
        co_return -1;
    }
    co_return 0;
}

/**
 * @brief 服务端握手流程
 *  - 接收软件信息，校队通信协议版本。 server <-- client
 *  - 成功后接收屏幕信息
 * @param peer
 * @param client
 * @return ::ilias::Task<int>
 */
auto MKCommunication::_server_handshake(std::string_view                peer,
                                        NekoProto::ProtoStreamClient<> *client)
    -> ::ilias::Task<int>
{
    auto hello = co_await client->recv();
    if (!hello) {
        SPDLOG_ERROR("recv hello from {} failed {}", peer, hello.error().message());
        co_return -1;
    }
    std::string name, version;
    if (auto *proto = hello->cast<HelloEvent>(); proto == nullptr) {
        SPDLOG_ERROR("recv hello from {} failed {}", peer, "not hello event");
    }
    else {
        name    = proto->name;
        version = proto->version;
        SPDLOG_INFO("recv hello from {} {} - {}", peer, proto->name, proto->version);
    }

    auto screenInfo = co_await client->recv();
    if (!screenInfo) {
        SPDLOG_ERROR("recv screen info from {} failed {}", peer, screenInfo.error().message());
        co_return -1;
    }
    if (auto *proto = screenInfo->cast<VirtualScreenInfo>(); proto == nullptr) {
        SPDLOG_ERROR("recv screen info from {} failed {}", peer, "not screen info event");
        co_return -1;
    }
    else {
        SPDLOG_INFO("recv screen info from {} {}:({}x{} {})", peer, proto->name, proto->width,
                    proto->height, proto->screenId);
        auto clientConnected = ClientConnected::emplaceProto(std::string(peer), std::move(*proto));
        co_await _app->push_event(std::move(clientConnected));
        co_return 0;
    }
}

// client
/**
 * @brief connect to server
 * 如果需要ilias_go参数列表必须复制。
 * @param endpoint
 * @return Task<void>
 */
auto MKCommunication::connect(ilias::IPEndpoint endpoint) -> ilias::Task<int>
{
    if (_status == eDisable || _status == eServer) {
        SPDLOG_ERROR("Communication is server mode or disabled");
        co_return -1;
    }
    if (_protoStreamClients.size() > 0) { // 确保没有正在作为服务端运行
        SPDLOG_ERROR("server/client is already running");
        co_return -2;
    }
    TcpClient tcpClient;
    if (auto ret = co_await TcpClient::make(endpoint.family()); !ret) {
        SPDLOG_ERROR("TcpClient make failed {}", ret.error().message());
        co_return -3;
    }
    else {
        tcpClient = std::move(ret.value());
    }
    if (auto res = co_await tcpClient.connect(endpoint); !res) {
        SPDLOG_ERROR("TcpClient connect {} failed {}", endpoint.toString(), res.error().message());
        co_return -4;
    }
    auto client = NekoProto::ProtoStreamClient<>(_protofactory, std::move(tcpClient));
    if (auto ret = co_await _client_handshake("self", &client); ret != 0) {
        SPDLOG_ERROR("client handshake failed {}", ret);
        co_return -5;
    }
    _status      = eClient;
    _currentPeer = _protoStreamClients.emplace_hint(_protoStreamClients.begin(),
                                                    std::make_pair("self", std::move(client)));
    _taskScope.spawn(_client_loop(_currentPeer->second));
    if (dynamic_cast<ClientCommunication *>(_communicationWapper.get()) == nullptr) {
        _communicationWapper = std::make_unique<ClientCommunication>(this);
    }
    SPDLOG_INFO("connect to {}", endpoint.toString());
    co_return 0;
}

// 断开服务端连接
auto MKCommunication::close() -> Task<void>
{
    for (auto &item : _protoStreamClients) {
        co_await item.second.close();
    }
    _taskScope.cancel();
    co_await _taskScope;
    _protoStreamClients.clear();
    _currentPeer = _protoStreamClients.end();
    if (_status == eClient || _status == eServer) {
        _status = eEnable;
    }
}

auto MKCommunication::set_communication_options(
    [[maybe_unused]] const CommandInvoker::ArgsType    &args,
    [[maybe_unused]] const CommandInvoker::OptionsType &options) -> Task<std::string>
{
    auto it = options.find("thread");
    if (it != options.end() && std::get<bool>(it->second)) {
        _flags = _flags | NekoProto::StreamFlag::SerializerInThread;
    }
    else {
        _flags = (NekoProto::StreamFlag)((uint32_t)_flags &
                                         (~(uint32_t)NekoProto::StreamFlag::SerializerInThread));
    }
    co_return "";
}

auto MKCommunication::make(App &app) -> std::unique_ptr<MKCommunication, void (*)(NodeBase *)>
{
    std::unique_ptr<MKCommunication, void (*)(NodeBase *)> communication(
        new MKCommunication(&app),
        [](NodeBase *ptr) { delete static_cast<MKCommunication *>(ptr); });
    return communication;
}
MKS_END