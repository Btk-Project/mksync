#define WIN32_LEAN_AND_MEAN
#include <spdlog/spdlog.h>
#include <ilias/task.hpp>
#include <ilias/sync.hpp>
#include <stop_token>
#include <ranges>
#include <thread>
#include <latch>

#include "platform.hpp"
#include <Windows.h>

namespace mksync::platform {

class Win32InputCapture;

namespace {
    static thread_local constinit Win32InputCapture* inputCapture = nullptr;
    static thread_local constinit ::HHOOK keyboardHook = nullptr;
    static thread_local constinit ::HHOOK mouseHook = nullptr;
    static constinit std::once_flag windowRegistry {};
}

struct Win32Monitor {
public:
    uint32_t         index {}; // 0-based index
    ::MONITORINFOEXW info;
    std::string      name;
};

// Impl win32
class Win32InputCapture final : public InputCapture {
public:
    auto initialize() -> IoTask<void> override {
        if (mThread.joinable()) { // Already initialized
            co_return {};
        }
        // Cache 100 events, i think it should be enough
        auto [sender, receiver] = ilias::mpsc::channel<InputEvent>(100);
        mSender = std::move(sender);
        mReceiver = std::move(receiver);
        mThread = std::jthread([this](std::stop_token token) { main(token); });
        co_return {};
    }

    auto shutdown() -> Task<void> override {
        mThread.request_stop();
        mThread.join();
        co_return;
    }

    auto nextEvent() -> IoTask<InputEvent> override {
        if (!mThread.joinable() || !mReceiver) {
            co_return Err(make_error_code(std::errc::invalid_argument));
        }
        if (auto res = co_await mReceiver.recv(); res) {
            co_return std::move(*res);
        }
        co_return Err(make_error_code(std::errc::operation_canceled));
    }
private:
    auto main(std::stop_token token) -> void {
        SPDLOG_INFO("Starting input capture thread");
        inputCapture = this;
        do {
            if (!::IsGUIThread(TRUE)) {
                break;
            }
            if (!initializeWindow()) {
                break;
            }
            if (!enumerateMonitors()) {
                break;
            }
            
            keyboardHook = ::SetWindowsHookExW(WH_KEYBOARD_LL, &Win32InputCapture::keyboardHookProc, nullptr, 0);
            mouseHook = ::SetWindowsHookExW(WH_MOUSE_LL, &Win32InputCapture::mouseHookProc, nullptr, 0);
            if (keyboardHook == nullptr || mouseHook == nullptr) {
                SPDLOG_ERROR("Failed to set hooks: {}", GetLastError());
                break;
            }
            mainLoop(token);
        }
        while (0);

        // Cleanup
        if (keyboardHook != nullptr) {
            ::UnhookWindowsHookEx(keyboardHook);
        }
        if (mouseHook != nullptr) {
            ::UnhookWindowsHookEx(mouseHook);
        }
        if (mMessageWindow) {
            ::DestroyWindow(mMessageWindow);
            mMessageWindow = nullptr;
        }
        SPDLOG_INFO("Stopping input capture thread");
    }

    auto mainLoop(std::stop_token token) -> void {
        std::stop_callback callback {token, [id = ::GetCurrentThreadId()]() {
            ::PostThreadMessageW(id, WM_QUIT, 0, 0);
        }};
        ::MSG msg {};
        while (!token.stop_requested()) {
            ::GetMessageW(&msg, nullptr, 0, 0);
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }

    // Initialize message window
    auto initializeWindow() -> bool {
        try {
            std::call_once(windowRegistry, []() {
                ::WNDCLASSEXW wc {
                    .cbSize = sizeof(wc),
                    .lpfnWndProc = Win32InputCapture::messageProc,
                    .lpszClassName = L"mksync::win32::InputCapture"
                };
                if (!::RegisterClassExW(&wc)) {
                    throw std::runtime_error("Failed to register window class");
                }
            });
        }
        catch (std::exception &) {
            return false;
        }
        mMessageWindow = ::CreateWindowExW(
            0,
            L"mksync::win32::InputCapture",
            nullptr,
            0,
            0, 0, 0, 0,
            HWND_MESSAGE,
            nullptr,
            ::GetModuleHandleW(nullptr),
            nullptr
        );
        if (!mMessageWindow) {
            return false;
        }
        return true;
    }

    // Enumerate monitors, fill mMonitors and mMonitorsMapping
    auto enumerateMonitors() -> bool {
        mMonitors.clear();
        mMonitorsMapping.clear();
        auto ok = ::EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) -> BOOL {
            auto self = reinterpret_cast<Win32InputCapture *>(dwData);
            auto info = ::MONITORINFOEXW {};
            info.cbSize = sizeof(info);
            if (!::GetMonitorInfoW(hMonitor, &info)) {
                return FALSE;
            }

            self->mMonitors.emplace_back(Win32Monitor {
                .index = static_cast<uint32_t>(self->mMonitors.size()), // 0-based
                .info = info
            });
            self->mMonitorsMapping[hMonitor] = &self->mMonitors.back();
            return TRUE;
        }, reinterpret_cast<LPARAM>(this));
        if (!ok) {
            return false;  
        }

