/**
 * @file win_mk_sender.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-22
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#ifdef _WIN32
    #include "mksync/proto/proto.hpp"
    #include "mksync/base/base_library.h"
    #include "mksync/base/mk_sender.hpp"

namespace mks::base
{
    /**
     * @brief event sender
     * 系统事件构造对象，用于将接收的事件构造并发送给系统。
     *
     */
    class MKS_BASE_API WinMKSender final : public MKSender {
    public:
        WinMKSender(::ilias::IoContext *ctx);
        ~WinMKSender();

        auto start_sender() -> ::ilias::Task<int> override;
        auto stop_sender() -> ::ilias::Task<int> override;
        auto name() -> const char * override;

        auto get_subscribers() -> std::vector<int> override;
        auto handle_event(NekoProto::IProto &event) -> ::ilias::Task<void> override;

    private:
        void _send_motion_event(const mks::MouseMotionEvent &event) const;
        void _send_button_event(const mks::MouseButtonEvent &event) const;
        void _send_wheel_event(const mks::MouseWheelEvent &event) const;
        void _send_keyboard_event(const mks::KeyEvent &event) const;

    private:
        bool _isStart = false;
    };
} // namespace mks::base
#endif