#include "mksync/core/platform/xcb_mk_sender.hpp"

#ifdef __linux__
    #include <spdlog/spdlog.h>

    #include "mksync/core/app.hpp"
    #include "mksync/core/platform/xcb_window.hpp"

MKS_BEGIN
XcbMKSender::XcbMKSender(IApp *app) : MKSender(app) {}

XcbMKSender::~XcbMKSender()
{
    _xcbConnect.reset();
}

auto XcbMKSender::name() -> const char *
{
    return "XcbMKSender";
}

auto XcbMKSender::handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void>
{
    if (event == nullptr) {
        co_return;
    }
    if (event.type() == NekoProto::ProtoFactory::protoType<MouseMotionEventConversion>()) {
        ILIAS_ASSERT(event.cast<MouseMotionEventConversion>() != nullptr);
        _send_motion_event(*event.cast<MouseMotionEventConversion>());
    }
    else if (event.type() == NekoProto::ProtoFactory::protoType<MouseButtonEvent>()) {
        ILIAS_ASSERT(event.cast<MouseButtonEvent>() != nullptr);
        _send_button_event(*event.cast<MouseButtonEvent>());
    }
    else if (event.type() == NekoProto::ProtoFactory::protoType<MouseWheelEvent>()) {
        ILIAS_ASSERT(event.cast<MouseWheelEvent>() != nullptr);
        _send_wheel_event(*event.cast<MouseWheelEvent>());
    }
    else if (event.type() == NekoProto::ProtoFactory::protoType<KeyboardEvent>()) {
        ILIAS_ASSERT(event.cast<KeyboardEvent>() != nullptr);
        _send_keyboard_event(*event.cast<KeyboardEvent>());
    }
    co_return co_await MKSender::handle_event(event);
}

auto XcbMKSender::start_sender() -> ::ilias::Task<int>
{
    if (!_xcbConnect) {
        _xcbConnect         = std::make_unique<XcbConnect>(_app->get_io_context());
        const auto *display = getenv("DISPLAY");
        auto        ret     = co_await _xcbConnect->connect(display == nullptr ? ":0" : display);
        if (!ret) {
            SPDLOG_ERROR("Failed to connect to X server!");
            co_return -1;
        }
    }
    int posx, posy;
    _xcbConnect->get_default_root_window().get_geometry(posx, posy, _screenWidth, _screenHeight);
    if (_screenWidth == 0 || _screenHeight == 0) {
        SPDLOG_ERROR("Failed to get screen size!");
        co_return -1;
    }
    SPDLOG_INFO("Screen size: {}x{}", _screenWidth, _screenHeight);
    _isStart       = true;
    _xcbLoopHandle = ::ilias::spawn(*_app->get_io_context(), _xcbConnect->event_loop());
    co_return 0;
}

auto XcbMKSender::stop_sender() -> ::ilias::Task<int>
{
    _isStart = false;
    if (_eventLoopHandle) {
        _eventLoopHandle.cancel();
    }
    co_return 0;
}

void XcbMKSender::_send_motion_event(const mks::MouseMotionEventConversion &event) const
{
    if (!_isStart || !_xcbConnect) {
        return;
    }
    // TODO: 相对移动
    SPDLOG_INFO("Mouse motion event: x={}, y={}", event.x, event.y);
    _xcbConnect->fake_mouse_move(event.x, event.y);
}

void XcbMKSender::_send_button_event(const mks::MouseButtonEvent &event) const
{
    if (!_isStart || !_xcbConnect) {
        return;
    }
    SPDLOG_INFO("Button event: {} {}", (int)event.button, (int)event.state);
    int button = 0;
    switch (event.button) {
    case mks::MouseButton::eButtonLeft:
        button = 1;
        break;
    case mks::MouseButton::eButtonRight:
        button = 3;
        break;
    case mks::MouseButton::eButtonMiddle:
        button = 2;
        break;
    default:
        SPDLOG_ERROR("Unknown button: {} in xcb", (int)event.button);
        return;
    }
    if (event.state == mks::MouseButtonState::eButtonDown) {
        _xcbConnect->fake_mouse_button_press(button);
    }
    else if (event.state == mks::MouseButtonState::eButtonUp) {
        _xcbConnect->fake_mouse_button_release(button);
    }
    else if (event.state == mks::MouseButtonState::eClick) {
        for (int i = 0; i < event.clicks; ++i) {
            _xcbConnect->fake_mouse_button_press(button);
            _xcbConnect->fake_mouse_button_release(button);
        }
    }
}

void XcbMKSender::_send_wheel_event(const mks::MouseWheelEvent &event) const
{
    if (!_isStart || !_xcbConnect) {
        return;
    }
    int button = 4;
    if (std::fabs(event.x) > 1e-6) {
        if (event.x > 0) {
            button = 4; // scroll up
        }
        else if (event.x < 0) {
            button = 5; // scroll down
        }
        _xcbConnect->fake_mouse_button_press(button);
        _xcbConnect->fake_mouse_button_release(button);
    }
    if (std::fabs(event.y) > 1e-6) {
        if (event.y > 0) {
            button = 6; // scroll left
        }
        else if (event.y < 0) {
            button = 7; // scroll right
        }
        _xcbConnect->fake_mouse_button_press(button);
        _xcbConnect->fake_mouse_button_release(button);
    }
}

void XcbMKSender::_send_keyboard_event(const mks::KeyboardEvent &event) const
{
    if (!_isStart || !_xcbConnect) {
        return;
    }
    SPDLOG_INFO("send keyboard event: {}", (int)event.key);
    auto *keysyms = _xcbConnect->key_symbols();
    int   keysym  = key_code_to_linux_keysym(event.key);
    if (keysym != 0) {
        xcb_keycode_t *keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
        if (keycode != nullptr) {
            if (event.state == mks::KeyboardState::eKeyDown) {
                _xcbConnect->fake_key_press(*keycode);
            }
            else if (event.state == mks::KeyboardState::eKeyUp) {
                _xcbConnect->fake_key_release(*keycode);
            }
            free(keycode);
        }
    }
}
MKS_END
#endif