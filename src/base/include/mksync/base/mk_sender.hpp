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

#include "mksync/proto/proto.hpp"
#include "mksync/base/base_library.h"

namespace mks::base
{

    /**
     * @brief event sender
     * 系统事件构造对象，用于将接收的事件构造并发送给系统。
     *
     */
    class MKS_BASE_API MKSender {
    public:
        MKSender()          = default;
        virtual ~MKSender() = default;

        virtual void send_motion_event(const mks::MouseMotionEvent &event) const = 0;
        virtual void send_button_event(const mks::MouseButtonEvent &event) const = 0;
        virtual void send_wheel_event(const mks::MouseWheelEvent &event) const   = 0;
        virtual void send_keyboard_event(const mks::KeyEvent &event) const       = 0;
        static auto  make() -> std::unique_ptr<MKSender>;
    };
} // namespace mks::base