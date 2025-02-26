/**
 * @file event_listener.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-22
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <nekoproto/communication/communication_base.hpp>

#include "mksync/base/base_library.h"
#include "mksync/base/nodebase.hpp"
#include "mksync/base/command_parser.hpp"

namespace mks::base
{
    class App;
    class MKS_BASE_API MKCapture : public NodeBase, public Producer {
    public:
        MKCapture(::ilias::IoContext *ctx) : _ctx(ctx) {};
        virtual ~MKCapture() = default;
        auto start() -> ::ilias::Task<int> override;
        auto stop() -> ::ilias::Task<int> override;

        virtual auto start_capture() -> ::ilias::Task<int> = 0;
        virtual auto stop_capture() -> ::ilias::Task<int>  = 0;
        auto         start_capture(const CommandParser::ArgsType    &args,
                                   const CommandParser::OptionsType &options) -> std::string;
        auto         stop_capture(const CommandParser::ArgsType    &args,
                                  const CommandParser::OptionsType &options) -> std::string;

        static auto make(App &app) -> std::unique_ptr<MKCapture, void (*)(NodeBase *)>;

    private:
        bool                _isEnable = false;
        ::ilias::IoContext *_ctx      = nullptr;
    };
} // namespace mks::base