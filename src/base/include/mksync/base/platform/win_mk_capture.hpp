/**
 * @file win_mk_capture.hpp
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
    #include <nekoproto/communication/communication_base.hpp>
    #include <ilias/sync/event.hpp>
    #include <ilias/task.hpp>

    #include "mksync/base/ring_buffer.hpp"
    #include "mksync/base/mk_capture.hpp"

namespace mks::base
{

    class MKS_BASE_API WinMKCapture final : public MKCapture {
    public:
        WinMKCapture();
        ~WinMKCapture();
        ///> 获取一个事件，如果没有就等待
        auto get_event() -> ILIAS_NAMESPACE::Task<NekoProto::IProto> override;
        auto notify() -> void override;

    private:
        LRESULT _mouse_proc(int ncode, WPARAM wp, LPARAM lp);
        LRESULT _keyboard_proc(int ncode, WPARAM wp, LPARAM lp);
        ///> 唤起正在等待事件的协程

        HHOOK    _mosueHook;
        HHOOK    _keyboardHook;
        uint64_t _screenWidth;
        uint64_t _screenHeight;

        RingBuffer<NekoProto::IProto> _events;
        ILIAS_NAMESPACE::Event        _syncEvent;
    };
} // namespace mks::base
#endif