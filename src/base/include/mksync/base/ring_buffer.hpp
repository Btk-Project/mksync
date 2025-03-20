/**
 * @file ring_buffer.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-22
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <memory>
#include <utility>

namespace mks::base
{
    /**
     * @brief ringbuffer
     * 一个循环队列，用于存储事件，为了避免没有消耗的鼠标键盘事件大量堆积，因此该队列会自动丢弃最早的事件。
     * @tparam T
     */
    template <typename T>
    class RingBuffer {
    public:
        RingBuffer(size_t size);
        ~RingBuffer();
        ///> 压入一个事件队尾
        auto push(const T &event) -> void;
        ///> move一个事件到队尾
        auto push(T &&event) -> void;
        ///> 就地构造一个事件到队尾
        template <typename... Args>
        auto emplace(Args &&...args) -> void;
        ///> 弹出一个队首的事件
        auto pop(T &event) -> bool;
        ///> 返回存在多少事件
        auto size() const -> std::size_t;
        auto empty() const -> bool;
        auto clear() -> void;
        auto capacity() const -> std::size_t;

    private:
        std::unique_ptr<T[]> _buffer   = nullptr;
        std::size_t          _head     = 0;
        std::size_t          _tail     = 0;
        std::size_t          _capacity = 0;
    };

    template <typename T>
    RingBuffer<T>::RingBuffer(size_t size)
    {
        _buffer.reset(new T[size + 1]);
        _capacity = size + 1;
    }

    template <typename T>
    RingBuffer<T>::~RingBuffer()
    {
    }

    template <typename T>
    auto RingBuffer<T>::push(const T &event) -> void
    {
        _buffer[_head] = event;
        _head          = (_head + 1) % _capacity;
        if (_head == _tail) {
            _tail = (_tail + 1) % _capacity;
        }
    }

    template <typename T>
    auto RingBuffer<T>::push(T &&event) -> void
    {
        _buffer[_head] = std::move(event);
        _head          = (_head + 1) % _capacity;
        if (_head == _tail) {
            _tail = (_tail + 1) % _capacity;
        }
    }

    template <typename T>
    template <typename... Args>
    auto RingBuffer<T>::emplace(Args &&...args) -> void
    {
        _buffer[_head] = std::move(T{std::forward<Args>(args)...});
        _head          = (_head + 1) % _capacity;
        if (_head == _tail) {
            _tail = (_tail + 1) % _capacity;
        }
    }

    template <typename T>
    auto RingBuffer<T>::pop(T &event) -> bool
    {
        if (_head == _tail) {
            return false;
        }
        event = std::move(_buffer[_tail]);
        _tail = (_tail + 1) % _capacity;
        return true;
    }

    template <typename T>
    auto RingBuffer<T>::size() const -> std::size_t
    {
        return (_head - _tail + _capacity) % _capacity;
    }

    template <typename T>
    auto RingBuffer<T>::empty() const -> bool
    {
        return _head == _tail;
    }

    template <typename T>
    auto RingBuffer<T>::clear() -> void
    {
        for (int i = _head; i < ((_tail > _head) ? _tail : _capacity); ++i) {
            _buffer[i] = T{};
        }
        for (int i = 0; i < ((_tail >= _head) ? 0 : _tail); ++i) {
            _buffer[i] = T{};
        }
        _head = _tail;
    }

    template <typename T>
    auto RingBuffer<T>::capacity() const -> std::size_t
    {
        return _capacity - 1;
    }

} // namespace mks::base