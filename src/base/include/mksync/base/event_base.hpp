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

        auto try_push(Event &&event) -> bool;
        [[nodiscard("coroutine function")]]
        auto push(Event &&event) -> ilias::Task<::ilias::Error>;
        auto try_pop(Event &event) -> bool;
        [[nodiscard("coroutine function")]]
        auto pop(Event &event) -> ilias::Task<::ilias::Error>;
        void clear();
        auto empty() const -> bool;
        auto size() const -> std::size_t;

    private:
        ::ilias::Event _emptySync;
        ::ilias::Event _fullSync;
        EventQueue     _queue;
    };
} // namespace mks::base