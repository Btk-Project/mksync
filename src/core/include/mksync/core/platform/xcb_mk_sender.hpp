/**
 * @file xcb_mk_sender.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-23
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#ifdef __linux__
    #include "mksync/proto/proto.hpp"
    #include "mksync/base/base_library.h"
    #include "mksync/core/mk_sender.hpp"

namespace mks::base
{
    class XcbConnect;
    class XcbWindow;
    class App;
    /**
     * @brief event sender
     * 系统事件构造对象，用于将接收的事件构造并发送给系统。
     *
     */
    class MKS_BASE_API XcbMKSender final : public MKSender {
    public:
        XcbMKSender(IApp *app);
        ~XcbMKSender();

        [[nodiscard("coroutine function")]]
        auto start_sender() -> ::ilias::Task<int> override;
        [[nodiscard("coroutine function")]]
        auto stop_sender() -> ::ilias::Task<int> override;
        auto name() -> const char * override;

        [[nodiscard("coroutine function")]]
        auto handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void> override;

    private:
        void _send_motion_event(const mks::MouseMotionEventConversion &event) const;
        void _send_button_event(const mks::MouseButtonEvent &event) const;
        void _send_wheel_event(const mks::MouseWheelEvent &event) const;
        void _send_keyboard_event(const mks::KeyboardEvent &event) const;

    private:
        bool                                       _isStart = false;
        std::unique_ptr<XcbConnect>                _xcbConnect;
        ::ilias::WaitHandle<::ilias::Result<void>> _eventLoopHandle;
        ::ilias::WaitHandle<::ilias::Result<void>> _xcbLoopHandle;
        int                                        _screenWidth  = 0;
        int                                        _screenHeight = 0;
    };
} // namespace mks::base
#endif