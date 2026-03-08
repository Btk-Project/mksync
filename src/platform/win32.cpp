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
class Win32Platform;

namespace {
    static thread_local constinit Win32InputCapture *inputCapture = nullptr;
    static thread_local constinit ::HHOOK keyboardHook = nullptr;
    static thread_local constinit ::HHOOK mouseHook = nullptr;
    static constinit std::once_flag windowRegistry {};
    static constexpr auto WM_PLATFORM_CALL = WM_USER + 1;
}

struct Win32Monitor {
public:
    uint32_t         index {}; // 0-based index
    ::MONITORINFOEXW info;
    std::string      name;
};

// Impl win32
class Win32Platform final : public Platform, public std::enable_shared_from_this<Win32Platform> {
public:
    Win32Platform() {
        auto latch = std::latch {1};
        mThread = std::jthread([&](std::stop_token token) { main(token, &latch); });
        latch.wait();
    }

    ~Win32Platform() {
        mThread.request_stop();
        mThread.join();
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

    // Schedule a call to the UI thread
    template <std::invocable Fn>
    auto uiCall(Fn fn) -> void {
        auto handler = +[](void *f) {
            auto fn = reinterpret_cast<Fn *>(f);
            (*fn)();
            delete fn;
        };
        auto f = new Fn {std::move(fn)};
        ::PostMessageW(mMessageWindow, WM_PLATFORM_CALL, reinterpret_cast<WPARAM>(handler), reinterpret_cast<LPARAM>(f));
    }

    // Factory ...
    auto createInputCapture() -> InputCapture::Ptr override;
    auto createInputInjector() -> InputInjector::Ptr override;
private:
    auto main(std::stop_token token, std::latch *latch) -> void {
        SPDLOG_INFO("Starting win32 platform thread");
        do {
            if (!::IsGUIThread(TRUE)) {
                break;
            }
            if (!::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
                break;
            }
            if (!initializeWindow()) {
                break;
            }
            if (!enumerateMonitors()) {
                break;
            }
            latch->count_down();
            mainLoop(token);
        }
        while (0);

        // Cleanup
        if (keyboardHook) {
            ::UnhookWindowsHookEx(keyboardHook);
        }
        if (mouseHook) {
            ::UnhookWindowsHookEx(mouseHook);
        }
        if (mMessageWindow) {
            ::DestroyWindow(mMessageWindow);
            mMessageWindow = nullptr;
        }
        SPDLOG_INFO("Stopping win32 platform thread");
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
                    .lpfnWndProc = Win32Platform::messageProc,
                    .lpszClassName = L"mksync::win32::Win32Platform"
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
            L"mksync::win32::Win32Platform",
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
        ::SetWindowLongPtrW(mMessageWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        return true;
    }

    // Enumerate monitors, fill mMonitors and mMonitorsMapping
    auto enumerateMonitors() -> bool {
        mMonitors.clear();
        mMonitorsMapping.clear();
        auto ok = ::EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) -> BOOL {
            auto self = reinterpret_cast<Win32Platform *>(dwData);
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
        return true;
    }

    static auto CALLBACK messageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
        auto self = reinterpret_cast<Win32Platform *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!self) { // Not fully initialized
            return ::DefWindowProcW(hwnd, msg, wp, lp);
        }
        switch (msg) {
            case WM_DISPLAYCHANGE: {
                SPDLOG_INFO("Display change");
                break;
            }
            case WM_PLATFORM_CALL: {
                auto fn = reinterpret_cast<void (*)(void *)>(wp);
                auto args = reinterpret_cast<void *>(lp);
                fn(args);
                break;
            }
        }
        return ::DefWindowProcW(hwnd, msg, wp, lp);
    }
    
    // Message Window
    std::jthread mThread;
    HWND         mMessageWindow = nullptr;

    // Child Object
    std::weak_ptr<Win32InputCapture> mInputCapture;

    // Monitor mapping ...
    std::vector<Win32Monitor>                    mMonitors;
    std::unordered_map<HMONITOR, Win32Monitor *> mMonitorsMapping;
};

