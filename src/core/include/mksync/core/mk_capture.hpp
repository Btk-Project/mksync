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

#include "mksync/core/core_library.h"
#include "mksync/base/nodebase.hpp"

namespace mks::base
{
    class IApp;
    /**
     * @brief Capture node
     * 属于生产者节点。
     * 会产生MouseButtonEvent， MouseMotionEvent， MouseWheelEvent， KeyEvent， BorderEvent
     * 插件时注册的命令：
     *    capture:     启动键鼠捕获，该命令会截断系统的键鼠输入事件，并转换成MouseButtonEvent，
     * MouseMotionEvent， MouseWheelEvent， KeyEvent。 stopcapture:
     * 停止键鼠捕获，该命令会恢复系统的键鼠输入事件。并停止生产以上事件。
     * start节点后，该节点就会开始生产BorderEvent，该事件在鼠标靠近屏幕边界时产生。
     *
     */
    class MKS_CORE_API MKCapture : public NodeBase, public Producer {
    public:
        MKCapture(::ilias::IoContext *ctx) : _ctx(ctx) {};
        virtual ~MKCapture() = default;
        [[nodiscard("coroutine function")]]
        auto enable() -> ::ilias::Task<int> override;
        [[nodiscard("coroutine function")]]
        auto disable() -> ::ilias::Task<int> override;

        [[nodiscard("coroutine function")]]
        virtual auto start_capture() -> ::ilias::Task<int> = 0;
        [[nodiscard("coroutine function")]]
        virtual auto stop_capture() -> ::ilias::Task<int> = 0;

        static auto make(IApp *app) -> std::unique_ptr<MKCapture, void (*)(NodeBase *)>;

    private:
        ::ilias::IoContext *_ctx = nullptr;
    };
} // namespace mks::base