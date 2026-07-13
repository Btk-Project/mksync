#ifdef _WIN32
#include "preinclude.hpp"

#include <Windows.h>
#include <ShellScalingApi.h>

#include <array>
#include <atomic>
#include <latch>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
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

#include "backend.hpp"
#include "platform.hpp"
#include "win32_key.hpp"

MKS_BEGIN

// MARK: Forward declarations

class Win32InputCapture;
class Win32Platform;
class Win32InputInjector;

// MARK: Shared helpers

namespace {

// LL hooks and the platform message loop share one UI thread; these are
// thread_local so a second platform instance on another thread cannot clobber them.
thread_local constinit Win32InputCapture *gInputCapture = nullptr;
thread_local constinit HHOOK gKeyboardHook = nullptr;
thread_local constinit HHOOK gMouseHook = nullptr;

// Posted to the platform window so other threads can run code on the UI thread.
constexpr DWORD WM_PLATFORM_CALL = WM_USER + 1;

auto lastErrorCode() -> std::error_code {
    return {static_cast<int>(::GetLastError()), std::system_category()};
}

auto fallbackError() -> std::error_code {
    if (auto error = lastErrorCode(); error) {
        return error;
    }
    return {ERROR_GEN_FAILURE, std::system_category()};
}

auto wideToUtf8(std::wstring_view text) -> std::string {
    if (text.empty()) {
        return {};
    }
    const auto len = static_cast<int>(text.size());
    const auto needed = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), len, nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string value(static_cast<size_t>(needed), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), len, value.data(), needed, nullptr, nullptr);
    return value;
}

auto monitorDpi(HMONITOR monitor) -> int {
    UINT dpiX = 96;
    UINT dpiY = 96;
    if (FAILED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        return 96;
    }
    return static_cast<int>(dpiX);
}

auto sendInputs(std::span<INPUT> inputs) -> std::error_code {
    if (inputs.empty()) {
        return {};
    }
    const auto count = ::SendInput(
        static_cast<UINT>(inputs.size()),
        inputs.data(),
        static_cast<int>(sizeof(INPUT))
    );
    if (count != inputs.size()) {
        return fallbackError();
    }
    return {};
}

auto mouseButtonFromMessage(WPARAM wp) -> MouseButton {
    switch (wp) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDOWN:
            return MouseButton::Left;
        case WM_RBUTTONUP:
        case WM_RBUTTONDOWN:
            return MouseButton::Right;
        case WM_MBUTTONUP:
        case WM_MBUTTONDOWN:
            return MouseButton::Middle;
        default:
            // XBUTTON1/2 are not modeled in MouseButton yet.
            return MouseButton::None;
    }
}

auto isMouseButtonRelease(WPARAM wp) -> bool {
    return wp == WM_LBUTTONUP
        || wp == WM_RBUTTONUP
        || wp == WM_MBUTTONUP
        || wp == WM_XBUTTONUP;
}

} // namespace

// MARK: Monitor state

struct Win32Monitor {
    uint32_t index = 0;
    HMONITOR handle = nullptr;
    MONITORINFOEXW info {};
    ScreenInfo screen {};
};

auto makeMonitor(HMONITOR handle, uint32_t index) -> std::optional<Win32Monitor> {
    MONITORINFOEXW info {};
    info.cbSize = sizeof(info);
    if (!::GetMonitorInfoW(handle, &info)) {
        return std::nullopt;
    }

    const auto &rect = info.rcMonitor;
    return Win32Monitor {
        .index = index,
        .handle = handle,
        .info = info,
        .screen = ScreenInfo {
            .x = rect.left,
            .y = rect.top,
            .width = rect.right - rect.left,
            .height = rect.bottom - rect.top,
            .dpi = monitorDpi(handle),
            .name = wideToUtf8({info.szDevice, wcsnlen(info.szDevice, CCHDEVICENAME)}),
            .primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0,
        },
    };
}

// MARK: Win32Platform

