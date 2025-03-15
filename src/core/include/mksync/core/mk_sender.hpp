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

#include "mksync/core/core_library.h"
#include "mksync/base/nodebase.hpp"
#include "mksync/base/command_invoker.hpp"

namespace mks::base
{
    class App;
    /**
     * @brief event sender
     * 系统事件构造对象，用于将接收的事件构造并发送给系统。
     *
     */
    class MKS_CORE_API MKSender : public NodeBase, public Consumer {
    public:
        MKSender(::ilias::IoContext *ctx) : _ctx(ctx) {}
        virtual ~MKSender() = default;
        [[nodiscard("coroutine function")]]
        auto enable() -> ::ilias::Task<int> override;
        [[nodiscard("coroutine function")]]
        auto disable() -> ::ilias::Task<int> override;

        [[nodiscard("coroutine function")]]
        virtual auto start_sender() -> ::ilias::Task<int> = 0;
        [[nodiscard("coroutine function")]]
        virtual auto stop_sender() -> ::ilias::Task<int> = 0;

        static auto make(App &app) -> std::unique_ptr<MKSender, void (*)(NodeBase *)>;

    private:
        ::ilias::IoContext *_ctx = nullptr;
    };
} // namespace mks::base