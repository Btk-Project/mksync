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
        int posx, posy;
        _xcbConnect->get_default_root_window().get_geometry(posx, posy, _screenWidth,
                                                            _screenHeight);
        if (_screenWidth == 0 || _screenHeight == 0) {
            spdlog::error("Failed to get screen size!");
            co_return -1;
        }
        spdlog::info("Screen size: {}x{}", _screenWidth, _screenHeight);
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
        _xcbConnect->send_mouse_move(event.x * _screenWidth, event.y * _screenHeight);
    }

    void XcbMKSender::send_button_event(const mks::MouseButtonEvent &event) const
    {
        if (!_isStart || !_xcbConnect) {
            return;
        }
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
            spdlog::error("Unknown button: {} in xcb", (int)event.button);
            return;
        }
        if (event.state == mks::MouseButtonState::eButtonDown) {
            _xcbConnect->send_mouse_button_press(button);
        }
        else if (event.state == mks::MouseButtonState::eButtonUp) {
            _xcbConnect->send_mouse_button_release(button);
        }
        else if (event.state == mks::MouseButtonState::eClick) {
            for (int i = 0; i < event.clicks; ++i) {
                _xcbConnect->send_mouse_button_press(button);
                _xcbConnect->send_mouse_button_release(button);
            }
        }
    }

    void XcbMKSender::send_wheel_event(const mks::MouseWheelEvent &event) const
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
            _xcbConnect->send_mouse_button_press(button);
            _xcbConnect->send_mouse_button_release(button);
        }
        if (std::fabs(event.y) > 1e-6) {
            if (event.y > 0) {
                button = 6; // scroll left
            }
            else if (event.y < 0) {
                button = 7; // scroll right
            }
            _xcbConnect->send_mouse_button_press(button);
            _xcbConnect->send_mouse_button_release(button);
        }
    }

    void XcbMKSender::send_keyboard_event(const mks::KeyEvent &event) const
    {
        if (!_isStart || !_xcbConnect) {
            return;
        }
        auto *keysyms = _xcbConnect->key_symbols();
        int   keysym  = key_code_to_linux_keysym(event.key);
        if (keysym != 0) {
            xcb_keycode_t *keycode = xcb_key_symbols_get_keycode(keysyms, keysym);
            if (keycode != nullptr) {
                if (event.state == mks::KeyboardState::eKeyDown) {
                    _xcbConnect->send_key_press(*keycode);
                }
                else if (event.state == mks::KeyboardState::eKeyUp) {
                    _xcbConnect->send_key_release(*keycode);
                }
                free(keycode);
            }
        }
    }

} // namespace mks::base
#endif