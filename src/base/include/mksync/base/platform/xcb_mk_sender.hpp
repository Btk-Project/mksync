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
    #include "mksync/base/mk_sender.hpp"

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
        XcbMKSender(App &app);
        ~XcbMKSender();

        auto start() -> ::ilias::Task<int> override;
        auto stop() -> ::ilias::Task<int> override;
        void send_motion_event(const mks::MouseMotionEvent &event) const override;
        void send_button_event(const mks::MouseButtonEvent &event) const override;
        void send_wheel_event(const mks::MouseWheelEvent &event) const override;
        void send_keyboard_event(const mks::KeyEvent &event) const override;

    private:
        bool                                       _isStart = false;
        App                                       *_app     = nullptr;
        std::unique_ptr<XcbConnect>                _xcbConnect;
        ::ilias::WaitHandle<::ilias::Result<void>> _eventLoopHandle;
    };
} // namespace mks::base
#endif