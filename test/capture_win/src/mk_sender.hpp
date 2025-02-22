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

/**
 * @brief event sender
 * 系统事件构造对象，用于将接收的事件构造并发送给系统。
 *
 */
class MKSender {
public:
    MKSender();
    ~MKSender();

    void send_motion_event(const mks::MouseMotionEvent &event) const;
    void send_button_event(const mks::MouseButtonEvent &event) const;
    void send_wheel_event(const mks::MouseWheelEvent &event) const;
    void send_keyboard_event(const mks::KeyEvent &event) const;
};
