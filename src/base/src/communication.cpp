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

    auto MKCommunication::send(NekoProto::IProto &event,
                               std::string_view   peer) -> ilias::IoTask<void>
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
                    disconnect();
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
        while (true) {
            if (auto ret1 = co_await server.accept(); !ret1) {
                if (ret1.error() != Error::Canceled) {
                    SPDLOG_ERROR("TcpListener::accept failed {}", ret1.error().message());
                }
                server.close();
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
#if 1
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
    auto MKCommunication::start_server(const CommandParser::ArgsType    &args,
                                       const CommandParser::OptionsType &options) -> std::string
    {
        IPEndpoint ipendpoint("127.0.0.1:12345");
        if (args.size() == 2) {
            if (auto ret =
                    IPEndpoint::fromString(std::string(args[0]) + ":" + std::string(args[1]));
                ret) {
                ipendpoint = ret.value();
            }
            else {
                SPDLOG_ERROR("ip endpoint {}:{} error {}", args[0], args[1], ret.error().message());
            }
        }
        else if (args.size() == 1) {
            if (auto ret = IPEndpoint::fromString(args[0]); ret) {
                ipendpoint = ret.value();
            }
            else {
                SPDLOG_ERROR("ip endpoint error {}", ret.error().message());
            }
        }
        else {
            auto it      = options.find("address");
            auto address = it == options.end() ? "127.0.0.1" : std::get<std::string>(it->second);
            it           = options.find("port");
            auto port    = it == options.end() ? 12345 : std::get<int>(it->second);
            auto ret     = IPEndpoint(::ilias::IPAddress(address.c_str()), port);
            if (ret.isValid()) {
                ipendpoint = ret;
            }
            else {
                SPDLOG_ERROR("ip endpoint error {}:{}", address, port);
            }
        }
        if (_status == eServer) { // 确保没有正在作为服务端运行
            SPDLOG_ERROR("server/client is already running");
            return "";
        }
        if (_cancelHandle) {
            _cancelHandle.cancel();
            _cancelHandle = {};
        }
        _cancelHandle = ::ilias::spawn(*_ctx, start_server(ipendpoint));
        return "";
    }

    auto MKCommunication::stop_server([[maybe_unused]] const CommandParser::ArgsType    &args,
                                      [[maybe_unused]] const CommandParser::OptionsType &options)
        -> std::string
    {
        stop_server();
        return "";
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
            SPDLOG_ERROR("Communication is disabled");
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

    auto MKCommunication::connect_to(const CommandParser::ArgsType    &args,
                                     const CommandParser::OptionsType &options) -> std::string
    {
        if (args.size() == 2) {
            connect_to(IPEndpoint(std::string(args[0]) + ":" + std::string(args[1]))).wait();
        }
        else if (args.size() == 1) {
            connect_to(IPEndpoint(args[0])).wait();
        }
        else {
            auto it      = options.find("address");
            auto address = it == options.end() ? "127.0.0.1" : std::get<std::string>(it->second);
            it           = options.find("port");
            auto port    = it == options.end() ? 12345 : std::get<int>(it->second);
            connect_to(IPEndpoint(ilias::IPAddress(address.c_str()), port)).wait();
        }
        return "";
    }

    auto MKCommunication::disconnect([[maybe_unused]] const CommandParser::ArgsType    &args,
                                     [[maybe_unused]] const CommandParser::OptionsType &options)
        -> std::string
    {
        disconnect();
        return "";
    }

    auto MKCommunication::set_communication_options(
        [[maybe_unused]] const CommandParser::ArgsType    &args,
        [[maybe_unused]] const CommandParser::OptionsType &options) -> std::string
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
        using CallbackType = std::string (MKCommunication::*)(const CommandParser::ArgsType &,
                                                              const CommandParser::OptionsType &);
        std::unique_ptr<MKCommunication, void (*)(NodeBase *)> communication(
            new MKCommunication(app.get_io_context()),
            [](NodeBase *ptr) { delete static_cast<MKCommunication *>(ptr); });
        auto commandInstaller = app.command_installer(communication->name());
        // 注册服务器监听相关命令
        commandInstaller({
            {"listen",                                                           "l"},
            "listen server, e.g. listen 127.0.0.1 12345",
            std::bind(static_cast<CallbackType>(&MKCommunication::start_server),
                      communication.get(), std::placeholders::_1, std::placeholders::_2),
            {{"address", CommandParser::OptionsData::eString, "lister address"},
             {"port", CommandParser::OptionsData::eInt, "lister port"}              }
        });
        commandInstaller(
            {{"stop_listen"},
             "stop the server",
             std::bind(static_cast<CallbackType>(&MKCommunication::stop_server),
                       communication.get(), std::placeholders::_1, std::placeholders::_2),
             {}});
        // 注册连接到服务器命令
        commandInstaller({
            {"connect"},
            "connect to server, e.g. connect 127.0.0.1 12345",
            std::bind(static_cast<CallbackType>(&MKCommunication::connect_to), communication.get(),
                      std::placeholders::_1, std::placeholders::_2),
            {{"address", CommandParser::OptionsData::eString, "server address"},
             {"port", CommandParser::OptionsData::eInt, "server port"}}
        });
        commandInstaller(
            {{"disconnect"},
             "disconnect from server",
             std::bind(static_cast<CallbackType>(&MKCommunication::disconnect), communication.get(),
                       std::placeholders::_1, std::placeholders::_2),
             {}});
        commandInstaller({
            {"communication_attributes", "comm_attr"},
            "set communication attributes",
            std::bind(static_cast<CallbackType>(&MKCommunication::set_communication_options),
                      communication.get(), std::placeholders::_1, std::placeholders::_2),
            {
             {"thread", CommandParser::OptionsData::eBool,
                 "enable independent thread for serialization"},
             }
        });
        return communication;
    }

} // namespace mks::base