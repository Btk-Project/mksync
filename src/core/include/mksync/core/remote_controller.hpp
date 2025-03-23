/**
 * @file remote_controller.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-22
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <ilias/sync/scope.hpp>

#include "mksync/core/core_library.h"
#include "mksync/base/nodebase.hpp"

namespace mks::base
{
    class IApp;
    class RemoteControllerImp;

    class MKS_CORE_API RemoteController : public NodeBase {
    public:
        RemoteController(IApp *app);
        ~RemoteController();
        [[nodiscard("coroutine function")]]
        auto setup() -> ::ilias::Task<int> override;
        [[nodiscard("coroutine function")]]
        auto teardown() -> ::ilias::Task<int> override;
        auto name() -> const char * override;

    private:
        IApp                                *_app;
        std::unique_ptr<RemoteControllerImp> _imp;
        ::ilias::TaskScope                   _taskScop;
    };
} // namespace mks::base