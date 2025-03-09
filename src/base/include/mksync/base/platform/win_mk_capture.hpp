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
        WinMKCapture(::ilias::IoContext *ctx);
        ~WinMKCapture();
        auto start() -> ::ilias::Task<int> override;
        auto stop() -> ::ilias::Task<int> override;
        auto name() -> const char * override;
        ///> 获取一个事件，如果没有就等待
        auto get_event() -> ::ilias::IoTask<NekoProto::IProto> override;
        ///> 唤起正在等待事件的协程并弹出一个事件，如果事件队列为空则会弹出一个空事件。
        auto notify() -> void;

        auto start_capture() -> ::ilias::Task<int> override;
        auto stop_capture() -> ::ilias::Task<int> override;

    private:
        LRESULT _mouse_proc(int ncode, WPARAM wp, LPARAM lp);
        LRESULT _keyboard_proc(int ncode, WPARAM wp, LPARAM lp);

        HHOOK _mosueHook      = nullptr;
        HHOOK _keyboardHook   = nullptr;
        int   _screenWidth    = 0;
        int   _screenHeight   = 0;
        bool  _isStartCapture = false;
        int   _posX           = 0;
        int   _posY           = 0;

        RingBuffer<NekoProto::IProto> _events;
        ::ilias::Event                _syncEvent;
        static WinMKCapture          *g_self;
    };
} // namespace mks::base
#endif