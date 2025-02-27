#include "mksync/base/platform/win_mk_capture.hpp"
#ifdef _WIN32
    #include <spdlog/spdlog.h>

    #include "mksync/proto/proto.hpp"

namespace mks::base
{
    using ::ilias::Error;
    using ::ilias::IoTask;
    using ::ilias::Task;
    using ::ilias::Unexpected;
    WinMKCapture *WinMKCapture::g_self = nullptr;

    auto WinMKCapture::name() -> const char *
    {
        return "WinMKCapture";
    }

    auto WinMKCapture::start() -> ::ilias::Task<int>
    {
        _mosueHook = SetWindowsHookExW(
            WH_MOUSE_LL,
            [](int ncode, WPARAM wp, LPARAM lp) { return g_self->_mouse_proc(ncode, wp, lp); },
            nullptr, 0);
        if (_mosueHook == nullptr) {
            SPDLOG_ERROR("SetWindowsHookExW failed: {}", GetLastError());
            co_return GetLastError();
        }

        _screenWidth  = GetSystemMetrics(SM_CXSCREEN);
        _screenHeight = GetSystemMetrics(SM_CYSCREEN);
        co_return co_await MKCapture::start();
    }

    auto WinMKCapture::stop() -> ::ilias::Task<int>
    {
        notify();
        UnhookWindowsHookEx(_mosueHook);
        co_return co_await MKCapture::stop();
    }

    auto WinMKCapture::start_capture() -> ::ilias::Task<int>
    {
        _keyboardHook = SetWindowsHookExW(
            WH_KEYBOARD_LL,
            [](int ncode, WPARAM wp, LPARAM lp) { return g_self->_keyboard_proc(ncode, wp, lp); },
            nullptr, 0);
        if (_keyboardHook == nullptr) {
            SPDLOG_ERROR("SetWindowsHookExW failed: {}", GetLastError());
            co_return GetLastError();
        }
        SetCursorPos(0, 0);
        _isStartCapture = true;
        co_return 0;
    }

    auto WinMKCapture::stop_capture() -> ::ilias::Task<int>
    {
        UnhookWindowsHookEx(_keyboardHook);
        _isStartCapture = false;
        co_return 0;
    }

    auto WinMKCapture::get_event() -> IoTask<NekoProto::IProto>
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

    auto WinMKCapture::notify() -> void
    {
        _syncEvent.set();
    }

    WinMKCapture::WinMKCapture(::ilias::IoContext *ctx) : MKCapture(ctx), _events(10)
    {
        if (g_self != nullptr) {
            SPDLOG_ERROR("WinMKcapture can't be created more than one");
            throw std::runtime_error("WinMKCapture already exists");
        }
        g_self = this;
    }

    WinMKCapture::~WinMKCapture()
    {
        g_self = nullptr;
        stop().wait();
    }