class Win32InputCapture final : public InputCapture {
public:
    Win32InputCapture(std::shared_ptr<Win32Platform> platform) : mPlatform(std::move(platform)) {
        
    }
    ~Win32InputCapture() {
        // Cleanup if 
    }

    auto initialize() -> IoTask<void> override {
        // Cache 100 events, i think it should be enough
        auto [sender, receiver] = ilias::mpsc::channel<InputEvent>(100);
        auto latch = ilias::Latch {1};
        auto err = DWORD {0};
        mSender = std::move(sender);
        mReceiver = std::move(receiver);
        mPlatform->uiCall([this, &latch, &err]() {
            if (!hook()) {
                err = ::GetLastError();
            }
            latch.countDown();
        });
        co_await ilias::unstoppable(latch.wait());
        if (err != 0) {
            co_return Err(std::error_code {static_cast<int>(err), std::system_category()});
        }
        co_return {};
    }

    auto shutdown() -> Task<void> override {
        auto latch = ilias::Latch {1};
        mPlatform->uiCall([this, &latch]() {
            unhook();
            latch.countDown();
        });
        co_await ilias::unstoppable(latch.wait());
        co_return;
    }

    auto nextEvent() -> IoTask<InputEvent> override {
        if (!mReceiver) {
            co_return Err(make_error_code(std::errc::invalid_argument));
        }
        if (auto res = co_await mReceiver.recv(); res) {
            co_return std::move(*res);
        }
        co_return Err(make_error_code(std::errc::operation_canceled));
    }
private:
    auto hook() -> bool {
        inputCapture = this;
        do {
            mouseHook = ::SetWindowsHookExW(WH_MOUSE_LL, Win32InputCapture::mouseHookProc, ::GetModuleHandleW(nullptr), 0);
            if (!mouseHook) {
                break;
            }
            keyboardHook = ::SetWindowsHookExW(WH_KEYBOARD_LL, Win32InputCapture::keyboardHookProc, ::GetModuleHandleW(nullptr), 0);
            if (!keyboardHook) {
                break;
            }
            SPDLOG_INFO("InputCapture hooked");
            return true;
        }
        while (0);
        unhook();
        return false;
    }

    auto unhook() -> void {
        if (mouseHook) {
            ::UnhookWindowsHookEx(mouseHook);
            mouseHook = nullptr;
        }
        if (keyboardHook) {
            ::UnhookWindowsHookEx(keyboardHook);
            keyboardHook = nullptr;
        }
        inputCapture = nullptr;
        SPDLOG_INFO("InputCapture unhooked");
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

    static auto translateMouseEvent(WPARAM wp, MSLLHOOKSTRUCT *info) -> std::optional<InputEvent> {
        auto [screnIndex, point] = inputCapture->mPlatform->globalToMonitor(info->pt);
        auto inputEvent = InputEvent {
            .type = InputEvent::None,
            .mouse = {
                .x = point.x,
                .y = point.y,
            }
        };
        // SPDLOG_INFO("Mouse hook: global({}, {}) -> monitor({}: {}, {})", info->pt.x, info->pt.y, screnIndex, point.x, point.y);
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

    // UiThread -> channel -> InputCapture
    ilias::mpsc::Sender<InputEvent>   mSender;
    ilias::mpsc::Receiver<InputEvent> mReceiver;
    std::shared_ptr<Win32Platform>    mPlatform;
};

auto Win32Platform::createInputCapture() -> InputCapture::Ptr {
    if (!mInputCapture.expired()) {
        throw std::runtime_error("InputCapture already created");
    }
    auto capture = std::make_shared<Win32InputCapture>(shared_from_this());
    mInputCapture = capture;
    return capture;
}

auto Win32Platform::createInputInjector() -> InputInjector::Ptr {
    // return std::make_shared<Win32InputInjector>(shared_from_this());
    return nullptr;
}

auto createPlatform() -> Platform::Ptr {
    return std::make_shared<Win32Platform>();
}

}