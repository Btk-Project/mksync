/**
 * @file event_base.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-20
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <nekoproto/proto/proto_base.hpp>
#include <ilias/sync/event.hpp>
#include <ilias/sync/scope.hpp>

#include "mksync/base/ring_buffer.hpp"
#include "mksync/base/base_library.h"
#include "mksync/base/nodebase.hpp"

namespace mks::base
{
class MKS_BASE_API EventBase {
public:
    struct Event {
        NodeBase         *from;
        NekoProto::IProto data;
    };
    using EventQueue = RingBuffer<Event>;

        EventBase(std::size_t size = 100);
        ~EventBase();

    /**
     * @brief 尝试向事件队列中推入事件
     *
     * @param event 事件
     * @return true
     * @return false 当队列已满时会失败
     */
    auto try_push(Event &&event) -> bool;

    /**
     * @brief 向事件队列中推入事件
     * @note 当队列已满时会阻塞
     * @param event 事件
     * @return ilias::Task<::ilias::Error> 当收到取消操作时会返回错误，此时通常代表程序正在退出。
     */
    [[nodiscard("coroutine function")]]
    auto push(Event &&event) -> ilias::Task<::ilias::Error>;

    /**
     * @brief 尝试从事件队列中取出事件
     * 
     * @param event 事件
     * @return true 
     * @return false 
     */
    auto try_pop(Event &event) -> bool;

    /**
     * @brief 从事件队列中取出事件
     * @note 当队列为空时会阻塞
     * @param event 事件
     * @return ilias::Task<::ilias::Error> 当收到取消操作时会返回错误，此时通常代表程序正在退出。
     */
    [[nodiscard("coroutine function")]]
    auto pop(Event &event) -> ilias::Task<::ilias::Error>;

    /**
     * @brief 清空事件队列
     * 
     */
    void clear();

    /**
     * @brief 判断事件队列是否为空
     * 
     * @return true 
     * @return false 
     */
    auto empty() const -> bool;

    /**
     * @brief 获取队列大小
     * 
     * @return std::size_t 
     */
    auto size() const -> std::size_t;

    private:
        ::ilias::Event _emptySync;
        ::ilias::Event _fullSync;
        EventQueue     _queue;
    };
} // namespace mks::base