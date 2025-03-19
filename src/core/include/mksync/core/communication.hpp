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

#include "mksync/core/core_library.h"
#include "mksync/base/nodebase.hpp"
#include "mksync/base/command_invoker.hpp"
#include "mksync/base/ring_buffer.hpp"
#include "mksync/base/communication.hpp"

#include <ilias/sync/event.hpp>
#include <ilias/sync/scope.hpp>
#include <map>

namespace mks::base
{
    class App;
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
    class MKS_CORE_API MKCommunication : public NodeBase, public Consumer, public Producer {
    public:
        enum Status
        {
            eEnable,
            eDisable,
            eServer,
            eClient,
        };

    public:
        MKCommunication(IApp *app);
        virtual ~MKCommunication();
        [[nodiscard("coroutine function")]]
        auto setup() -> ::ilias::Task<int> override;
        [[nodiscard("coroutine function")]]
        auto teardown() -> ::ilias::Task<int> override;
        auto name() -> const char * override;
        auto get_subscribes() -> std::vector<int> override;
        [[nodiscard("coroutine function")]]
        auto handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void> override;
        [[nodiscard("coroutine function")]]
        auto get_event() -> ::ilias::IoTask<NekoProto::IProto> override;
        auto pust_event(NekoProto::IProto &&event) -> void;

        auto status() -> Status;
        auto set_flags(NekoProto::StreamFlag flags) -> void;
        auto add_subscribe(int type) -> void; // add event form send to current peer.
        auto remove_subscribers(int type) -> void;

        // server
        /**
         * @brief server
         * 如果需要ilias_go参数列表必须复制。
         * @param endpoint
         * @return Task<void>
         */
        [[nodiscard("coroutine function")]]
        auto listen(ilias::IPEndpoint endpoint) -> ilias::Task<int>;

        // client
        /**
         * @brief connect to server
         * 如果需要ilias_go参数列表必须复制。
         * @param endpoint
         * @return Task<void>
         */
        [[nodiscard("coroutine function")]]
        auto connect(ilias::IPEndpoint endpoint) -> ilias::Task<int>;

        [[nodiscard("coroutine function")]]
        auto close() -> ilias::Task<void>;
        // communication
        auto set_current_peer(std::string_view currentPeer) -> void;
        auto current_peer() const -> std::string;
        auto peers() const -> std::vector<std::string>;
        [[nodiscard("coroutine function")]]
        auto send(NekoProto::IProto &event, std::string_view peer) -> ilias::IoTask<void>;
        [[nodiscard("coroutine function")]]
        auto recv(std::string_view peer) -> ilias::IoTask<NekoProto::IProto>;
        [[nodiscard("coroutine function")]]
        auto set_communication_options(const CommandInvoker::ArgsType    &args,
                                       const CommandInvoker::OptionsType &options)
            -> ::ilias::Task<std::string>;

        // 作为服务端或客户端开启后才能获取，关闭或重启节点都会导致指针失效。
        auto get_communication() -> ICommunication *;

        static auto make(App &app) -> std::unique_ptr<MKCommunication, void (*)(NodeBase *)>;

    private:
        [[nodiscard("coroutine function")]]
        auto _server_loop(::ilias::TcpListener tcplistener) -> ::ilias::Task<void>;
        [[nodiscard("coroutine function")]]
        auto _client_connection_loop(std::string peer, NekoProto::ProtoStreamClient<> &client)
            -> ::ilias::Task<void>;
        [[nodiscard("coroutine function")]]
        auto _client_loop(NekoProto::ProtoStreamClient<> &client) -> ::ilias::Task<void>;
        [[nodiscard("coroutine function")]]
        auto _client_handshake(std::string_view peer, NekoProto::ProtoStreamClient<> *client)
            -> ::ilias::Task<int>;
        [[nodiscard("coroutine function")]]
        auto _server_handshake(std::string_view peer, NekoProto::ProtoStreamClient<> *client)
            -> ::ilias::Task<int>;

    private:
        IApp                         *_app = nullptr;
        NekoProto::ProtoFactory       _protofactory;
        RingBuffer<NekoProto::IProto> _events;
        ::ilias::Event                _syncEvent;
        ::ilias::TaskScope            _taskScope;
        Status                        _status = eDisable;
        NekoProto::StreamFlag         _flags  = NekoProto::StreamFlag::None;
        ilias::IPEndpoint             _ipEndpoint;
        std::map<std::string, NekoProto::ProtoStreamClient<>, std::less<>> _protoStreamClients;
        std::map<std::string, NekoProto::ProtoStreamClient<>, std::less<>>::iterator _currentPeer;
        std::unique_ptr<ICommunication> _communicationWapper; // 通信封装
    };
} // namespace mks::base