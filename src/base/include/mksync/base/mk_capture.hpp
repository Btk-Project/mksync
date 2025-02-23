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

namespace mks::base
{
    class App;
    class MKS_BASE_API MKCapture {
    public:
        MKCapture()                                = default;
        virtual ~MKCapture()                       = default;
        virtual auto start() -> ::ilias::Task<int> = 0;
        virtual auto stop() -> ::ilias::Task<int>  = 0;

        ///> 获取一个事件，如果没有就等待
        virtual auto get_event() -> ::ilias::Task<NekoProto::IProto> = 0;
        virtual auto notify() -> void                                = 0;
        static auto  make(App &app) -> std::unique_ptr<MKCapture>;
    };
} // namespace mks::base