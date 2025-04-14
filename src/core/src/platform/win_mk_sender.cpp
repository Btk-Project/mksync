#include "mksync/core/platform/win_mk_sender.hpp"

#ifdef _WIN32
    #include <windows.h>

    #include "mksync/core/app.hpp"
MKS_BEGIN
WinMKSender::WinMKSender(IApp *app) : MKSender(app) {}

WinMKSender::~WinMKSender() {}

auto WinMKSender::start_sender() -> ::ilias::Task<int>
{
    HWND hd       = GetDesktopWindow();
    int  zoom     = GetDpiForWindow(hd); // 96 is the default DPI
    _screenWidth  = GetSystemMetrics(SM_CXSCREEN) * zoom / 96;
    _screenHeight = GetSystemMetrics(SM_CYSCREEN) * zoom / 96;
    _isStart      = true;
    co_return 0;
}

auto WinMKSender::stop_sender() -> ::ilias::Task<int>
{
    _isStart = false;
    co_return 0;
}

auto WinMKSender::name() -> const char *
{
    return "WinMKSender";
}

auto WinMKSender::handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void>
{
    ILIAS_ASSERT(event != nullptr);
    SPDLOG_INFO("WinMKSender handle_event: {}", event.protoName());
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

void WinMKSender::_send_motion_event(const mks::MouseMotionEventConversion &event) const
{
    if (!_isStart) {
        return;
    }
    SPDLOG_INFO("Mouse move: x {}, y {}", event.x, event.y);
    INPUT input = {};
    memset(&input, 0, sizeof(INPUT));
    input.type  = INPUT_MOUSE;
    input.mi.dx = (LONG)(event.isAbsolute ? (((float)event.x / _screenWidth) * 65535) : event.x);
    input.mi.dy = (LONG)(event.isAbsolute ? (((float)event.y / _screenHeight) * 65535) : event.y);
    input.mi.mouseData = 0;
    input.mi.dwFlags =
        (event.isAbsolute ? (MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE) : MOUSEEVENTF_MOVE);
    input.mi.time        = 0;
    input.mi.dwExtraInfo = 0;
    SendInput(1, &input, sizeof(INPUT));
}

void WinMKSender::_send_button_event(const mks::MouseButtonEvent &event) const
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

void WinMKSender::_send_wheel_event(const mks::MouseWheelEvent &event) const
{
    if (!_isStart) {
        return;
    }
    bool                     isVertical   = std::abs(event.x - 1e-6) > 1e-6;
    bool                     isHorizontal = std::abs(event.y - 1e-6) > 1e-6;
    std::unique_ptr<INPUT[]> input =
        std::make_unique<INPUT[]>(static_cast<int>(isHorizontal) + static_cast<int>(isVertical));
    memset(input.get(), 0,
           sizeof(INPUT) * (static_cast<int>(isHorizontal) + static_cast<int>(isVertical)));
    if (isVertical) {
        input[0].type         = INPUT_MOUSE;
        input[0].mi.mouseData = (LONG)(event.x * WHEEL_DELTA);
        input[0].mi.dwFlags   = MOUSEEVENTF_WHEEL;
    }
    if (isHorizontal) {
        input[static_cast<size_t>(isVertical)].type         = INPUT_MOUSE;
        input[static_cast<size_t>(isVertical)].mi.mouseData = (LONG)(event.y * WHEEL_DELTA);
        input[static_cast<size_t>(isVertical)].mi.dwFlags   = MOUSEEVENTF_HWHEEL;
    }
    SendInput(static_cast<int>(isHorizontal) + static_cast<int>(isVertical), input.get(),
              sizeof(INPUT));
}

void WinMKSender::_send_keyboard_event(const mks::KeyboardEvent &event) const
{
    if (!_isStart) {
        return;
    }
    INPUT input = {};
    memset(&input, 0, sizeof(INPUT));
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
MKS_END
#endif