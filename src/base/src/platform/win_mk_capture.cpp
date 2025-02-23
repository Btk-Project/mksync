#include "mksync/base/platform/win_mk_capture.hpp"
#ifdef _WIN32
    #include <spdlog/spdlog.h>

    #include "mksync/proto/proto.hpp"

namespace mks::base
{
    template <typename T>
    using Task = ::ilias::Task<T>;
    auto WinMKCapture::get_event() -> Task<NekoProto::IProto>
    {
        if (_events.size() == 0) {
            _syncEvent.clear();
            co_await _syncEvent;
        }
        NekoProto::IProto proto;
        _events.pop(proto);
        co_return std::move(proto);
    }

    auto WinMKCapture::notify() -> void
    {
        _syncEvent.set();
    }

    WinMKCapture::WinMKCapture() : _events(10)
    {
        static WinMKCapture *self = nullptr;
        self                      = this;
        _mosueHook                = SetWindowsHookExW(
            WH_MOUSE_LL,
            [](int ncode, WPARAM wp, LPARAM lp) { return self->_mouse_proc(ncode, wp, lp); },
            nullptr, 0);
        if (_mosueHook == nullptr) {
            spdlog::error("SetWindowsHookExW failed: {}", GetLastError());
            return;
        }

        _keyboardHook = SetWindowsHookExW(
            WH_KEYBOARD_LL,
            [](int ncode, WPARAM wp, LPARAM lp) { return self->_keyboard_proc(ncode, wp, lp); },
            nullptr, 0);
        if (_keyboardHook == nullptr) {
            spdlog::error("SetWindowsHookExW failed: {}", GetLastError());
            return;
        }
        _screenWidth  = GetSystemMetrics(SM_CXSCREEN);
        _screenHeight = GetSystemMetrics(SM_CYSCREEN);
    }

    WinMKCapture::~WinMKCapture()
    {
        notify();
        UnhookWindowsHookEx(_mosueHook);
        UnhookWindowsHookEx(_keyboardHook);
    }

