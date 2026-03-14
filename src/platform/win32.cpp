#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <latch>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <ilias/sync.hpp>
#include <ilias/task.hpp>
#include <spdlog/spdlog.h>

#include "platform.hpp"

namespace mksync::platform {

class Win32InputCapture;
class Win32Platform;

namespace {
    static thread_local constinit Win32InputCapture *gInputCapture = nullptr;
    static thread_local constinit HHOOK gKeyboardHook = nullptr;
    static thread_local constinit HHOOK gMouseHook = nullptr;
    static constinit std::once_flag gWindowRegistry {};
    static constexpr auto WM_PLATFORM_CALL = WM_USER + 1;

    auto lastErrorCode() -> std::error_code {
        return {static_cast<int>(::GetLastError()), std::system_category()};
    }
}

struct Win32Monitor {
    uint32_t index = 0;
    HMONITOR handle = nullptr;
    MONITORINFOEXW info {};
    ScreenInfo screen {};
};

class Win32Platform final : public Platform, public std::enable_shared_from_this<Win32Platform> {
public:
    Win32Platform() {
        auto latch = std::latch {1};
        mThread = std::jthread([this, &latch](std::stop_token token) { main(token, &latch); });
        latch.wait();

        if (!mStartupSucceeded) {
            if (mThread.joinable()) {
                mThread.request_stop();
                mThread.join();
            }
            throw std::runtime_error(mStartupError.empty() ? "Failed to initialize Win32 platform" : mStartupError);
        }
    }

    ~Win32Platform() {
        if (mThread.joinable()) {
            mThread.request_stop();
            mThread.join();
        }
    }

    auto screens() const -> std::vector<ScreenInfo> override {
        auto lock = std::scoped_lock(mMonitorMutex);
        std::vector<ScreenInfo> result;
        result.reserve(mMonitors.size());
        for (const auto &monitor : mMonitors) {
            result.push_back(monitor.screen);
        }
        return result;
    }

    auto globalToMonitor(POINT pt) const -> std::pair<uint32_t, POINT> {
        auto hMonitor = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        auto lock = std::scoped_lock(mMonitorMutex);
        auto it = mMonitorMapping.find(hMonitor);
        if (it == mMonitorMapping.end()) {
            throw std::runtime_error("Unknown monitor");
        }

        const auto &monitor = mMonitors[it->second];
        const auto &rect = monitor.info.rcMonitor;
        return {
            monitor.index,
            POINT {
                pt.x - rect.left,
                pt.y - rect.top,
            }
        };
    }

    template <std::invocable Fn>
    auto uiCall(Fn fn) -> void {
        auto *callable = new Fn(std::move(fn));
        auto handler = +[](void *ptr) {
            auto owned = std::unique_ptr<Fn>(reinterpret_cast<Fn *>(ptr));
            (*owned)();
        };
        if (!::PostMessageW(mMessageWindow, WM_PLATFORM_CALL, reinterpret_cast<WPARAM>(handler), reinterpret_cast<LPARAM>(callable))) {
            delete callable;
            throw std::system_error(lastErrorCode(), "Failed to post call to Win32 platform thread");
        }
    }

    auto createInputCapture() -> InputCapture::Ptr override;
    auto createInputInjector() -> InputInjector::Ptr override;

private:
    auto main(std::stop_token token, std::latch *latch) -> void {
        auto startupSignaled = false;
        auto signalStartup = [&]() {
            if (!startupSignaled) {
                startupSignaled = true;
                latch->count_down();
            }
        };
        auto failStartup = [&](std::string message) {
            mStartupError = std::move(message);
            mStartupSucceeded = false;
            signalStartup();
        };

        SPDLOG_INFO("Starting win32 platform thread");

        do {
            if (!::IsGUIThread(TRUE)) {
                failStartup("Failed to initialize GUI thread state");
                break;
            }

            if (!::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
                SPDLOG_WARN("SetProcessDpiAwarenessContext failed: {}", lastErrorCode().message());
            }

            if (!initializeWindow()) {
                failStartup("Failed to create Win32 message window");
                break;
            }
            if (!enumerateMonitors()) {
                failStartup("Failed to enumerate Win32 monitors");
                break;
            }

            mStartupSucceeded = true;
            signalStartup();
            mainLoop(token);
        }
        while (false);

        signalStartup();
        cleanup();
        SPDLOG_INFO("Stopping win32 platform thread");
    }

