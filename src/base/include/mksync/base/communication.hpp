/**
 * @file communication.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-26
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <nekoproto/communication/communication_base.hpp>

#include "mksync/base/base_library.h"
#include "mksync/base/nodebase.hpp"
#include "mksync/base/command_invoker.hpp"

#include <ilias/sync/event.hpp>
#include <unordered_map>

namespace mks::base
{
    class App;
    // 公共通信接口
    class MKS_BASE_API ICommunication {
    public:
        ICommunication()                                     = default;
        virtual ~ICommunication()                            = default;
        virtual auto declare_proto_to_send(int type) -> void = 0;
    };
    class MKS_BASE_API IServerCommunication : public ICommunication {
    public:
        IServerCommunication()          = default;
        virtual ~IServerCommunication() = default;
        virtual auto send(NekoProto::IProto &event, std::string_view peer)
            -> ilias::IoTask<void>                                                   = 0;
        virtual auto recv(std::string_view peer) -> ilias::IoTask<NekoProto::IProto> = 0;
        virtual auto peers() const -> std::vector<std::string>                       = 0;
    };
    class MKS_BASE_API IClientCommunication : public ICommunication {
    public:
        IClientCommunication()                                             = default;
        virtual ~IClientCommunication()                                    = default;
        virtual auto send(NekoProto::IProto &event) -> ilias::IoTask<void> = 0;
        virtual auto recv() -> ilias::IoTask<NekoProto::IProto>            = 0;
    };
    /**
     * @brief communication
     * - 核心通信节点
     *   - 作为服务器通信节点
     *      - 管理连接的客户节点
     *      - 发送/接收来自客户端的数据
     *   - 作为客户端通信节点
     *      - 发送/接收来自服务端的数据
     *   - 作为生产者/消费者
     *      - 接收来自网络的事件到本地
     *      - 发送本地事件到网络
     */
    class MKS_BASE_API MKCommunication : public NodeBase, public Consumer, public Producer {
    public:
        enum Status
        {
            eEnable,
            eDisable,
            eServer,
            eClient,
        };

    public:
        MKCommunication(App *app);
        virtual ~MKCommunication() = default;
        auto enable() -> ::ilias::Task<int> override;
        auto disable() -> ::ilias::Task<int> override;
        auto name() -> const char * override;
        auto get_subscribers() -> std::vector<int> override;
        auto handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void> override;
        auto get_event() -> ::ilias::IoTask<NekoProto::IProto> override;

        auto status() -> Status;
        auto set_flags(NekoProto::StreamFlag flags) -> void;
        auto add_subscribers(int type) -> void; // add event form send to current peer.
        auto remove_subscribers(int type) -> void;

        // server
        /**
         * @brief start server
         * 如果需要ilias_go参数列表必须复制。
         * @param endpoint
         * @return Task<void>
         */
        auto start_server(ilias::IPEndpoint endpoint) -> ilias::Task<void>;
        auto stop_server() -> void;

        // client
        /**
         * @brief connect to server
         * 如果需要ilias_go参数列表必须复制。
         * @param endpoint
         * @return Task<void>
         */
        auto connect_to(ilias::IPEndpoint endpoint) -> ilias::Task<void>;
        auto disconnect() -> void;

        // communication
        auto set_current_peer(std::string_view currentPeer) -> void;
        auto current_peer() const -> std::string;
        auto peers() const -> std::vector<std::string>;
        auto send(NekoProto::IProto &event, std::string_view peer) -> ilias::IoTask<void>;
        auto recv(std::string_view peer) -> ilias::IoTask<NekoProto::IProto>;
        auto set_communication_options(const CommandInvoker::ArgsType    &args,
                                       const CommandInvoker::OptionsType &options) -> std::string;

        // 作为服务端或客户端开启后才能获取，关闭或重启节点都会导致指针失效。
        auto get_communication() -> ICommunication *;

        static auto make(App &app) -> std::unique_ptr<MKCommunication, void (*)(NodeBase *)>;

    private:
        auto _server_loop(::ilias::TcpListener tcplistener) -> ::ilias::Task<void>;

    private:
        NekoProto::ProtoFactory _protofactory;
        ::ilias::Event          _syncEvent;
        ::ilias::CancelHandle   _cancelHandle = {};
        Status                  _status       = eDisable;
        App                    *_app          = nullptr;
        NekoProto::StreamFlag   _flags        = NekoProto::StreamFlag::None;
        std::unordered_map<std::string, NekoProto::ProtoStreamClient<>> _protoStreamClients;
        std::unordered_map<std::string, NekoProto::ProtoStreamClient<>>::iterator _currentPeer;
        std::unique_ptr<ICommunication> _communicationWapper; // 通信封装
    };
} // namespace mks::base