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

    #include "mksync/base/mk_capture.hpp"
    #include "mksync/base/base_library.h"
    #include "mksync/base/ring_buffer.hpp"

namespace mks::base
{
    class XcbConnect;
    class XcbWindow;
    class App;

    class MKS_BASE_API XcbMKCapture final : public MKCapture {
    public:
        XcbMKCapture(App &app);
        ~XcbMKCapture();
        auto start() -> ::ilias::Task<int> override;
        auto stop() -> ::ilias::Task<int> override;
        ///> 获取一个事件，如果没有就等待
        auto get_event() -> ::ilias::Task<NekoProto::IProto> override;
        ///> 唤起正在等待事件的协程
        auto notify() -> void override;

        auto pointer_proc(void *event) -> void;
        auto keyboard_proc(void *event) -> void;

    private:
        App                                       *_app = nullptr;
        RingBuffer<NekoProto::IProto>              _events;
        std::unique_ptr<XcbConnect>                _xcbConnect;
        std::unique_ptr<XcbWindow>                 _xcbWindow;
        ::ilias::Event                             _syncEvent;
        ::ilias::WaitHandle<::ilias::Result<void>> _grabEventHandle;
    };
} // namespace mks::base
#endif