    auto mainLoop(std::stop_token token) -> void {
        std::stop_callback callback {token, [threadId = ::GetCurrentThreadId()]() {
            ::PostThreadMessageW(threadId, WM_QUIT, 0, 0);
        }};

        MSG msg {};
        while (!token.stop_requested()) {
            auto status = ::GetMessageW(&msg, nullptr, 0, 0);
            if (status == -1) {
                SPDLOG_ERROR("GetMessageW failed: {}", lastErrorCode().message());
                break;
            }
            if (status == 0) {
                break;
            }
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }

    auto cleanup() -> void {
        if (gKeyboardHook) {
            ::UnhookWindowsHookEx(gKeyboardHook);
            gKeyboardHook = nullptr;
        }
        if (gMouseHook) {
            ::UnhookWindowsHookEx(gMouseHook);
            gMouseHook = nullptr;
        }
        gInputCapture = nullptr;

        if (mMessageWindow) {
            ::DestroyWindow(mMessageWindow);
            mMessageWindow = nullptr;
        }
    }

    auto initializeWindow() -> bool {
        try {
            std::call_once(gWindowRegistry, []() {
                WNDCLASSEXW wc {
                    .cbSize = sizeof(wc),
                    .lpfnWndProc = Win32Platform::messageProc,
                    .lpszClassName = L"mksync::win32::Win32Platform"
                };
                if (!::RegisterClassExW(&wc)) {
                    throw std::system_error(lastErrorCode(), "Failed to register Win32Platform class");
                }
            });
        }
        catch (const std::exception &ex) {
            SPDLOG_ERROR("{}", ex.what());
            return false;
        }

        mMessageWindow = ::CreateWindowExW(
            0,
            L"mksync::win32::Win32Platform",
            nullptr,
            0,
            0,
            0,
            0,
            0,
            HWND_MESSAGE,
            nullptr,
            ::GetModuleHandleW(nullptr),
            nullptr
        );
        if (!mMessageWindow) {
            SPDLOG_ERROR("CreateWindowExW failed: {}", lastErrorCode().message());
            return false;
        }

        ::SetWindowLongPtrW(mMessageWindow, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        return true;
    }

    auto enumerateMonitors() -> bool {
        using MonitorEnumPayload = std::pair<std::vector<Win32Monitor> *, std::unordered_map<HMONITOR, size_t> *>;

        std::vector<Win32Monitor> monitors;
        std::unordered_map<HMONITOR, size_t> mapping;
        auto payload = MonitorEnumPayload {&monitors, &mapping};

        auto ok = ::EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMonitor, HDC, LPRECT, LPARAM data) -> BOOL {
            auto *payload = reinterpret_cast<MonitorEnumPayload *>(data);
            auto &[monitorsRef, mappingRef] = *payload;

            MONITORINFOEXW info {};
            info.cbSize = sizeof(info);
            if (!::GetMonitorInfoW(hMonitor, &info)) {
                return FALSE;
            }

            const auto &rect = info.rcMonitor;
            Win32Monitor monitor {
                .index = static_cast<uint32_t>(monitorsRef->size()),
                .handle = hMonitor,
                .info = info,
                .screen = ScreenInfo {
                    .x = rect.left,
                    .y = rect.top,
                    .width = rect.right - rect.left,
                    .height = rect.bottom - rect.top,
                    .dpi = 72,
                    .name = [&]() {
                        auto len = static_cast<int>(wcsnlen(info.szDevice, CCHDEVICENAME));
                        auto needed = ::WideCharToMultiByte(CP_UTF8, 0, info.szDevice, len, nullptr, 0, nullptr, nullptr);
                        std::string value(static_cast<size_t>(needed), '\0');
                        if (needed > 0) {
                            ::WideCharToMultiByte(CP_UTF8, 0, info.szDevice, len, value.data(), needed, nullptr, nullptr);
                        }
                        return value;
                    }(),
                    .primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0,
                }
            };

            mappingRef->emplace(hMonitor, monitorsRef->size());
            monitorsRef->push_back(std::move(monitor));
            return TRUE;
        }, reinterpret_cast<LPARAM>(&payload));

        if (!ok) {
            SPDLOG_ERROR("EnumDisplayMonitors failed: {}", lastErrorCode().message());
            return false;
        }

        {
            auto lock = std::scoped_lock(mMonitorMutex);
            mMonitors = std::move(monitors);
            mMonitorMapping = std::move(mapping);
        }

        for (const auto &monitor : screens()) {
            SPDLOG_INFO(
                "Monitor {} at ({}, {}) size {}x{} primary={}",
                monitor.name,
                monitor.x,
                monitor.y,
                monitor.width,
                monitor.height,
                monitor.primary
            );
        }
        return true;
    }

