/**
 * @file xcb_mk_capture.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-23
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#ifdef __linux__
    #include <ilias/sync/event.hpp>

    #include "mksync/core/mk_capture.hpp"
    #include "mksync/base/base_library.h"
    #include "mksync/base/ring_buffer.hpp"
    #include "mksync/proto/proto.hpp"

namespace mks::base
{
    class XcbConnect;
    class XcbWindow;
    class App;

    class MKS_BASE_API XcbMKCapture final : public MKCapture {
    public:
        XcbMKCapture(App &app);
        ~XcbMKCapture();

        [[nodiscard("coroutine function")]]
        auto enable() -> ::ilias::Task<int> override;
        [[nodiscard("coroutine function")]]
        auto disable() -> ::ilias::Task<int> override;
        [[nodiscard("coroutine function")]]
        auto start_capture() -> ::ilias::Task<int> override;
        [[nodiscard("coroutine function")]]
        auto stop_capture() -> ::ilias::Task<int> override;

        auto name() -> const char * override;
        ///> 获取一个事件，如果没有就等待
        [[nodiscard("coroutine function")]]
        auto get_event() -> ::ilias::IoTask<NekoProto::IProto> override;

        auto pointer_proc(void *event) -> void;
        auto keyboard_proc(void *event) -> void;
        auto window_proc(void *event) -> void;

    private:
        App                                       *_app = nullptr;
        RingBuffer<NekoProto::IProto>              _events;
        std::unique_ptr<XcbConnect>                _xcbConnect;
        std::unique_ptr<XcbWindow>                 _captureWindow;
        std::vector<XcbWindow>                     _boardTriggerWindow;
        ::ilias::Event                             _syncEvent;
        ::ilias::WaitHandle<::ilias::Result<void>> _grabEventHandle;
        int                                        _screenWidth  = 0;
        int                                        _screenHeight = 0;
        bool                                       _isCapture    = false;
        int                                        _xiOpcode;
    };
} // namespace mks::base
#endif