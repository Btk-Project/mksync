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
        auto xcbWindow = _xcbConnect->get_default_root_window();
        int  posx, posy;
        xcbWindow.get_geometry(posx, posy, _screenWidth, _screenHeight);
        if (_screenHeight == 0 || _screenWidth == 0) {
            spdlog::error("Failed to get screen size!");
            co_return -1;
        }
        auto ret = _xcbConnect->grab_keyboard(
            &xcbWindow, [this](xcb_generic_event_t *event) { keyboard_proc(event); }, true);
        if (ret != 0) {
            spdlog::error("xcb grab keyboard failed!");
            co_return ret;
        }
        auto ret1 = _xcbConnect->grab_pointer(
            &xcbWindow, [this](xcb_generic_event_t *event) { pointer_proc(event); }, true);
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
        co_return 0;
    }

    auto XcbMKCapture::pointer_proc(void *ev) -> void
    {
        auto             *event = (xcb_generic_event_t *)ev;
        NekoProto::IProto proto;
        switch (event->response_type & ~0x80) {
        case XCB_BUTTON_PRESS: {
            xcb_button_press_event_t *buttonEvent = (xcb_button_press_event_t *)event;
            if (buttonEvent->detail <= 3) {
                proto = mks::MouseButtonEvent::emplaceProto(MouseButtonState::eButtonDown,
                                                            (MouseButton)buttonEvent->detail, 1,
                                                            buttonEvent->time);
            }
            else if (buttonEvent->detail == 4) {
                proto = mks::MouseWheelEvent::emplaceProto(1, 0, buttonEvent->time);
            }
            else if (buttonEvent->detail == 5) {
                proto = mks::MouseWheelEvent::emplaceProto(-1, 0, buttonEvent->time);
            }
            else if (buttonEvent->detail == 6) {
                proto = mks::MouseWheelEvent::emplaceProto(0, 1, buttonEvent->time);
            }
            else if (buttonEvent->detail == 7) {
                proto = mks::MouseWheelEvent::emplaceProto(0, -1, buttonEvent->time);
            }
            spdlog::info("mouse button {} press", buttonEvent->detail);
        } break;
        case XCB_BUTTON_RELEASE: {
            xcb_button_release_event_t *buttonEvent = (xcb_button_release_event_t *)event;
            if (buttonEvent->detail <= 1) {
                proto = mks::MouseButtonEvent::emplaceProto(MouseButtonState::eButtonUp,
                                                            (MouseButton)buttonEvent->detail, 1,
                                                            buttonEvent->time);
            }
            spdlog::info("mouse button {} release", buttonEvent->detail);
            break;
        }
        case XCB_MOTION_NOTIFY: {
            xcb_motion_notify_event_t *motionEvent = (xcb_motion_notify_event_t *)event;
            proto = mks::MouseMotionEvent::emplaceProto((float)motionEvent->event_x / _screenWidth,
                                                        (float)motionEvent->event_y / _screenHeight,
                                                        motionEvent->time);
            spdlog::info("mouse move to ({}, {})", motionEvent->event_x, motionEvent->event_y);
            break;
        }
        default:
            spdlog::info("unknown event {}", event->response_type & ~0x80);
            return;
        }
        _events.push(std::move(proto));
        _syncEvent.set();
    }

    auto XcbMKCapture::keyboard_proc(void *ev) -> void
    {
        auto             *event   = (xcb_generic_event_t *)ev;
        auto             *keysyms = _xcbConnect->key_symbols();
        NekoProto::IProto proto;
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
            auto keycode = linux_keysym_to_key_code(keysym);
            spdlog::info("keycode {}", (int)keycode);
            uint16_t keyMod = (uint16_t)KeyMod::eKmodNone;
            if ((buttonEvent->state & XCB_MOD_MASK_SHIFT) != 0) {
                keyMod |= (uint16_t)KeyMod::eKmodShift;
            }
            if ((buttonEvent->state & XCB_MOD_MASK_CONTROL) != 0) {
                keyMod |= (uint16_t)KeyMod::eKmodCtrl;
            }
            if ((buttonEvent->state & XCB_MOD_MASK_LOCK) != 0) {
                keyMod |= (uint16_t)KeyMod::eKmodCaps;
            }
            if ((buttonEvent->state & XCB_MOD_MASK_1) != 0) {
                keyMod |= (uint16_t)KeyMod::eKmodAlt;
            }
            if ((buttonEvent->state & XCB_MOD_MASK_4) != 0) {
                keyMod |= (uint16_t)KeyMod::eKmodGui;
            }
            proto = KeyEvent::emplaceProto(KeyboardState::eKeyDown, keycode, (KeyMod)keyMod,
                                           buttonEvent->time);
        } break;
        case XCB_KEY_RELEASE: {
            xcb_key_press_event_t *buttonEvent = (xcb_key_press_event_t *)event;
            xcb_keysym_t keysym = xcb_key_symbols_get_keysym(keysyms, buttonEvent->detail, 0);
            if (keysym >= 0x20 && keysym <= 0x7E) {
                spdlog::info("key {} release", (char)keysym);
            }
            else {
                spdlog::info("key {} release", keysym);
            }
            auto keycode = linux_keysym_to_key_code(keysym);
            spdlog::info("keycode {}", (int)keycode);
            proto = KeyEvent::emplaceProto(KeyboardState::eKeyUp, keycode, KeyMod::eKmodNone,
                                           buttonEvent->time);
        } break;
        default:
            spdlog::error("unknown event type {}", event->response_type & ~0x80);
            break;
        }
    }

    ///> 获取一个事件，如果没有就等待
    auto XcbMKCapture::get_event() -> ilias::Task<NekoProto::IProto>
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