    static auto CALLBACK messageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT;

    std::jthread mThread;
    HWND mMessageWindow = nullptr;

    bool mStartupSucceeded = false;
    std::string mStartupError;

    std::weak_ptr<Win32InputCapture> mInputCapture;

    mutable std::mutex mMonitorMutex;
    std::vector<Win32Monitor> mMonitors;
    std::unordered_map<HMONITOR, size_t> mMonitorMapping;
};

class Win32InputCapture final : public InputCapture {
public:
    explicit Win32InputCapture(std::shared_ptr<Win32Platform> platform) : mPlatform(std::move(platform)) {}

    auto initialize() -> IoTask<void> override {
        auto [sender, receiver] = ilias::mpsc::channel<InputEvent>(512);
        auto latch = ilias::Latch {1};
        auto error = std::error_code {};

        mDroppedEvents.store(0, std::memory_order_relaxed);
        mSender = std::move(sender);
        mReceiver = std::move(receiver);

        mPlatform->uiCall([this, &latch, &error]() {
            if (!hook()) {
                error = lastErrorCode();
            }
            latch.countDown();
        });

        co_await ilias::unstoppable(latch.wait());
        if (error) {
            co_return Err(error);
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

        if (auto dropped = mDroppedEvents.exchange(0, std::memory_order_relaxed); dropped != 0) {
            SPDLOG_WARN("Dropped {} input events because the capture queue was full", dropped);
        }

        if (auto res = co_await mReceiver.recv(); res) {
            co_return std::move(*res);
        }
        co_return Err(make_error_code(std::errc::operation_canceled));
    }

    auto notifyScreenTopologyChanged() -> void {
        tryEnqueue(InputEvent {
            .type = InputEvent::Type::ScreenChange,
            .metadata = {
                .timestampUs = timestampNowUs(),
                .screenIndex = 0,
                .injected = false,
            },
            .payload = InputEvent::ScreenChangeData {
                .screenCount = static_cast<uint32_t>(mPlatform->screens().size()),
            }
        });
    }

private:
    auto hook() -> bool {
        gInputCapture = this;

        do {
            gMouseHook = ::SetWindowsHookExW(WH_MOUSE_LL, Win32InputCapture::mouseHookProc, ::GetModuleHandleW(nullptr), 0);
            if (!gMouseHook) {
                break;
            }

            gKeyboardHook = ::SetWindowsHookExW(WH_KEYBOARD_LL, Win32InputCapture::keyboardHookProc, ::GetModuleHandleW(nullptr), 0);
            if (!gKeyboardHook) {
                break;
            }

            SPDLOG_INFO("InputCapture hooked");
            return true;
        }
        while (false);

        unhook();
        return false;
    }

    auto unhook() -> void {
        if (gMouseHook) {
            ::UnhookWindowsHookEx(gMouseHook);
            gMouseHook = nullptr;
        }
        if (gKeyboardHook) {
            ::UnhookWindowsHookEx(gKeyboardHook);
            gKeyboardHook = nullptr;
        }
        gInputCapture = nullptr;
        SPDLOG_INFO("InputCapture unhooked");
    }

    auto tryEnqueue(InputEvent event) -> void {
        if (!mSender) {
            return;
        }
        if (auto result = mSender.trySend(std::move(event)); !result) {
            mDroppedEvents.fetch_add(1, std::memory_order_relaxed);
        }
    }

    static auto timestampNowUs() -> uint64_t {
        using namespace std::chrono;
        return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
    }

    static auto timestampFromHook(DWORD hookTimeMs) -> uint64_t {
        return static_cast<uint64_t>(hookTimeMs) * 1000ULL;
    }

    static auto normalizeVirtualKey(DWORD vkCode, DWORD scanCode) -> DWORD {
        switch (vkCode) {
            case VK_SHIFT:
            case VK_CONTROL:
            case VK_MENU:
                return ::MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX);
            default:
                return vkCode;
        }
    }

