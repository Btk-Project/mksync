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
#include "mksync/base/command_parser.hpp"

#include <ilias/sync/event.hpp>

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
        MKCommunication(::ilias::IoContext *ctx) : _ctx(ctx) {}
        virtual ~MKCommunication() = default;
        auto start() -> ::ilias::Task<int> override;
        auto stop() -> ::ilias::Task<int> override;
        auto name() -> const char * override;
        auto get_subscribers() -> std::vector<int> override;
        auto handle_event(NekoProto::IProto &event) -> ::ilias::Task<void> override;
        auto get_event() -> ::ilias::IoTask<NekoProto::IProto> override;

        auto status() -> Status;
        auto set_flags(NekoProto::StreamFlag flags) -> void;

        // server
        /**
         * @brief start server
         * 如果需要ilias_go参数列表必须复制。
         * @param endpoint
         * @return Task<void>
         */
        auto start_server(ilias::IPEndpoint endpoint) -> ilias::Task<void>;
        auto stop_server() -> void;
        auto start_server(const CommandParser::ArgsType    &args,
                          const CommandParser::OptionsType &options) -> std::string;
        auto stop_server(const CommandParser::ArgsType    &args,
                         const CommandParser::OptionsType &options) -> std::string;

        // client
        /**
         * @brief connect to server
         * 如果需要ilias_go参数列表必须复制。
         * @param endpoint
         * @return Task<void>
         */
        auto connect_to(ilias::IPEndpoint endpoint) -> ilias::Task<void>;
        auto disconnect() -> void;
        auto connect_to(const CommandParser::ArgsType    &args,
                        const CommandParser::OptionsType &options) -> std::string;
        auto disconnect(const CommandParser::ArgsType    &args,
                        const CommandParser::OptionsType &options) -> std::string;

        // communication
        auto set_communication_options(const CommandParser::ArgsType    &args,
                                       const CommandParser::OptionsType &options) -> std::string;

        static auto make(App &app) -> std::unique_ptr<MKCommunication, void (*)(NodeBase *)>;

    private:
        std::unique_ptr<NekoProto::ProtoStreamClient<>> _protoStreamClient;
        NekoProto::ProtoFactory                         _protofactory;
        ilias::CancelHandle                             _cancelHandle;
        ::ilias::Event                                  _syncEvent;
        Status                                          _status = eDisable;
        NekoProto::StreamFlag                           _flags  = NekoProto::StreamFlag::None;
        ::ilias::IoContext                             *_ctx    = nullptr;
    };
} // namespace mks::base