    LRESULT WinMKCapture::_mouse_proc(int ncode, WPARAM wp, LPARAM lp)
    {
        auto             *hookStruct = reinterpret_cast<MSLLHOOKSTRUCT *>(lp);
        NekoProto::IProto proto;
        if (!_isStartCapture) {
            if (wp == WM_MOUSEMOVE) {
                uint32_t border = 0;
                if (hookStruct->pt.x <= 0) {
                    border |= (uint32_t)BorderEvent::eLeft;
                }
                if (hookStruct->pt.x >= _screenWidth) {
                    border |= (uint32_t)BorderEvent::eRight;
                }
                if (hookStruct->pt.y <= 0) {
                    border |= (uint32_t)BorderEvent::eTop;
                }
                if (hookStruct->pt.y >= _screenHeight) {
                    border |= (uint32_t)BorderEvent::eBottom;
                }
                if (border != 0) {
                    SPDLOG_INFO("Mouse Border: {}", border);
                    proto = mks::BorderEvent::emplaceProto(0U, border);
                }
            }
        }
        else {
            switch (wp) {
            case WM_LBUTTONDOWN:
                proto = mks::MouseButtonEvent::emplaceProto(mks::MouseButtonState::eButtonDown,
                                                            mks::MouseButton::eButtonLeft,
                                                            (uint8_t)1, (uint64_t)hookStruct->time);
                SPDLOG_INFO("Left Mouse Button Down");
                break;
            case WM_LBUTTONUP:
                proto = mks::MouseButtonEvent::emplaceProto(mks::MouseButtonState::eButtonUp,
                                                            mks::MouseButton::eButtonLeft,
                                                            (uint8_t)1, (uint64_t)hookStruct->time);
                SPDLOG_INFO("Left Mouse Button Up");
                break;
            case WM_LBUTTONDBLCLK:
                proto = mks::MouseButtonEvent::emplaceProto(mks::MouseButtonState::eClick,
                                                            mks::MouseButton::eButtonLeft,
                                                            (uint8_t)2, (uint64_t)hookStruct->time);
                SPDLOG_INFO("Left Mouse Button Double Click");
                break;
            case WM_RBUTTONDOWN:
                proto = mks::MouseButtonEvent::emplaceProto(mks::MouseButtonState::eButtonDown,
                                                            mks::MouseButton::eButtonRight,
                                                            (uint8_t)1, (uint64_t)hookStruct->time);
                SPDLOG_INFO("Right Mouse Button Down");
                break;
            case WM_RBUTTONUP:
                proto = mks::MouseButtonEvent::emplaceProto(mks::MouseButtonState::eButtonUp,
                                                            mks::MouseButton::eButtonRight,
                                                            (uint8_t)1, (uint64_t)hookStruct->time);
                SPDLOG_INFO("Right Mouse Button Up");
                break;
            case WM_RBUTTONDBLCLK:
                proto = mks::MouseButtonEvent::emplaceProto(mks::MouseButtonState::eClick,
                                                            mks::MouseButton::eButtonRight,
                                                            (uint8_t)2, (uint64_t)hookStruct->time);
                SPDLOG_INFO("Right Mouse Button Double Click");
                break;
            case WM_MBUTTONDOWN:
                proto = mks::MouseButtonEvent::emplaceProto(mks::MouseButtonState::eButtonDown,
                                                            mks::MouseButton::eButtonMiddle,
                                                            (uint8_t)1, (uint64_t)hookStruct->time);
                SPDLOG_INFO("Middle Mouse Button Down");
                break;
            case WM_MBUTTONUP:
                proto = mks::MouseButtonEvent::emplaceProto(mks::MouseButtonState::eButtonUp,
                                                            mks::MouseButton::eButtonMiddle,
                                                            (uint8_t)1, (uint64_t)hookStruct->time);
                SPDLOG_INFO("Middle Mouse Button Up");
                break;
            case WM_MBUTTONDBLCLK:
                proto = mks::MouseButtonEvent::emplaceProto(mks::MouseButtonState::eClick,
                                                            mks::MouseButton::eButtonMiddle,
                                                            (uint8_t)2, (uint64_t)hookStruct->time);
                SPDLOG_INFO("Middle Mouse Button Double Click");
                break;
            case WM_MOUSEMOVE:
                proto = mks::MouseMotionEvent::emplaceProto(
                    (float)hookStruct->pt.x / _screenWidth, (float)hookStruct->pt.y / _screenHeight,
                    !_isStartCapture, (uint64_t)hookStruct->time);
                SPDLOG_INFO("Mouse Move X: {} Y: {}", hookStruct->pt.x, hookStruct->pt.y);
                break;
            case WM_MOUSEWHEEL:
                proto = mks::MouseWheelEvent::emplaceProto(
                    (float)(GET_WHEEL_DELTA_WPARAM(hookStruct->mouseData) / WHEEL_DELTA), 0.F,
                    (uint64_t)hookStruct->time);
                SPDLOG_INFO("Mouse Wheel {}", GET_WHEEL_DELTA_WPARAM(hookStruct->mouseData));
                break;
            case WM_MOUSEHWHEEL:
                proto = mks::MouseWheelEvent::emplaceProto(
                    0.F, (float)(GET_WHEEL_DELTA_WPARAM(hookStruct->mouseData) / WHEEL_DELTA),
                    (uint64_t)hookStruct->time);
                SPDLOG_INFO("Mouse HWheel {}", GET_WHEEL_DELTA_WPARAM(hookStruct->mouseData));
            default:
                break;
            }
        }
        if (proto != nullptr) {
            _events.push(std::move(proto));
            _syncEvent.set();
        }
        return (!_isStartCapture || ncode < 0) ? CallNextHookEx(nullptr, ncode, wp, lp) : 1;
    }

    LRESULT WinMKCapture::_keyboard_proc(int ncode, WPARAM wp, LPARAM lp)
    {
        auto *hookStruct = reinterpret_cast<KBDLLHOOKSTRUCT *>(lp);
    #if !defined(NDEBUG)
        char name[32]{0};
        GetKeyNameTextA(hookStruct->scanCode << 16, name, 256);
        switch (wp) {
        case WM_KEYDOWN:
            SPDLOG_INFO("Key {} Down ", name);
            break;
        case WM_KEYUP:
            SPDLOG_INFO("Key {} Up ", name);
        }
        if (name[0] == 'Q' || name[0] == 'q') {
            _isStartCapture = false;
        }
    #endif
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
            keymod, (uint64_t)hookStruct->time));
        _syncEvent.set();
        return (!_isStartCapture || ncode < 0) ? CallNextHookEx(nullptr, ncode, wp, lp) : 1;
    }
} // namespace mks::base
#endif