    static auto keyModifierFor(Key key) -> KeyModifier {
        switch (key) {
            case Key::LeftCtrl: return KeyModifier::LeftCtrl;
            case Key::RightCtrl: return KeyModifier::RightCtrl;
            case Key::LeftShift: return KeyModifier::LeftShift;
            case Key::RightShift: return KeyModifier::RightShift;
            case Key::LeftAlt: return KeyModifier::LeftAlt;
            case Key::RightAlt: return KeyModifier::RightAlt;
            case Key::LeftMeta: return KeyModifier::LeftMeta;
            case Key::RightMeta: return KeyModifier::RightMeta;
            default: return KeyModifier::None;
        }
    }

    static auto currentModifiers() -> KeyModifier {
        auto has = [](int vk) {
            return (::GetAsyncKeyState(vk) & 0x8000) != 0;
        };

        auto modifiers = KeyModifier::None;
        if (has(VK_LCONTROL)) { modifiers |= KeyModifier::LeftCtrl; }
        if (has(VK_RCONTROL)) { modifiers |= KeyModifier::RightCtrl; }
        if (has(VK_LSHIFT))   { modifiers |= KeyModifier::LeftShift; }
        if (has(VK_RSHIFT))   { modifiers |= KeyModifier::RightShift; }
        if (has(VK_LMENU))    { modifiers |= KeyModifier::LeftAlt; }
        if (has(VK_RMENU))    { modifiers |= KeyModifier::RightAlt; }
        if (has(VK_LWIN))     { modifiers |= KeyModifier::LeftMeta; }
        if (has(VK_RWIN))     { modifiers |= KeyModifier::RightMeta; }
        return modifiers;
    }

    static auto translateVirtualKey(DWORD vkCode) -> Key {
        using enum Key;
        if (vkCode >= 'A' && vkCode <= 'Z') {
            return static_cast<Key>(static_cast<uint32_t>(Key::A) + (vkCode - 'A'));
        }
        if (vkCode >= '0' && vkCode <= '9') {
            return static_cast<Key>(static_cast<uint32_t>(Key::Digit0) + (vkCode - '0'));
        }

        switch (vkCode) {
            case VK_RETURN: return Enter;
            case VK_ESCAPE: return Esc;
            case VK_BACK: return Backspace;
            case VK_TAB: return Tab;
            case VK_SPACE: return Space;
            case VK_OEM_MINUS: return Minus;
            case VK_OEM_PLUS: return Equal;
            case VK_OEM_4: return LeftBrace;
            case VK_OEM_6: return RightBrace;
            case VK_OEM_5: return Backslash;
            case VK_OEM_1: return Semicolon;
            case VK_OEM_7: return Apostrophe;
            case VK_OEM_3: return Grave;
            case VK_OEM_COMMA: return Comma;
            case VK_OEM_PERIOD: return Dot;
            case VK_OEM_2: return Slash;
            case VK_CAPITAL: return CapsLock;
            case VK_F1: return F1;
            case VK_F2: return F2;
            case VK_F3: return F3;
            case VK_F4: return F4;
            case VK_F5: return F5;
            case VK_F6: return F6;
            case VK_F7: return F7;
            case VK_F8: return F8;
            case VK_F9: return F9;
            case VK_F10: return F10;
            case VK_F11: return F11;
            case VK_F12: return F12;
            case VK_SNAPSHOT: return SysRq;
            case VK_SCROLL: return ScrollLock;
            case VK_PAUSE: return Pause;
            case VK_INSERT: return Insert;
            case VK_HOME: return Home;
            case VK_PRIOR: return PageUp;
            case VK_DELETE: return Delete;
            case VK_END: return End;
            case VK_NEXT: return PageDown;
            case VK_RIGHT: return Right;
            case VK_LEFT: return Left;
            case VK_DOWN: return Down;
            case VK_UP: return Up;
            case VK_NUMLOCK: return NumLock;
            case VK_DIVIDE: return KeypadSlash;
            case VK_MULTIPLY: return KeypadAsterisk;
            case VK_SUBTRACT: return KeypadMinus;
            case VK_ADD: return KeypadPlus;
            case VK_NUMPAD0: return Keypad0;
            case VK_NUMPAD1: return Keypad1;
            case VK_NUMPAD2: return Keypad2;
            case VK_NUMPAD3: return Keypad3;
            case VK_NUMPAD4: return Keypad4;
            case VK_NUMPAD5: return Keypad5;
            case VK_NUMPAD6: return Keypad6;
            case VK_NUMPAD7: return Keypad7;
            case VK_NUMPAD8: return Keypad8;
            case VK_NUMPAD9: return Keypad9;
            case VK_DECIMAL: return KeypadDot;
            case VK_LCONTROL: return LeftCtrl;
            case VK_RCONTROL: return RightCtrl;
            case VK_LSHIFT: return LeftShift;
            case VK_RSHIFT: return RightShift;
            case VK_LMENU: return LeftAlt;
            case VK_RMENU: return RightAlt;
            case VK_LWIN: return LeftMeta;
            case VK_RWIN: return RightMeta;
            default: return Key::None;
        }
    }

