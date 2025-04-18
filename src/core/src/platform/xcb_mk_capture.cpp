#include "mksync/core/platform/xcb_mk_capture.hpp"

#ifdef __linux__
    #include <spdlog/spdlog.h>

    #include <xcb/xcb.h>
    #include <xcb/xinput.h>
    #include <stdlib.h>

    #include "mksync/core/app.hpp"
    #include "mksync/core/platform/xcb_window.hpp"

MKS_BEGIN
XcbMKCapture::XcbMKCapture(IApp *app) : MKCapture(app), _events(10) {}

XcbMKCapture::~XcbMKCapture()
{
    _xcbConnect.reset();
}

auto XcbMKCapture::setup() -> ::ilias::Task<int>
{
    if (!_xcbConnect) {
        _xcbConnect         = std::make_unique<XcbConnect>(_app->get_io_context());
        const auto *display = getenv("DISPLAY");
        if (auto ret = co_await _xcbConnect->connect(display == nullptr ? ":0" : display); !ret) {
            SPDLOG_ERROR("Failed to connect to X server!");
            co_return -1;
        }
    }
    _captureWindow = std::make_unique<XcbWindow>(_xcbConnect->get_default_root_window());
    _xcbConnect->event_handler([this](xcb_generic_event_t *event) { window_proc(event); });
    int posx, posy;
    _captureWindow->get_geometry(posx, posy, _screenWidth, _screenHeight);
    if (_screenHeight == 0 || _screenWidth == 0) {
        SPDLOG_ERROR("Failed to get screen size!");
        co_return -1;
    }
    SPDLOG_INFO("Screen size: {}x{}", _screenWidth, _screenHeight);
    // 监听鼠标事件
    // 检查 X Input Extension 是否可用
    if (!_xcbConnect->query_extension(&xcb_input_id)) {
        SPDLOG_ERROR("X Input Extension not found!");
        co_return -1;
    }

    // 查询 XInput2 的版本
    xcb_input_xi_query_version_cookie_t versionCookie =
        xcb_input_xi_query_version(_xcbConnect->connection(), 2, 3); // 请求 XInput2.3 的版本
    xcb_input_xi_query_version_reply_t *versionReply =
        xcb_input_xi_query_version_reply(_xcbConnect->connection(), versionCookie, nullptr);
    if ((versionReply == nullptr) || versionReply->major_version < 2) {
        SPDLOG_ERROR("XInput2 的版本不足，要求 2.0 或更高\n");
        free(versionReply);
        co_return -1;
    }
    free(versionReply);

    // 设置监听的事件掩码
    struct {
        xcb_input_event_mask_t
            head; // describes the subsequent xcb_input_xi_event_mask_t (or an array thereof)
        xcb_input_xi_event_mask_t mask;
    } mask;

    mask.head.deviceid = XCB_INPUT_DEVICE_ALL;
    mask.head.mask_len = sizeof(mask.mask) / sizeof(uint32_t);

    // 设置感兴趣的事件 (XI_RawKeyRelease 和 XI_RawMotion)
    mask.mask = (xcb_input_xi_event_mask_t)(XCB_INPUT_XI_EVENT_MASK_RAW_MOTION);

    // 发送 XISelectEvents 请求
    xcb_input_xi_select_events(_xcbConnect->connection(), _captureWindow->native_handle(), 1,
                               &mask.head);
    _xcbConnect->select_event(_captureWindow.get());

    _grabEventHandle = ::ilias::spawn(*_app->get_io_context(), _xcbConnect->event_loop());
    co_return co_await MKCapture::setup();
}

auto XcbMKCapture::teardown() -> ::ilias::Task<int>
{
    _syncEvent.set();
    _boardTriggerWindow.clear();
    _xcbConnect.reset();
    co_return co_await MKCapture::teardown();
}

auto XcbMKCapture::start_capture() -> ::ilias::Task<int>
{
    auto xcbWindow = _xcbConnect->get_default_root_window();
    _xcbConnect->fake_mouse_move(_screenWidth / 2, _screenWidth / 2);
    if (auto ret = _xcbConnect->grab_pointer(
            _captureWindow.get(), [this](xcb_generic_event_t *event) { pointer_proc(event); },
            true);
        ret != 0) {
        SPDLOG_ERROR("xcb grab pointer failed! error: {}", ret);
        co_return ret;
    }
    if (auto ret = _xcbConnect->grab_keyboard(
            &xcbWindow, [this](xcb_generic_event_t *event) { keyboard_proc(event); }, true);
        ret != 0) {
        SPDLOG_ERROR("xcb grab keyboard failed!");
        co_return ret;
    }
    _isCapture = true;
    co_return 0;
}

auto XcbMKCapture::stop_capture() -> ::ilias::Task<int>
{
    _isCapture = false;
    if (_grabEventHandle) {
        _grabEventHandle.cancel();
        co_await std::move(_grabEventHandle);
    }
    if (_xcbConnect) {
        _xcbConnect->ungrab_pointer();
        _xcbConnect->ungrab_keyboard();
    }
    co_return 0;
}