    LRESULT WinMKCapture::_mouse_proc(int ncode, WPARAM wp, LPARAM lp)
    {
        auto             *hookStruct = reinterpret_cast<MSLLHOOKSTRUCT *>(lp);
        NekoProto::IProto proto;
        switch (wp) {
        case WM_LBUTTONDOWN:
            proto = mks::MouseButtonEvent::emplaceProto(
                mks::MouseButtonState::eButtonDown, mks::MouseButton::eButtonLeft, (uint8_t)1,
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Left Mouse Button Down");
            break;
        case WM_LBUTTONUP:
            proto = mks::MouseButtonEvent::emplaceProto(
                mks::MouseButtonState::eButtonUp, mks::MouseButton::eButtonLeft, (uint8_t)1,
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Left Mouse Button Up");
            break;
        case WM_LBUTTONDBLCLK:
            proto = mks::MouseButtonEvent::emplaceProto(
                mks::MouseButtonState::eClick, mks::MouseButton::eButtonLeft, (uint8_t)2,
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Left Mouse Button Double Click");
            break;
        case WM_RBUTTONDOWN:
            proto = mks::MouseButtonEvent::emplaceProto(
                mks::MouseButtonState::eButtonDown, mks::MouseButton::eButtonRight, (uint8_t)1,
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Right Mouse Button Down");
            break;
        case WM_RBUTTONUP:
            proto = mks::MouseButtonEvent::emplaceProto(
                mks::MouseButtonState::eButtonUp, mks::MouseButton::eButtonRight, (uint8_t)1,
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Right Mouse Button Up");
            break;
        case WM_RBUTTONDBLCLK:
            proto = mks::MouseButtonEvent::emplaceProto(
                mks::MouseButtonState::eClick, mks::MouseButton::eButtonRight, (uint8_t)2,
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Right Mouse Button Double Click");
            break;
        case WM_MBUTTONDOWN:
            proto = mks::MouseButtonEvent::emplaceProto(
                mks::MouseButtonState::eButtonDown, mks::MouseButton::eButtonMiddle, (uint8_t)1,
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Middle Mouse Button Down");
            break;
        case WM_MBUTTONUP:
            proto = mks::MouseButtonEvent::emplaceProto(
                mks::MouseButtonState::eButtonUp, mks::MouseButton::eButtonMiddle, (uint8_t)1,
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Middle Mouse Button Up");
            break;
        case WM_MBUTTONDBLCLK:
            proto = mks::MouseButtonEvent::emplaceProto(
                mks::MouseButtonState::eClick, mks::MouseButton::eButtonMiddle, (uint8_t)2,
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Middle Mouse Button Double Click");
            break;
        case WM_MOUSEMOVE:
            proto = mks::MouseMotionEvent::emplaceProto(
                (float)hookStruct->pt.x / _screenWidth, (float)hookStruct->pt.y / _screenHeight,
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Mouse Move X: {} Y: {}", hookStruct->pt.x, hookStruct->pt.y);
            break;
        case WM_MOUSEWHEEL:
            proto = mks::MouseWheelEvent::emplaceProto(
                (float)(GET_WHEEL_DELTA_WPARAM(hookStruct->mouseData) / WHEEL_DELTA), 0.F,
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Mouse Wheel {}", GET_WHEEL_DELTA_WPARAM(hookStruct->mouseData));
            break;
        case WM_MOUSEHWHEEL:
            proto = mks::MouseWheelEvent::emplaceProto(
                0.F, (float)(GET_WHEEL_DELTA_WPARAM(hookStruct->mouseData) / WHEEL_DELTA),
                (uint64_t)std::chrono::system_clock::now().time_since_epoch().count());
            spdlog::info("Mouse HWheel {}", GET_WHEEL_DELTA_WPARAM(hookStruct->mouseData));
        default:
            return CallNextHookEx(nullptr, ncode, wp, lp);
        }
        _events.push(std::move(proto));
        _syncEvent.set();
        return CallNextHookEx(nullptr, ncode, wp, lp);
    }

    LRESULT WinMKCapture::_keyboard_proc(int ncode, WPARAM wp, LPARAM lp)
    {
        auto *hookStruct = reinterpret_cast<KBDLLHOOKSTRUCT *>(lp);
        char  name[256]{0};
        GetKeyNameTextA(hookStruct->scanCode << 16, name, 256);
        switch (wp) {
        case WM_KEYDOWN:
            spdlog::info("Key {} Down ", name);
            break;
        case WM_KEYUP:
            spdlog::info("Key {} Up ", name);
        }
        uint16_t rawcode;
        bool     virtualKey;
        uint32_t scanCode = hookStruct->scanCode;
        if ((hookStruct->flags & LLKHF_EXTENDED) != 0U) {
            scanCode |= (KF_EXTENDED << 16);
        }
        auto keycode =
            mks::windows_scan_code_to_key_code(scanCode, hookStruct->vkCode, &rawcode, &virtualKey);

        mks::KeyMod keymod = mks::KeyMod::eKmodNone;
        if ((GetAsyncKeyState(VK_LMENU) & 0x8000) != 0) {
            keymod = mks::KeyMod::eKmodLalt;
        }
        if ((GetAsyncKeyState(VK_RMENU) & 0x8000) != 0) {
            keymod = mks::KeyMod::eKmodRalt;
        }
        if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) {
            keymod = mks::KeyMod::eKmodLctrl;
        }
        if ((GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0) {
            keymod = mks::KeyMod::eKmodRctrl;
        }
        if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0) {
            keymod = mks::KeyMod::eKmodShift;
        }
        if ((GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0) {
            keymod = mks::KeyMod::eKmodRshift;
        }
        if ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) {
            keymod = mks::KeyMod::eKmodLgui;
        }
        if ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0) {
            keymod = mks::KeyMod::eKmodRgui;
        }
        if ((GetAsyncKeyState(VK_CAPITAL) & 0x0001) != 0) {
            keymod = mks::KeyMod::eKmodCaps;
        }
        if ((GetAsyncKeyState(VK_NUMLOCK) & 0x0001) != 0) {
            keymod = mks::KeyMod::eKmodNum;
        }
        if ((GetAsyncKeyState(VK_SCROLL) & 0x0001) != 0) {
            keymod = mks::KeyMod::eKmodScroll;
        }

        _events.emplace(mks::KeyEvent::emplaceProto(
            (wp == WM_KEYDOWN) ? mks::KeyboardState::eKeyDown : mks::KeyboardState::eKeyUp, keycode,
            keymod, (uint64_t)std::chrono::system_clock::now().time_since_epoch().count()));
        _syncEvent.set();
        return CallNextHookEx(nullptr, ncode, wp, lp);
    }
} // namespace mks::base
#endif