    static auto makeMouseMetadata(const MSLLHOOKSTRUCT *info, uint32_t screenIndex) -> InputEvent::Metadata {
        return InputEvent::Metadata {
            .timestampUs = timestampFromHook(info->time),
            .screenIndex = screenIndex,
            .injected = (info->flags & LLMHF_INJECTED) != 0,
        };
    }

    static auto makeKeyboardMetadata(const KBDLLHOOKSTRUCT *info) -> InputEvent::Metadata {
        return InputEvent::Metadata {
            .timestampUs = timestampFromHook(info->time),
            .screenIndex = 0,
            .injected = (info->flags & LLKHF_INJECTED) != 0,
        };
    }

    static auto CALLBACK mouseHookProc(int code, WPARAM wp, LPARAM lp) -> LRESULT {
        if (code < 0 || !gInputCapture) {
            return ::CallNextHookEx(nullptr, code, wp, lp);
        }

        auto *info = reinterpret_cast<MSLLHOOKSTRUCT *>(lp);
        if (auto event = translateMouseEvent(wp, info); event) {
            gInputCapture->tryEnqueue(std::move(*event));
        }
        return ::CallNextHookEx(nullptr, code, wp, lp);
    }

    static auto CALLBACK keyboardHookProc(int code, WPARAM wp, LPARAM lp) -> LRESULT {
        if (code < 0 || !gInputCapture) {
            return ::CallNextHookEx(nullptr, code, wp, lp);
        }

        auto *info = reinterpret_cast<KBDLLHOOKSTRUCT *>(lp);
        if (auto event = translateKeyboardEvent(wp, info); event) {
            gInputCapture->tryEnqueue(std::move(*event));
        }
        return ::CallNextHookEx(nullptr, code, wp, lp);
    }

