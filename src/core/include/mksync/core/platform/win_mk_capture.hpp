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
    #include <ilias/task.hpp>

    #include "mksync/base/ring_buffer.hpp"
    #include "mksync/core/mk_capture.hpp"
    #include "mksync/base/event_base.hpp"

MKS_BEGIN
class MKS_BASE_API WinMKCapture final : public MKCapture {
public:
    WinMKCapture(IApp *app);
    ~WinMKCapture();
    [[nodiscard("coroutine function")]]
    auto setup() -> ::ilias::Task<int> override;
    [[nodiscard("coroutine function")]]
    auto teardown() -> ::ilias::Task<int> override;
    auto name() -> const char * override;
    ///> 获取一个事件，如果没有就等待
    [[nodiscard("coroutine function")]]
    auto get_event() -> ::ilias::IoTask<NekoProto::IProto> override;
    auto push_event(NekoProto::IProto &&event) -> void;

    [[nodiscard("coroutine function")]]
    auto start_capture() -> ::ilias::Task<int> override;
    [[nodiscard("coroutine function")]]
    auto stop_capture() -> ::ilias::Task<int> override;

private:
    LRESULT _mouse_proc(int ncode, WPARAM wp, LPARAM lp);
    LRESULT _keyboard_proc(int ncode, WPARAM wp, LPARAM lp);

    HHOOK _mosueHook      = nullptr;
    HHOOK _keyboardHook   = nullptr;
    int   _screenWidth    = 0;
    int   _screenHeight   = 0;
    bool  _isStartCapture = false;
    bool  _isInBorder     = false;

    RingBuffer<NekoProto::IProto> _events;
    ::ilias::Event                _syncEvent;
    static WinMKCapture          *g_self;
};
MKS_END
#endif