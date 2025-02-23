#include "mksync/base/platform/win_mk_sender.hpp"

#ifdef _WIN32
namespace mks::base
{
    WinMKSender::WinMKSender() {}

    WinMKSender::~WinMKSender() {}

    auto WinMKSender::start() -> ::ilias::Task<int>
    {
        _isStart = true;
        co_return 0;
    }

    auto WinMKSender::stop() -> ::ilias::Task<int>
    {
        _isStart = false;
        co_return 0;
    }

    void WinMKSender::send_motion_event(const mks::MouseMotionEvent &event) const
    {
        if (!_isStart) {
            return;
        }
        INPUT input          = {0};
        input.type           = INPUT_MOUSE;
        input.mi.dx          = (LONG)(event.x * 65535);
        input.mi.dy          = (LONG)(event.y * 65535);
        input.mi.mouseData   = 0;
        input.mi.dwFlags     = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        input.mi.time        = 0;
        input.mi.dwExtraInfo = 0;
        SendInput(1, &input, sizeof(INPUT));
    }

    void WinMKSender::send_button_event(const mks::MouseButtonEvent &event) const
    {
        if (!_isStart) {
            return;
        }
        std::unique_ptr<INPUT[]> input;
        int                      count = 1;
        if (event.clicks <= 1) {
            input = std::make_unique<INPUT[]>(1);
        }
        else {
            count = event.clicks;
            input = std::make_unique<INPUT[]>(event.clicks);
        }
        for (int i = 0; i < count; ++i) {
            memset(&input[i], 0, sizeof(INPUT));
            input[i].type       = INPUT_MOUSE;
            input[i].mi.dwFlags = 0;
            input[i].mi.time    = 0;
            switch (event.button) {
            case mks::MouseButton::eButtonLeft:
                input[i].mi.dwFlags =
                    (event.state == mks::MouseButtonState::eButtonUp ? MOUSEEVENTF_LEFTUP
                                                                     : MOUSEEVENTF_LEFTDOWN);
                break;
            case mks::MouseButton::eButtonRight:
                input[i].mi.dwFlags =
                    (event.state == mks::MouseButtonState::eButtonUp ? MOUSEEVENTF_RIGHTUP
                                                                     : MOUSEEVENTF_RIGHTDOWN);
                break;
            case mks::MouseButton::eButtonMiddle:
                input[i].mi.dwFlags =
                    (event.state == mks::MouseButtonState::eButtonUp ? MOUSEEVENTF_MIDDLEDOWN
                                                                     : MOUSEEVENTF_MIDDLEUP);
                break;
            case mks::MouseButton::eButtonX1:
                input[i].mi.mouseData = XBUTTON1;
            case mks::MouseButton::eButtonX2:
                input[i].mi.mouseData = XBUTTON2;
                input[i].mi.dwFlags =
                    (event.state == mks::MouseButtonState::eButtonUp ? MOUSEEVENTF_XUP
                                                                     : MOUSEEVENTF_XDOWN);
                break;
            default:
                break;
            }
        }
        SendInput(count, input.get(), sizeof(INPUT));
    }

    void WinMKSender::send_wheel_event(const mks::MouseWheelEvent &event) const
    {
        if (!_isStart) {
            return;
        }
        bool                     is_vertical   = std::abs(event.x - 1e-6) > 1e-6;
        bool                     is_horizontal = std::abs(event.y - 1e-6) > 1e-6;
        std::unique_ptr<INPUT[]> input = std::make_unique<INPUT[]>(is_horizontal + is_vertical);
        memset(input.get(), 0, sizeof(INPUT) * (is_horizontal + is_vertical));
        if (is_vertical) {
            input[0].type         = INPUT_MOUSE;
            input[0].mi.mouseData = (LONG)(event.x * WHEEL_DELTA);
            input[0].mi.dwFlags   = MOUSEEVENTF_WHEEL;
        }
        if (is_horizontal) {
            input[is_vertical].type         = INPUT_MOUSE;
            input[is_vertical].mi.mouseData = (LONG)(event.y * WHEEL_DELTA);
            input[is_vertical].mi.dwFlags   = MOUSEEVENTF_HWHEEL;
        }
        SendInput(is_horizontal + is_vertical, input.get(), sizeof(INPUT));
    }

    void WinMKSender::send_keyboard_event(const mks::KeyEvent &event) const
    {
        if (!_isStart) {
            return;
        }
        INPUT input          = {0};
        input.type           = INPUT_KEYBOARD;
        input.ki.wVk         = 0;
        input.ki.wScan       = (WORD)key_code_to_windows_scan_code(event.key);
        input.ki.time        = 0;
        input.ki.dwExtraInfo = 0;
        input.ki.dwFlags =
            (event.state == mks::KeyboardState::eKeyUp ? KEYEVENTF_KEYUP : 0) | KEYEVENTF_SCANCODE;
        if ((input.ki.wScan & 0xe000) != 0) {
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }
        SendInput(1, &input, sizeof(INPUT));
    }
} // namespace mks::base
#endif