    static auto translateMouseEvent(WPARAM wp, const MSLLHOOKSTRUCT *info) -> std::optional<InputEvent> {
        auto [screenIndex, point] = gInputCapture->mPlatform->globalToMonitor(info->pt);
        auto metadata = makeMouseMetadata(info, screenIndex);

        switch (wp) {
            case WM_MOUSEMOVE:
                return InputEvent {
                    .type = InputEvent::Type::MouseMove,
                    .metadata = metadata,
                    .payload = InputEvent::MouseMoveData {
                        .x = point.x,
                        .y = point.y,
                    }
                };
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
                return InputEvent {
                    .type = InputEvent::Type::MousePress,
                    .metadata = metadata,
                    .payload = InputEvent::MouseButtonData {
                        .x = point.x,
                        .y = point.y,
                        .button = wp == WM_LBUTTONDOWN ? MouseButton::Left : (wp == WM_RBUTTONDOWN ? MouseButton::Right : MouseButton::Middle),
                    }
                };
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
                return InputEvent {
                    .type = InputEvent::Type::MouseRelease,
                    .metadata = metadata,
                    .payload = InputEvent::MouseButtonData {
                        .x = point.x,
                        .y = point.y,
                        .button = wp == WM_LBUTTONUP ? MouseButton::Left : (wp == WM_RBUTTONUP ? MouseButton::Right : MouseButton::Middle),
                    }
                };
            case WM_MOUSEWHEEL:
                return InputEvent {
                    .type = InputEvent::Type::MouseWheel,
                    .metadata = metadata,
                    .payload = InputEvent::MouseWheelData {
                        .x = point.x,
                        .y = point.y,
                        .deltaX = 0,
                        .deltaY = static_cast<SHORT>(HIWORD(info->mouseData)),
                    }
                };
            case WM_MOUSEHWHEEL:
                return InputEvent {
                    .type = InputEvent::Type::MouseWheel,
                    .metadata = metadata,
                    .payload = InputEvent::MouseWheelData {
                        .x = point.x,
                        .y = point.y,
                        .deltaX = static_cast<SHORT>(HIWORD(info->mouseData)),
                        .deltaY = 0,
                    }
                };
            default:
                return std::nullopt;
        }
    }

    static auto translateKeyboardEvent(WPARAM wp, const KBDLLHOOKSTRUCT *info) -> std::optional<InputEvent> {
        const bool isPress = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
        const bool isRelease = wp == WM_KEYUP || wp == WM_SYSKEYUP;
        if (!isPress && !isRelease) {
            return std::nullopt;
        }

        auto vkCode = normalizeVirtualKey(info->vkCode, info->scanCode);
        auto key = translateVirtualKey(vkCode);
        auto modifiers = currentModifiers();
        if (auto modifier = keyModifierFor(key); modifier != KeyModifier::None) {
            if (isPress) {
                modifiers |= modifier;
            }
            else {
                modifiers &= ~modifier;
            }
        }

        return InputEvent {
            .type = isPress ? InputEvent::Type::KeyPress : InputEvent::Type::KeyRelease,
            .metadata = makeKeyboardMetadata(info),
            .payload = InputEvent::KeyData {
                .key = key,
                .modifiers = modifiers,
                .nativeCode = vkCode,
                .repeat = false,
            }
        };
    }

    ilias::mpsc::Sender<InputEvent> mSender;
    ilias::mpsc::Receiver<InputEvent> mReceiver;
    std::shared_ptr<Win32Platform> mPlatform;
    std::atomic<uint64_t> mDroppedEvents {0};

    friend class Win32Platform;
};

auto CALLBACK Win32Platform::messageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
    auto *self = reinterpret_cast<Win32Platform *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) {
        return ::DefWindowProcW(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_DISPLAYCHANGE: {
            SPDLOG_INFO("Display change detected");
            if (self->enumerateMonitors() && gInputCapture) {
                gInputCapture->notifyScreenTopologyChanged();
            }
            return 0;
        }
        case WM_PLATFORM_CALL: {
            auto fn = reinterpret_cast<void (*)(void *)>(wp);
            auto *args = reinterpret_cast<void *>(lp);
            fn(args);
            return 0;
        }
        default:
            return ::DefWindowProcW(hwnd, msg, wp, lp);
    }
}
auto Win32Platform::createInputCapture() -> InputCapture::Ptr {
    if (!mInputCapture.expired()) {
        throw std::runtime_error("InputCapture already created");
    }
    auto capture = std::make_shared<Win32InputCapture>(shared_from_this());
    mInputCapture = capture;
    return capture;
}

auto Win32Platform::createInputInjector() -> InputInjector::Ptr {
    return nullptr;
}

auto createPlatform() -> Platform::Ptr {
    try {
        return std::make_shared<Win32Platform>();
    }
    catch (const std::exception &ex) {
        SPDLOG_ERROR("Failed to create Win32 platform: {}", ex.what());
        return nullptr;
    }
}

} // namespace mksync::platform



