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
        MKCommunication(::ilias::IoContext *ctx);
        virtual ~MKCommunication() = default;
        auto start() -> ::ilias::Task<int> override;
        auto stop() -> ::ilias::Task<int> override;
        auto name() -> const char * override;
        auto get_subscribers() -> std::vector<int> override;
        auto handle_event(NekoProto::IProto &event) -> ::ilias::Task<void> override;
        auto get_event() -> ::ilias::IoTask<NekoProto::IProto> override;

        auto status() -> Status;
        auto set_flags(NekoProto::StreamFlag flags) -> void;
        auto add_subscribers(int type) -> void; // add event form send to current peer.

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

        static auto make(App &app) -> std::unique_ptr<MKCommunication, void (*)(NodeBase *)>;

    private:
        auto _server_loop(::ilias::TcpListener tcplistener) -> ::ilias::Task<void>;

    private:
        NekoProto::ProtoFactory _protofactory;
        ::ilias::Event          _syncEvent;
        ::ilias::CancelHandle   _cancelHandle = {};
        Status                  _status       = eDisable;
        ::ilias::IoContext     *_ctx          = nullptr;
        NekoProto::StreamFlag   _flags        = NekoProto::StreamFlag::None;
        std::set<int>           _subscribers;
        std::unordered_map<std::string, NekoProto::ProtoStreamClient<>> _protoStreamClients;
        std::unordered_map<std::string, NekoProto::ProtoStreamClient<>>::iterator _currentPeer;
    };
} // namespace mks::base