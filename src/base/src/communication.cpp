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
        auto             protomap = NekoProto::ProtoFactory::protoTypeMap();
        std::vector<int> subscribers;
        for (auto &proto : protomap) {
            subscribers.push_back(proto.second);
        }
        return subscribers;
    }

    auto MKCommunication::handle_event(NekoProto::IProto &event) -> ::ilias::Task<void>
    {
        if (_protoStreamClient) {
            SPDLOG_INFO("send {}", event.protoName());
            auto ret = co_await _protoStreamClient->send(event, _flags);
            if (!ret) {
                SPDLOG_ERROR("send event failed {}", ret.error().message());
            }
        }
    }

    auto MKCommunication::get_event() -> ::ilias::IoTask<NekoProto::IProto>
    {
        if (!_protoStreamClient) {
            auto ret = co_await _syncEvent;
            if (!ret) {
                co_return Unexpected<Error>(ret.error());
            }
        }
        if (_protoStreamClient) {
            auto ret = co_await _protoStreamClient->recv(_flags);
            SPDLOG_INFO("recv {}", ret->protoName());
            co_return ret;
        }
        co_return ilias::Unexpected<ilias::Error>(ilias::Error::InvalidArgument);
    }

    auto MKCommunication::status() -> Status
    {
        return _status;
    }

    auto MKCommunication::set_flags(NekoProto::StreamFlag flags) -> void
    {
        _flags = flags;
    }

    auto MKCommunication::start_server(ilias::IPEndpoint endpoint) -> ilias::Task<void>
    {
        if (_status == eDisable || _status == eClient) {
            SPDLOG_ERROR("{} is disabled", name());
            co_return;
        }
        if (_protoStreamClient) { // 确保没有正在作为服务端运行
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
        SPDLOG_INFO("start server with {}", endpoint.toString());
        if (auto res = server.bind(endpoint); !res) {
            SPDLOG_ERROR("TcpListener::bind failed {}", res.error().message());
            server.close();
            co_return;
        }
        _status = eServer;
        while (true) {
            if (auto ret1 = co_await server.accept(); !ret1) {
                SPDLOG_ERROR("TcpListener::accept failed {}", ret1.error().message());
                server.close();
                stop_server();
                co_return;
            }
            else {
                // TODO:增加处理多个连接的请求。
                auto tcpClient     = std::move(ret1.value());
                _protoStreamClient = std::make_unique<NekoProto::ProtoStreamClient<>>(
                    _protofactory, std::move(tcpClient.first));
                _syncEvent.set();
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
        }
        if (_protoStreamClient) {
            _protoStreamClient.reset();
        }
        _status = eEnable;
    }
    auto MKCommunication::start_server(const CommandParser::ArgsType    &args,
                                       const CommandParser::OptionsType &options) -> std::string
    {
        IPEndpoint ipendpoint("127.0.0.1:12345");
        if (args.size() == 2) {
            if (auto ret = IPEndpoint::fromString(args[0] + ":" + args[1]); ret) {
                ipendpoint = ret.value();
            }
            else {
                SPDLOG_ERROR("ip endpoint error {}", ret.error().message());
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
            auto address = it == options.end() ? "127.0.0.1" : it->second;
            it           = options.find("port");
            auto port    = it == options.end() ? "12345" : it->second;
            auto ret     = IPEndpoint::fromString(address + ":" + port);
            if (ret) {
                ipendpoint = ret.value();
            }
            else {
                SPDLOG_ERROR("ip endpoint error {}", ret.error().message());
            }
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
        if (_protoStreamClient) { // 确保没有正在作为服务端运行
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
        _protoStreamClient = // 构造用于传输协议的壳
            std::make_unique<NekoProto::ProtoStreamClient<>>(_protofactory, std::move(tcpClient));
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
        if (_protoStreamClient) {
            _protoStreamClient.reset();
        }
        _status = eEnable;
    }

    auto MKCommunication::connect_to(const CommandParser::ArgsType    &args,
                                     const CommandParser::OptionsType &options) -> std::string
    {
        if (args.size() == 2) {
            connect_to(IPEndpoint(args[0] + ":" + args[1])).wait();
        }
        else if (args.size() == 1) {
            connect_to(IPEndpoint(args[0])).wait();
        }
        else {
            auto it      = options.find("address");
            auto address = it == options.end() ? "127.0.0.1" : it->second;
            it           = options.find("port");
            auto port    = it == options.end() ? "12345" : it->second;
            connect_to(IPEndpoint(address + ":" + port)).wait();
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
        const CommandParser::ArgsType                     &args,
        [[maybe_unused]] const CommandParser::OptionsType &options) -> std::string
    {
        if (args.empty()) {
            return "please usage: thread <enabled/disabled>";
        }
        if (args[0] == "enabled") {
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
            {"listen", "l"},
            "listen server, e.g. listen 127.0.0.1 12345",
            std::bind(static_cast<CallbackType>(&MKCommunication::start_server),
                      communication.get(), std::placeholders::_1, std::placeholders::_2)
        });
        commandInstaller(
            {{"stop_listen"},
             "stop the server",
             std::bind(static_cast<CallbackType>(&MKCommunication::stop_server),
                       communication.get(), std::placeholders::_1, std::placeholders::_2)});
        // 注册连接到服务器命令
        commandInstaller(
            {{"connect"},
             "connect to server, e.g. connect 127.0.0.1 12345",
             std::bind(static_cast<CallbackType>(&MKCommunication::connect_to), communication.get(),
                       std::placeholders::_1, std::placeholders::_2)});
        commandInstaller({
            {"disconnect", "dcon"},
            "disconnect from server",
            std::bind(static_cast<CallbackType>(&MKCommunication::disconnect), communication.get(),
                      std::placeholders::_1, std::placeholders::_2)
        });
        commandInstaller({
            {"thread", "t"},
            "<enable/disable> to enable independent thread for serialization",
            std::bind(static_cast<CallbackType>(&MKCommunication::set_communication_options),
                      communication.get(), std::placeholders::_1, std::placeholders::_2)
        });
        return communication;
    }

} // namespace mks::base