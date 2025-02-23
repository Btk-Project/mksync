#include "mksync/base/platform/xcb_mk_sender.hpp"

#ifdef __linux__
    #include <spdlog/spdlog.h>

    #include "mksync/base/app.hpp"
    #include "mksync/base/platform/xcb_window.hpp"

namespace mks::base
{

    XcbMKSender::XcbMKSender(App &app) : _app(&app) {}

    XcbMKSender::~XcbMKSender()
    {
        stop().wait();
        _xcbConnect.reset();
    }

    auto XcbMKSender::start() -> ::ilias::Task<int>
    {
        if (!_xcbConnect) {
            _xcbConnect = std::make_unique<XcbConnect>(_app->get_io_context());
            auto ret    = co_await _xcbConnect->connect(nullptr);
            if (!ret) {
                spdlog::error("Failed to connect to X server!");
                co_return -1;
            }
        }
        _isStart = true;
        ilias_go _xcbConnect->event_loop();
        co_return 0;
    }

    auto XcbMKSender::stop() -> ::ilias::Task<int>
    {
        _isStart = false;
        if (_eventLoopHandle) {
            _eventLoopHandle.cancel();
        }
        co_return 0;
    }

    void XcbMKSender::send_motion_event(const mks::MouseMotionEvent &event) const
    {
        if (!_isStart || !_xcbConnect) {
            return;
        }
    }

    void XcbMKSender::send_button_event(const mks::MouseButtonEvent &event) const
    {
        if (!_isStart || !_xcbConnect) {
            return;
        }
    }

    void XcbMKSender::send_wheel_event(const mks::MouseWheelEvent &event) const
    {
        if (!_isStart || !_xcbConnect) {
            return;
        }
    }

    void XcbMKSender::send_keyboard_event(const mks::KeyEvent &event) const
    {
        if (!_isStart || !_xcbConnect) {
            return;
        }
    }

} // namespace mks::base
#endif