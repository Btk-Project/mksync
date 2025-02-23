#include "mksync/base/platform/xcb_mk_capture.hpp"

#ifdef __linux__
    #include <spdlog/spdlog.h>

    #include "mksync/base/app.hpp"
    #include "mksync/base/platform/xcb_window.hpp"

namespace mks::base
{
    XcbMKCapture::XcbMKCapture(App &app) : _app(&app), _events(10) {}

    XcbMKCapture::~XcbMKCapture()
    {
        stop().wait();
        _xcbWindow.reset();
        _xcbConnect.reset();
    }

    auto XcbMKCapture::start() -> ::ilias::Task<int>
    {
        if (!_xcbConnect) {
            _xcbConnect = std::make_unique<XcbConnect>(_app->get_io_context());
            auto ret    = co_await _xcbConnect->connect(nullptr);
            if (!ret) {
                spdlog::error("Failed to connect to X server!");
                co_return -1;
            }
        }
        if (!_xcbWindow) {
            _xcbWindow = std::make_unique<XcbWindow>(_xcbConnect.get());
            _xcbWindow->set_property("WM_TYPE", "NORMAL");
            _xcbWindow->show();
            xcb_flush(_xcbConnect->connection());
        }

        auto ret = _xcbConnect->grab_keyboard(
            _xcbWindow.get(), [this](xcb_generic_event_t *event) { keyboard_proc(event); });
        if (ret != 0) {
            spdlog::error("xcb grab keyboard failed!");
            co_return ret;
        }
        auto ret1 = _xcbConnect->grab_pointer(
            _xcbWindow.get(), [this](xcb_generic_event_t *event) { pointer_proc(event); });
        if (ret1 != 0) {
            spdlog::error("xcb grab pointer failed!");
            co_return ret1;
        }
        _grabEventHandle = ilias_go _xcbConnect->event_loop();
        co_return 0;
    }

    auto XcbMKCapture::stop() -> ::ilias::Task<int>
    {
        if (_grabEventHandle) {
            _grabEventHandle.cancel();
        }
        if (_xcbConnect) {
            _xcbConnect->ungrab_pointer();
            _xcbConnect->ungrab_keyboard();
        }
        _xcbWindow.reset();
    }

    auto XcbMKCapture::pointer_proc(void *ev) -> void
    {
        auto *event = (xcb_generic_event_t *)ev;
        switch (event->response_type & ~0x80) {
        case XCB_BUTTON_PRESS: {
            xcb_button_press_event_t *buttonEvent = (xcb_button_press_event_t *)event;
            spdlog::info("mouse button {} press", buttonEvent->detail);
            break;
        }
        case XCB_BUTTON_RELEASE: {
            xcb_button_release_event_t *buttonEvent = (xcb_button_release_event_t *)event;
            spdlog::info("mouse button {} release", buttonEvent->detail);
            break;
        }
        case XCB_MOTION_NOTIFY: {
            xcb_motion_notify_event_t *motionEvent = (xcb_motion_notify_event_t *)event;
            spdlog::info("mouse move to ({}, {})", motionEvent->event_x, motionEvent->event_y);
            break;
        }
        default:
            spdlog::info("unknown event {}", event->response_type & ~0x80);
            break;
        }
    }

    auto XcbMKCapture::keyboard_proc(void *ev) -> void
    {
        auto *event   = (xcb_generic_event_t *)ev;
        auto *keysyms = _xcbConnect->key_symbols();
        switch (event->response_type & ~0x80) {
        case XCB_KEY_PRESS: {
            xcb_key_press_event_t *buttonEvent = (xcb_key_press_event_t *)event;
            xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, buttonEvent->detail, 0);
            if (keysym >= 0x20 && keysym <= 0x7E) {
                spdlog::info("key {} press", (char)keysym);
            }
            else {
                spdlog::info("key {} press", keysym);
            }
            break;
        }
        case XCB_KEY_RELEASE: {
            xcb_key_press_event_t *buttonEvent = (xcb_key_press_event_t *)event;
            xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, buttonEvent->detail, 0);
            if (keysym >= 0x20 && keysym <= 0x7E) {
                spdlog::info("key {} release", (char)keysym);
            }
            else {
                spdlog::info("key {} release", keysym);
            }
            break;
        }
        default:
            spdlog::error("unknown event type {}", event->response_type & ~0x80);
            break;
        }
    }

    ///> 获取一个事件，如果没有就等待
    auto XcbMKCapture::get_event() -> ILIAS_NAMESPACE::Task<NekoProto::IProto>
    {
        if (_events.size() == 0) {
            _syncEvent.clear();
            co_await _syncEvent;
        }
        NekoProto::IProto proto;
        _events.pop(proto);
        co_return std::move(proto);
    }

    ///> 唤起正在等待事件的协程
    auto XcbMKCapture::notify() -> void
    {
        _syncEvent.set();
    }

} // namespace mks::base
#endif