        for (auto [idx, monitor] : std::views::enumerate(mMonitors)) {
            auto rect = monitor.info.rcMonitor;
            SPDLOG_INFO("Monitor {}: size: {}, {}", idx, rect.right - rect.left, rect.bottom - rect.top);
        }
        // Send to the receiver
        auto _ = mSender.blockingSend(InputEvent {
            .type = InputEvent::ScreenChange    
        });
        return true;
    }

    // Map global coordinates to monitor coordinates (index, point)
    auto globalToMonitor(POINT pt) -> std::pair<uint32_t, POINT> {
        auto hMonitor = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        auto it = mMonitorsMapping.find(hMonitor);
        if (it == mMonitorsMapping.end()) {
            throw std::runtime_error("Unknown monitor");
        }
        auto [_, monitor] = *it;
        auto &info = monitor->info;

        // int monitorWidth = info.rcMonitor.right - info.rcMonitor.left;
        // int monitorHeight = info.rcMonitor.bottom - info.rcMonitor.top;

        int x = pt.x - info.rcMonitor.left;
        int y = pt.y - info.rcMonitor.top;
        return std::pair {
            monitor->index,
            POINT {x, y}
        };
    }

    static auto CALLBACK mouseHookProc(int ncode, WPARAM wp, LPARAM lp) -> LRESULT {
        auto info = reinterpret_cast<MSLLHOOKSTRUCT *>(lp);
        // SPDLOG_INFO("Mouse hook: x={}, y={}, flags={}", info->pt.x, info->pt.y, info->flags);
        if (auto event = translateMouseEvent(wp, info); event) {
            auto _ = inputCapture->mSender.blockingSend(std::move(*event));
        }
        return ::CallNextHookEx(nullptr, ncode, wp, lp);
    }

    static auto CALLBACK keyboardHookProc(int ncode, WPARAM wp, LPARAM lp) -> LRESULT {
        return ::CallNextHookEx(nullptr, ncode, wp, lp);
    }

    static auto CALLBACK messageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
        switch (msg) {
            case WM_DISPLAYCHANGE: {
                SPDLOG_INFO("Display change");
                inputCapture->enumerateMonitors();
                break;
            }
        }
        return ::DefWindowProcW(hwnd, msg, wp, lp);
    }

    static auto translateMouseEvent(WPARAM wp, MSLLHOOKSTRUCT *info) -> std::optional<InputEvent> {
        auto [screnIndex, point] = inputCapture->globalToMonitor(info->pt);
        auto inputEvent = InputEvent {
            .type = InputEvent::None,
            .mouse = {
                .x = point.x,
                .y = point.y,
            }
        };
        SPDLOG_INFO("Mouse hook: global({}, {}) -> monitor({}: {}, {})", info->pt.x, info->pt.y, screnIndex, point.x, point.y);
        switch (wp) {
            case WM_MOUSEMOVE: inputEvent.type = InputEvent::MouseMove; break;
            case WM_LBUTTONDOWN: inputEvent.type = InputEvent::MousePress; inputEvent.mouse.button = MouseButton::Left; break;
            case WM_LBUTTONUP: inputEvent.type = InputEvent::MouseRelease; inputEvent.mouse.button = MouseButton::Left; break;
            case WM_RBUTTONDOWN: inputEvent.type = InputEvent::MousePress; inputEvent.mouse.button = MouseButton::Right; break;
            case WM_RBUTTONUP: inputEvent.type = InputEvent::MouseRelease; inputEvent.mouse.button = MouseButton::Right; break;
            case WM_MBUTTONDOWN: inputEvent.type = InputEvent::MousePress; inputEvent.mouse.button = MouseButton::Middle; break;
            case WM_MBUTTONUP: inputEvent.type = InputEvent::MouseRelease; inputEvent.mouse.button = MouseButton::Middle; break;
            default: return std::nullopt;
        }
        return inputEvent;
    }

    // InputCaptureThread -> channel -> InputCapture
    ilias::mpsc::Sender<InputEvent>   mSender;
    ilias::mpsc::Receiver<InputEvent> mReceiver;
    std::jthread                      mThread;

    // Message Window
    HWND                              mMessageWindow = nullptr;

    // Monitor mapping ...
    std::vector<Win32Monitor>                    mMonitors;
    std::unordered_map<HMONITOR, Win32Monitor *> mMonitorsMapping;
};

auto createInputCapture() -> InputCapture::Ptr {
    return std::make_shared<Win32InputCapture>();
}

}