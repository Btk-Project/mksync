#include "mksync/base/event_base.hpp"

namespace mks::base
{
    EventBase::EventBase(std::size_t size) : _queue(size) {}

    EventBase::~EventBase()
    {
        _queue.clear();
    }

    auto EventBase::try_push(Event &&event) -> bool
    {
        ILIAS_ASSERT(event.data != nullptr);
        if (_queue.size() >= _queue.capacity()) {
            return false;
        }
        _queue.emplace(std::forward<Event>(event));
        _emptySync.set();
        return true;
    }

    auto EventBase::push(Event &&event) -> ilias::Task<::ilias::Error>
    {
        ILIAS_ASSERT(event.data != nullptr);
        while (_queue.size() >= _queue.capacity()) {
            _fullSync.clear();
            if (auto ret = co_await _fullSync; !ret) {
                co_return ret.error();
            }
        }
        _queue.emplace(std::forward<Event>(event));
        _emptySync.set();
        co_return ::ilias::Error::Ok;
    }

    auto EventBase::try_pop(Event &event) -> bool
    {
        if (_queue.pop(event)) {
            _fullSync.set();
            return true;
        }
        return false;
    }

    auto EventBase::pop(Event &event) -> ilias::Task<::ilias::Error>
    {
        while (!_queue.pop(event)) {
            _emptySync.clear();
            if (auto ret = co_await _emptySync; !ret) {
                co_return ret.error();
            }
        }
        _fullSync.set();
        co_return ilias::Error::Ok;
    }

    void EventBase::clear()
    {
        _queue.clear();
        _fullSync.set();
    }

    auto EventBase::empty() const -> bool
    {
        return _queue.empty();
    }

    auto EventBase::size() const -> std::size_t
    {
        return _queue.size();
    }
} // namespace mks::base