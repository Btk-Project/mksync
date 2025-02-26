/**
 * @file mk_sender.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-22
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <ilias/task.hpp>

#include "mksync/proto/proto.hpp"
#include "mksync/base/base_library.h"
#include "mksync/base/nodebase.hpp"
#include "mksync/base/command_parser.hpp"

namespace mks::base
{
    class App;
    /**
     * @brief event sender
     * 系统事件构造对象，用于将接收的事件构造并发送给系统。
     *
     */
    class MKS_BASE_API MKSender : public NodeBase, public Consumer {
    public:
        MKSender(::ilias::IoContext *ctx) : _ctx(ctx) {}
        virtual ~MKSender() = default;
        auto start() -> ::ilias::Task<int> override;
        auto stop() -> ::ilias::Task<int> override;

        virtual auto start_sender() -> ::ilias::Task<int> = 0;
        virtual auto stop_sender() -> ::ilias::Task<int>  = 0;
        auto         start_sender(const CommandParser::ArgsType    &args,
                                  const CommandParser::OptionsType &options) -> std::string;
        auto         stop_sender(const CommandParser::ArgsType    &args,
                                 const CommandParser::OptionsType &options) -> std::string;

        static auto make(App &app) -> std::unique_ptr<MKSender, void (*)(NodeBase *)>;

    private:
        bool                _isEnable = false;
        ::ilias::IoContext *_ctx      = nullptr;
    };
} // namespace mks::base