// Owns the dedicated UI thread, hidden top-level window, and monitor topology.
// Capture hooks and PostMessage-based UI work must run on that thread.
class Win32Platform final : public Platform, public std::enable_shared_from_this<Win32Platform> {
public:
    Win32Platform() {
        auto latch = std::latch {1};
        mThread = std::jthread([this, &latch](std::stop_token token) {
            runUiThread(token, latch);
        });
        latch.wait();

        if (mStartupSucceeded) {
            return;
        }

        if (mThread.joinable()) {
            mThread.request_stop();
            mThread.join();
        }
        throw std::runtime_error(
            mStartupError.empty() ? "Failed to initialize Win32 platform" : mStartupError
        );
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

    // Absolute desktop point -> (screenIndex, local pixel inside that monitor).
    auto globalToMonitor(POINT pt) const -> std::pair<uint32_t, POINT> {
        const auto hMonitor = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        auto lock = std::scoped_lock(mMonitorMutex);
        const auto it = mMonitorMapping.find(hMonitor);
        if (it == mMonitorMapping.end()) {
            throw std::runtime_error("Unknown monitor");
        }

        const auto &monitor = mMonitors[it->second];
        const auto &rect = monitor.info.rcMonitor;
        return {
            monitor.index,
            POINT {pt.x - rect.left, pt.y - rect.top},
        };
    }

    // Local pixel on screenIndex -> absolute desktop point for SetCursorPos.
    auto localToGlobal(uint32_t screenIndex, POINT pt) const -> std::optional<POINT> {
        auto lock = std::scoped_lock(mMonitorMutex);
        if (screenIndex >= mMonitors.size()) {
            return std::nullopt;
        }
        const auto &rect = mMonitors[screenIndex].info.rcMonitor;
        return POINT {rect.left + pt.x, rect.top + pt.y};
    }

    // Fire-and-forget: schedule fn on the platform UI thread.
    template <std::invocable Fn>
    auto uiCall(Fn fn) -> void {
        auto *callable = new Fn(std::move(fn));
        auto handler = +[](void *ptr) {
            auto owned = std::unique_ptr<Fn>(reinterpret_cast<Fn *>(ptr));
            (*owned)();
        };
        if (!::PostMessageW(
                mMessageWindow,
                WM_PLATFORM_CALL,
                reinterpret_cast<WPARAM>(handler),
                reinterpret_cast<LPARAM>(callable)
            )) {
            delete callable;
            throw std::system_error(lastErrorCode(), "Failed to post call to Win32 platform thread");
        }
    }

    // Block the caller until fn finishes on the UI thread. fn may return void or error_code.
    template <std::invocable Fn>
    auto syncUiCall(Fn fn) -> std::error_code {
        auto latch = std::latch {1};
        auto error = std::error_code {};
        try {
            uiCall([&] {
                if constexpr (std::is_void_v<std::invoke_result_t<Fn>>) {
                    fn();
                }
                else {
                    error = fn();
                }
                latch.count_down();
            });
            latch.wait();
        }
        catch (const std::system_error &ex) {
            return ex.code();
        }
        catch (...) {
            return std::make_error_code(std::errc::operation_not_supported);
        }
        return error;
    }

    auto createCapture() -> InputCapture::Ptr override;
    auto createInjector() -> InputInjector::Ptr override;

private:
    // MARK: Platform UI thread

    auto runUiThread(std::stop_token token, std::latch &startupLatch) -> void {
        SPDLOG_INFO("Starting win32 platform thread");

        auto signaled = false;
        auto signalStartup = [&] {
            if (!signaled) {
                signaled = true;
                startupLatch.count_down();
            }
        };
        auto failStartup = [&](std::string message) {
            mStartupError = std::move(message);
            mStartupSucceeded = false;
            signalStartup();
        };

        if (!::IsGUIThread(TRUE)) {
            failStartup("Failed to initialize GUI thread state");
            cleanup();
            SPDLOG_INFO("Stopping win32 platform thread");
            return;
        }

        if (!::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
            && !::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
            SPDLOG_WARN("SetProcessDpiAwarenessContext failed: {}", lastErrorCode().message());
        }

        if (!createMessageWindow()) {
            failStartup("Failed to create Win32 message window");
            cleanup();
            SPDLOG_INFO("Stopping win32 platform thread");
            return;
        }
        if (!enumerateMonitors()) {
            failStartup("Failed to enumerate Win32 monitors");
            cleanup();
            SPDLOG_INFO("Stopping win32 platform thread");
            return;
        }

        mStartupSucceeded = true;
        signalStartup();
        pumpMessages(token);

        // Constructor waiters that lost the race still need a countdown.
        signalStartup();
        cleanup();
        SPDLOG_INFO("Stopping win32 platform thread");
    }

    auto pumpMessages(std::stop_token token) -> void {
        std::stop_callback onStop {token, [threadId = ::GetCurrentThreadId()] {
            ::PostThreadMessageW(threadId, WM_QUIT, 0, 0);
        }};

        MSG msg {};
        while (!token.stop_requested()) {
            const auto status = ::GetMessageW(&msg, nullptr, 0, 0);
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

    auto createMessageWindow() -> bool {
        try {
            static constinit std::once_flag once {};
            std::call_once(once, [] {
                WNDCLASSEXW wc {};
                wc.cbSize = sizeof(wc);
                wc.lpfnWndProc = Win32Platform::messageProc;
                wc.lpszClassName = L"mksync::win32::Win32Platform";
                if (!::RegisterClassExW(&wc)) {
                    throw std::system_error(lastErrorCode(), "Failed to register Win32Platform class");
                }
            });
        }
        catch (const std::exception &ex) {
            SPDLOG_ERROR("{}", ex.what());
            return false;
        }

        // Hidden top-level window (not HWND_MESSAGE) so WM_DISPLAYCHANGE is delivered.
        mMessageWindow = ::CreateWindowExW(
            WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
            L"mksync::win32::Win32Platform",
            L"mksync-win32-platform",
            WS_POPUP,
            0,
            0,
            0,
            0,
            nullptr,
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
        struct EnumState {
            std::vector<Win32Monitor> *monitors = nullptr;
            std::unordered_map<HMONITOR, size_t> *mapping = nullptr;
        };

        std::vector<Win32Monitor> monitors;
        std::unordered_map<HMONITOR, size_t> mapping;
        EnumState state {&monitors, &mapping};

        const auto ok = ::EnumDisplayMonitors(
            nullptr,
            nullptr,
            [](HMONITOR hMonitor, HDC, LPRECT, LPARAM data) -> BOOL {
                auto *state = reinterpret_cast<EnumState *>(data);
                auto monitor = makeMonitor(hMonitor, static_cast<uint32_t>(state->monitors->size()));
                if (!monitor) {
                    return FALSE;
                }
                state->mapping->emplace(hMonitor, state->monitors->size());
                state->monitors->push_back(std::move(*monitor));
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&state)
        );
        if (!ok) {
            SPDLOG_ERROR("EnumDisplayMonitors failed: {}", lastErrorCode().message());
            return false;
        }

        {
            auto lock = std::scoped_lock(mMonitorMutex);
            mMonitors = std::move(monitors);
            mMonitorMapping = std::move(mapping);
        }

        for (const auto &screen : screens()) {
            SPDLOG_INFO(
                "Monitor {} at ({}, {}) size {}x{} dpi={} primary={}",
                screen.name,
                screen.x,
                screen.y,
                screen.width,
                screen.height,
                screen.dpi,
                screen.primary
            );
        }
        return true;
    }

    static auto CALLBACK messageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT;

    // MARK: Platform members

    std::jthread mThread;                 // Dedicated UI / hook thread
    HWND mMessageWindow = nullptr;        // Receives WM_PLATFORM_CALL + display broadcasts

    bool mStartupSucceeded = false;
    std::string mStartupError;

    std::weak_ptr<Win32InputCapture> mInputCapture;   // At most one live capture
    std::weak_ptr<Win32InputInjector> mInputInjector; // At most one live injector

    mutable std::mutex mMonitorMutex;
    std::vector<Win32Monitor> mMonitors;
    std::unordered_map<HMONITOR, size_t> mMonitorMapping; // HMONITOR -> index in mMonitors
};

// MARK: Win32InputCapture

// Installs WH_MOUSE_LL / WH_KEYBOARD_LL on the platform UI thread and pushes
// translated InputEvents into an mpsc channel for Server::nextEvent consumers.
class Win32InputCapture final : public InputCapture {
public:
    explicit Win32InputCapture(std::shared_ptr<Win32Platform> platform)
        : mPlatform(std::move(platform)) {}

    // MARK: Capture public API

    auto initialize() -> IoTask<void> override {
        auto [sender, receiver] = ilias::mpsc::channel<InputEvent>(512);
        auto latch = ilias::Latch {1};
        auto error = std::error_code {};

        mDroppedEvents.store(0, std::memory_order_relaxed);
        mSender = std::move(sender);
        mReceiver = std::move(receiver);

        mPlatform->uiCall([this, &latch, &error] {
            if (!installHooks()) {
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
        mPlatform->uiCall([this, &latch] {
            removeHooks();
            latch.countDown();
        });
        co_await ilias::unstoppable(latch.wait());
        co_return;
    }

    auto nextEvent() -> Task<InputEvent> override {
        if (!mReceiver) {
            throw std::runtime_error("InputCapture::nextEvent called after shutdown");
        }

        if (auto dropped = mDroppedEvents.exchange(0, std::memory_order_relaxed); dropped != 0) {
            SPDLOG_WARN("Dropped {} input events because the capture queue was full", dropped);
        }

        if (auto res = co_await mReceiver.recv(); res) {
            co_return std::move(*res);
        }
        throw std::runtime_error("InputCapture::nextEvent called after shutdown");
    }

    // When the active screen is remote, block local delivery and switch mouse
    // sampling to anchor-relative deltas so motion continues past physical edges.
    auto setRemoteControlActive(bool active) -> IoResult<void> override {
        auto error = mPlatform->syncUiCall([this, active]() -> std::error_code {
            if (active == mRemoteControlActive.load(std::memory_order_relaxed)) {
                return {};
            }
            return active ? beginRemoteControl() : (endRemoteControl(), std::error_code {});
        });
        if (error) {
            return Err(error);
        }
        return {};
    }

    auto moveLocalCursor(uint32_t screenIndex, int32_t x, int32_t y) -> IoResult<void> override {
        auto error = mPlatform->syncUiCall([this, screenIndex, x, y]() -> std::error_code {
            auto point = mPlatform->localToGlobal(screenIndex, POINT {.x = x, .y = y});
            if (!point) {
                return std::make_error_code(std::errc::invalid_argument);
            }
            if (!::SetCursorPos(point->x, point->y)) {
                return lastErrorCode();
            }
            mLastMouseGlobal = *point;
            mHasLastMouseGlobal = true;
            return {};
        });
        if (error) {
            return Err(error);
        }
        SPDLOG_TRACE("Win32 capture moved local cursor screen={} local=({}, {})", screenIndex, x, y);
        return {};
    }

private:
    // MARK: Hooks lifecycle

    auto installHooks() -> bool {
        gInputCapture = this;
        gMouseHook = ::SetWindowsHookExW(
            WH_MOUSE_LL,
            mouseHookProc,
            ::GetModuleHandleW(nullptr),
            0
        );
        if (!gMouseHook) {
            removeHooks();
            return false;
        }

        gKeyboardHook = ::SetWindowsHookExW(
            WH_KEYBOARD_LL,
            keyboardHookProc,
            ::GetModuleHandleW(nullptr),
            0
        );
        if (!gKeyboardHook) {
            removeHooks();
            return false;
        }

        SPDLOG_INFO("InputCapture hooked");
        return true;
    }

    auto removeHooks() -> void {
        endRemoteControl();
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

    // MARK: Remote control mode

    auto beginRemoteControl() -> std::error_code {
        POINT cursor {};
        if (!::GetCursorPos(&cursor)) {
            return lastErrorCode();
        }

        // Pin the OS cursor at the exit pixel. User motion becomes deltas from
        // this anchor; the hook warps back after each move so physical edges do
        // not freeze the remote virtual cursor.
        mRemoteAnchor = cursor;
        mHasRemoteAnchor = true;
        mLastMouseGlobal = cursor;
        mHasLastMouseGlobal = true;

        if (!::SetCursorPos(mRemoteAnchor.x, mRemoteAnchor.y)) {
            SPDLOG_WARN("Failed to pin cursor for remote control: {}", lastErrorCode().message());
        }

        // ShowCursor is reference-counted; drive the display count negative.
        if (!mCursorHidden) {
            while (::ShowCursor(FALSE) >= 0) {
            }
            mCursorHidden = true;
        }

        mRemoteControlActive.store(true, std::memory_order_release);
        SPDLOG_INFO(
            "Win32 remote control capture enabled at ({}, {})",
            mRemoteAnchor.x,
            mRemoteAnchor.y
        );
        return {};
    }

    auto endRemoteControl() -> void {
        const auto wasActive = mRemoteControlActive.exchange(false, std::memory_order_acq_rel);
        if (!wasActive && !mCursorHidden && !mHasRemoteAnchor) {
            return;
        }

        mHasRemoteAnchor = false;
        if (mCursorHidden) {
            while (::ShowCursor(TRUE) < 0) {
            }
            mCursorHidden = false;
        }
        SPDLOG_INFO("Win32 remote control capture disabled");
    }

    auto tryEnqueue(InputEvent event) -> void {
        if (!mSender) {
            return;
        }
        SPDLOG_TRACE("Win32 capture event {}", event);
        if (!mSender.trySend(std::move(event))) {
            mDroppedEvents.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // MARK: Low-level hook callbacks

    static auto CALLBACK mouseHookProc(int code, WPARAM wp, LPARAM lp) -> LRESULT {
        if (code < 0 || !gInputCapture) {
            return ::CallNextHookEx(nullptr, code, wp, lp);
        }

        auto *info = reinterpret_cast<MSLLHOOKSTRUCT *>(lp);
        const auto remote = gInputCapture->mRemoteControlActive.load(std::memory_order_acquire);
        const auto injected = (info->flags & LLMHF_INJECTED) != 0;

        // Anchor re-centering uses SetCursorPos; ignore those synthetic moves.
        if (remote && injected) {
            return 1;
        }

        if (auto event = translateMouseEvent(wp, info); event) {
            gInputCapture->tryEnqueue(std::move(*event));
        }

        if (!remote) {
            return ::CallNextHookEx(nullptr, code, wp, lp);
        }

        if (wp == WM_MOUSEMOVE && gInputCapture->mHasRemoteAnchor) {
            const auto &anchor = gInputCapture->mRemoteAnchor;
            if (info->pt.x != anchor.x || info->pt.y != anchor.y) {
                ::SetCursorPos(anchor.x, anchor.y);
            }
        }
        // Swallow so host apps do not receive events meant for the remote peer.
        return 1;
    }

    static auto CALLBACK keyboardHookProc(int code, WPARAM wp, LPARAM lp) -> LRESULT {
        if (code < 0 || !gInputCapture) {
            return ::CallNextHookEx(nullptr, code, wp, lp);
        }

        auto *info = reinterpret_cast<KBDLLHOOKSTRUCT *>(lp);
        const auto remote = gInputCapture->mRemoteControlActive.load(std::memory_order_acquire);
        if (remote && (info->flags & LLKHF_INJECTED) != 0) {
            return 1;
        }

        if (auto event = translateKeyboardEvent(wp, info); event) {
            gInputCapture->tryEnqueue(std::move(*event));
        }

        return remote ? 1 : ::CallNextHookEx(nullptr, code, wp, lp);
    }

    // MARK: Event translation

    static auto translateMouseEvent(WPARAM wp, const MSLLHOOKSTRUCT *info) -> std::optional<InputEvent> {
        auto [screenIndex, local] = gInputCapture->mPlatform->globalToMonitor(info->pt);
        const auto remote = gInputCapture->mRemoteControlActive.load(std::memory_order_acquire);

        switch (wp) {
            case WM_MOUSEMOVE: {
                auto deltaX = 0;
                auto deltaY = 0;
                if (remote && gInputCapture->mHasRemoteAnchor) {
                    deltaX = info->pt.x - gInputCapture->mRemoteAnchor.x;
                    deltaY = info->pt.y - gInputCapture->mRemoteAnchor.y;
                    if (deltaX == 0 && deltaY == 0) {
                        return std::nullopt;
                    }
                }
                else if (gInputCapture->mHasLastMouseGlobal) {
                    deltaX = info->pt.x - gInputCapture->mLastMouseGlobal.x;
                    deltaY = info->pt.y - gInputCapture->mLastMouseGlobal.y;
                }

                gInputCapture->mLastMouseGlobal = info->pt;
                gInputCapture->mHasLastMouseGlobal = true;

                return InputEvent {MouseMoveEvent {
                    .x = local.x,
                    .y = local.y,
                    .screenIndex = screenIndex,
                    .deltaX = deltaX,
                    .deltaY = deltaY,
                }};
            }

            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_XBUTTONUP:
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN: {
                const auto button = mouseButtonFromMessage(wp);
                if (button == MouseButton::None) {
                    return std::nullopt;
                }
                return InputEvent {MouseButtonEvent {
                    .x = local.x,
                    .y = local.y,
                    .screenIndex = screenIndex,
                    .button = button,
                    .release = isMouseButtonRelease(wp),
                }};
            }

            case WM_MOUSEWHEEL:
                return InputEvent {MouseWheelEvent {
                    .x = local.x,
                    .y = local.y,
                    .deltaX = 0,
                    .deltaY = static_cast<SHORT>(HIWORD(info->mouseData)),
                }};

            case WM_MOUSEHWHEEL:
                return InputEvent {MouseWheelEvent {
                    .x = local.x,
                    .y = local.y,
                    .deltaX = static_cast<SHORT>(HIWORD(info->mouseData)),
                    .deltaY = 0,
                }};

            default:
                return std::nullopt;
        }
    }

    static auto translateKeyboardEvent(WPARAM wp, const KBDLLHOOKSTRUCT *info)
        -> std::optional<InputEvent> {
        const auto isPress = wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN;
        const auto isRelease = wp == WM_KEYUP || wp == WM_SYSKEYUP;
        if (!isPress && !isRelease) {
            return std::nullopt;
        }

        const auto vkCode = normalizeVirtualKey(info->vkCode, info->scanCode);
        const auto key = virtualKeyToKey(vkCode);
        auto modifiers = currentModifiers();

        // GetAsyncKeyState may lag the hook event; force the event's own modifier bit.
        if (auto modifier = keyModifierFor(key); modifier != KeyModifier::None) {
            if (isPress) {
                modifiers |= modifier;
            }
            else {
                modifiers &= ~modifier;
            }
        }

        return InputEvent {KeyEvent {
            .key = key,
            .modifiers = modifiers,
            .nativeCode = vkCode,
            .repeat = false,
            .release = isRelease,
        }};
    }

    // MARK: Capture members

    ilias::mpsc::Sender<InputEvent> mSender;
    ilias::mpsc::Receiver<InputEvent> mReceiver;
    std::shared_ptr<Win32Platform> mPlatform;
    std::atomic<uint64_t> mDroppedEvents {0};

    // Shared with hook procs (same UI thread) and setRemoteControlActive callers.
    std::atomic_bool mRemoteControlActive {false};
    bool mCursorHidden = false;
    bool mHasRemoteAnchor = false;      // Valid while remote control pins the cursor
    bool mHasLastMouseGlobal = false;   // Baseline for local absolute->delta sampling
    POINT mRemoteAnchor {};             // Desktop pixel the cursor is pinned to
    POINT mLastMouseGlobal {};          // Previous absolute sample for delta fallback

    friend class Win32Platform;
};

// MARK: Win32InputInjector

// Injects InputEvents into the local OS via SetCursorPos / SendInput.
// Unlike capture, injection does not need the UI thread.
class Win32InputInjector final : public InputInjector {
public:
    explicit Win32InputInjector(std::shared_ptr<Win32Platform> platform)
        : mPlatform(std::move(platform)) {}

    auto initialize() -> IoTask<void> override {
        mInitialized.store(true, std::memory_order_release);
        co_return {};
    }

    auto shutdown() -> Task<void> override {
        mInitialized.store(false, std::memory_order_release);
        co_return;
    }

    auto inject(const InputEvent &event) -> IoTask<void> override {
        if (!mInitialized.load(std::memory_order_acquire)) {
            co_return Err(std::make_error_code(std::errc::not_connected));
        }

        SPDLOG_TRACE("Win32 injecting event {}", event);
        auto error = std::error_code {};
        std::visit([&](const auto &value) {
            error = injectOne(value);
        }, event);

        if (error) {
            SPDLOG_WARN("Win32 failed to inject event {}: {}", event, error.message());
            co_return Err(error);
        }
        SPDLOG_TRACE("Win32 injected event {}", event);
        co_return {};
    }

private:
    // MARK: Inject helpers

    auto moveCursor(uint32_t screenIndex, int32_t x, int32_t y) -> std::error_code {
        auto point = mPlatform->localToGlobal(screenIndex, POINT {.x = x, .y = y});
        if (!point) {
            return std::make_error_code(std::errc::invalid_argument);
        }
        if (!::SetCursorPos(point->x, point->y)) {
            return fallbackError();
        }
        SPDLOG_TRACE(
            "Win32 moved cursor screen={} local=({}, {}) global=({}, {})",
            screenIndex,
            x,
            y,
            point->x,
            point->y
        );
        return {};
    }

    auto injectOne(const MouseMoveEvent &event) -> std::error_code {
        return moveCursor(event.screenIndex, event.x, event.y);
    }

    auto injectOne(const MouseButtonEvent &event) -> std::error_code {
        if (auto error = moveCursor(event.screenIndex, event.x, event.y); error) {
            return error;
        }

        INPUT input {};
        input.type = INPUT_MOUSE;
        switch (event.button) {
            case MouseButton::Left:
                input.mi.dwFlags = event.release ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN;
                break;
            case MouseButton::Right:
                input.mi.dwFlags = event.release ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN;
                break;
            case MouseButton::Middle:
                input.mi.dwFlags = event.release ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN;
                break;
            case MouseButton::None:
                return std::make_error_code(std::errc::invalid_argument);
        }

        auto inputs = std::array {input};
        return sendInputs(inputs);
    }

    auto injectOne(const MouseWheelEvent &event) -> std::error_code {
        std::array<INPUT, 2> inputs {};
        auto count = size_t {0};

        if (event.deltaY != 0) {
            auto &input = inputs[count++];
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_WHEEL;
            input.mi.mouseData = static_cast<DWORD>(event.deltaY);
        }
        if (event.deltaX != 0) {
            auto &input = inputs[count++];
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_HWHEEL;
            input.mi.mouseData = static_cast<DWORD>(event.deltaX);
        }

        return sendInputs(std::span {inputs.data(), count});
    }

    auto injectOne(const KeyEvent &event) -> std::error_code {
        auto key = keyToWin32(event.key, event.nativeCode);
        if (!key) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        INPUT input {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = key->virtualKey;
        input.ki.dwFlags = event.release ? KEYEVENTF_KEYUP : 0;
        if (key->extended) {
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }

        auto inputs = std::array {input};
        return sendInputs(inputs);
    }

    std::shared_ptr<Win32Platform> mPlatform;
    std::atomic_bool mInitialized {false};
};

// MARK: Platform wiring

auto CALLBACK Win32Platform::messageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
    auto *self = reinterpret_cast<Win32Platform *>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) {
        return ::DefWindowProcW(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_DISPLAYCHANGE:
            SPDLOG_INFO("Display change detected");
            if (!self->enumerateMonitors()) {
                SPDLOG_WARN("Failed to re-enumerate monitors after display change");
            }
            return 0;

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

auto Win32Platform::createCapture() -> InputCapture::Ptr {
    if (!mInputCapture.expired()) {
        throw std::runtime_error("InputCapture already created");
    }
    auto capture = std::make_shared<Win32InputCapture>(shared_from_this());
    mInputCapture = capture;
    return capture;
}

auto Win32Platform::createInjector() -> InputInjector::Ptr {
    if (!mInputInjector.expired()) {
        throw std::runtime_error("InputInjector already created");
    }
    auto injector = std::make_shared<Win32InputInjector>(shared_from_this());
    mInputInjector = injector;
    return injector;
}

auto Platform::create() -> Platform::Ptr {
    try {
        return std::make_shared<Win32Platform>();
    }
    catch (const std::exception &ex) {
        SPDLOG_ERROR("Failed to create Win32 platform: {}", ex.what());
        return nullptr;
    }
}

namespace {
    auto createRegisteredWin32Backend() -> Platform::Ptr {
        return Platform::create();
    }

    auto checkWin32Backend() -> Task<BackendCheck> {
        co_return co_await probeBackend("Win32", createRegisteredWin32Backend);
    }

    const BackendRegistration kWin32BackendRegistration {BackendDescriptor {
        .name = "win32",
        .displayName = "Win32",
        .order = 100,
        .check = checkWin32Backend,
        .create = createRegisteredWin32Backend,
    }};
}

MKS_END
#endif