auto XcbMKCapture::pointer_proc(void *ev) -> void
{
    // TODO: 不要一直发送边缘事件，如果鼠标持续在边缘则忽略边缘事件。
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
        SPDLOG_INFO("mouse button {} press", buttonEvent->detail);
    } break;
    case XCB_BUTTON_RELEASE: {
        xcb_button_release_event_t *buttonEvent = (xcb_button_release_event_t *)event;
        if (buttonEvent->detail <= 1) {
            proto = mks::MouseButtonEvent::emplaceProto(MouseButtonState::eButtonUp,
                                                        (MouseButton)buttonEvent->detail, 1,
                                                        buttonEvent->time);
        }
        SPDLOG_INFO("mouse button {} release", buttonEvent->detail);
        break;
    }
    case XCB_MOTION_NOTIFY: {
        xcb_motion_notify_event_t *motionEvent = (xcb_motion_notify_event_t *)event;
        proto                                  = mks::MouseMotionEvent::emplaceProto(
            (int32_t)(motionEvent->event_x - (_screenWidth / 2)),
            (int32_t)(motionEvent->event_y - (_screenHeight / 2)), false, motionEvent->time);
        _xcbConnect->fake_mouse_move(_screenWidth / 2, _screenWidth / 2);
        SPDLOG_INFO("mouse move to ({}, {})", motionEvent->event_x, motionEvent->event_y);
        break;
    }
    default:
        SPDLOG_INFO("unknown event {}", event->response_type & ~0x80);
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
        xcb_keysym_t           keysym = xcb_key_symbols_get_keysym(keysyms, buttonEvent->detail, 0);
        if (keysym >= 0x20 && keysym <= 0x7E) {
            SPDLOG_INFO("key {} press", (char)keysym);
        }
        else {
            SPDLOG_INFO("key {} press", keysym);
        }
        auto keycode = linux_keysym_to_key_code(keysym);
        SPDLOG_INFO("keycode {}", (int)keycode);
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
        proto = KeyboardEvent::emplaceProto(KeyboardState::eKeyDown, keycode, (KeyMod)keyMod,
                                            buttonEvent->time);
    } break;
    case XCB_KEY_RELEASE: {
        xcb_key_press_event_t *buttonEvent = (xcb_key_press_event_t *)event;
        xcb_keysym_t           keysym = xcb_key_symbols_get_keysym(keysyms, buttonEvent->detail, 0);
        if (keysym >= 0x20 && keysym <= 0x7E) {
            SPDLOG_INFO("key {} release", (char)keysym);
        }
        else {
            SPDLOG_INFO("key {} release", keysym);
        }
        auto keycode = linux_keysym_to_key_code(keysym);
        SPDLOG_INFO("keycode {}", (int)keycode);
        proto = KeyboardEvent::emplaceProto(KeyboardState::eKeyUp, keycode, KeyMod::eKmodNone,
                                            buttonEvent->time);
    } break;
    default:
        SPDLOG_ERROR("unknown event type {}", event->response_type & ~0x80);
        break;
    }
    if (proto != nullptr) {
        _events.push(std::move(proto));
        _syncEvent.set();
    }
}

auto XcbMKCapture::window_proc(void *ev) -> void
{
    auto *event = (xcb_generic_event_t *)ev;
    if ((event->response_type & ~0x80) == XCB_GE_GENERIC) {
        auto *genericEvent = (xcb_ge_generic_event_t *)event;
        if (genericEvent->event_type == XCB_INPUT_RAW_MOTION) {
            auto pos = _xcbConnect->query_pointer(_captureWindow.get());
            if (_isCapture) {
                SPDLOG_INFO("response event type x:{} y:{}", pos.first, pos.second);
                return;
            }
            uint32_t border = 0;
            if (pos.first <= 0) {
                border |= (uint32_t)BorderEvent::eLeft;
            }
            if (pos.first >= _screenWidth - 1) {
                border |= (uint32_t)BorderEvent::eRight;
            }
            if (pos.second <= 0) {
                border |= (uint32_t)BorderEvent::eTop;
            }
            if (pos.second >= _screenHeight - 1) {
                border |= (uint32_t)BorderEvent::eBottom;
            }
            if (border != 0) {
                SPDLOG_INFO("on border event type x:{} y:{}", pos.first, pos.second);
                auto proto =
                    BorderEvent::emplaceProto(0U, border, (int32_t)pos.first, (int32_t)pos.second);
                _events.push(std::move(proto));
                _syncEvent.set();
            }
        }
        else {
            SPDLOG_ERROR("generic event type: {}", genericEvent->event_type);
        }
    }
}

auto XcbMKCapture::name() -> const char *
{
    return "XcbMKCapture";
}

///> 获取一个事件，如果没有就等待
auto XcbMKCapture::get_event() -> ilias::IoTask<NekoProto::IProto>
{
    if (_events.size() == 0) {
        _syncEvent.clear();
        auto ret = co_await _syncEvent;
        if (!ret) {
            co_return Unexpected<Error>(ret.error());
        }
    }
    NekoProto::IProto proto;
    _events.pop(proto);
    co_return std::move(proto);
}
MKS_END
#endif