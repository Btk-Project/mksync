#if defined(__linux__)

#include "preinclude.hpp"

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <X11/keysym.h>
#include <poll.h>
#include <xcb/xcb.h>

#ifdef None
    #undef None
#endif

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <latch>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include <ilias/sync.hpp>
#include <ilias/task.hpp>
#include <spdlog/spdlog.h>

#include "platform.hpp"

MKS_BEGIN

class XcbInputCapture;
class XcbPlatform;
class XcbInputInjector;

namespace {
    auto xInitThreads() -> void {
        static std::once_flag once;
        std::call_once(once, []() {
            if (::XInitThreads() == 0) {
                throw std::runtime_error("XInitThreads failed");
            }
        });
    }

    auto systemError() -> std::error_code {
        return {errno, std::generic_category()};
    }

    auto makeIoError(std::errc err) -> std::error_code {
        return std::make_error_code(err);
    }
}

struct XcbScreen {
    uint32_t index = 0;
    xcb_window_t root = XCB_WINDOW_NONE;
    ScreenInfo screen {};
};

class XcbPlatform final : public Platform, public std::enable_shared_from_this<XcbPlatform> {
public:
    XcbPlatform() {
        xInitThreads();
        mDisplay = ::XOpenDisplay(nullptr);
        if (!mDisplay) {
            throw std::runtime_error("Failed to open X display");
        }

        mDisplayName = ::XDisplayString(mDisplay);
        mDefaultScreen = ::XDefaultScreen(mDisplay);
        mRoot = ::XRootWindow(mDisplay, mDefaultScreen);

        if (!queryXInput()) {
            throw std::runtime_error("XInput2 is not available");
        }
        enumerateScreens();
    }

    ~XcbPlatform() {
        if (mDisplay) {
            ::XCloseDisplay(mDisplay);
            mDisplay = nullptr;
        }
    }

    auto screens() const -> std::vector<ScreenInfo> override {
        auto lock = std::scoped_lock(mScreenMutex);
        std::vector<ScreenInfo> result;
        result.reserve(mScreens.size());
        for (const auto &screen : mScreens) {
            result.push_back(screen.screen);
        }
        return result;
    }

    auto globalToScreen(int32_t x, int32_t y) const -> std::pair<uint32_t, std::pair<int32_t, int32_t>> {
        auto lock = std::scoped_lock(mScreenMutex);
        if (mScreens.empty()) {
            return {0, {x, y}};
        }

        const auto *best = &mScreens.front();
        auto bestDistance = std::numeric_limits<int64_t>::max();
        for (const auto &screen : mScreens) {
            const auto &info = screen.screen;
            const auto inside = x >= info.x && y >= info.y
                && x < info.x + info.width
                && y < info.y + info.height;
            if (inside) {
                best = &screen;
                break;
            }

            const auto clampedX = std::clamp(x, info.x, info.x + info.width);
            const auto clampedY = std::clamp(y, info.y, info.y + info.height);
            const auto dx = static_cast<int64_t>(x) - clampedX;
            const auto dy = static_cast<int64_t>(y) - clampedY;
            const auto distance = dx * dx + dy * dy;
            if (distance < bestDistance) {
                bestDistance = distance;
                best = &screen;
            }
        }

        const auto &info = best->screen;
        return {best->index, {x - info.x, y - info.y}};
    }

    auto displayName() const -> const std::string & {
        return mDisplayName;
    }

    auto root() const -> Window {
        return mRoot;
    }

    auto xiOpcode() const -> int {
        return mXiOpcode;
    }

    auto createCapture() -> InputCapture::Ptr override;
    auto createInjector() -> InputInjector::Ptr override;
private:
    auto queryXInput() -> bool {
        int event = 0;
        int error = 0;
        if (::XQueryExtension(mDisplay, "XInputExtension", &mXiOpcode, &event, &error) == 0) {
            SPDLOG_ERROR("XInputExtension is not available");
            return false;
        }

        auto major = 2;
        auto minor = 0;
        const auto status = ::XIQueryVersion(mDisplay, &major, &minor);
        if (status == BadRequest) {
            SPDLOG_ERROR("XInput2 2.0 is not supported by the X server");
            return false;
        }
        if (status != Success) {
            SPDLOG_ERROR("XIQueryVersion failed with status {}", status);
            return false;
        }

        SPDLOG_INFO("Using XInput {}.{}", major, minor);
        return true;
    }

    auto enumerateScreens() -> void {
        int defaultScreen = 0;
        auto *connection = ::xcb_connect(mDisplayName.empty() ? nullptr : mDisplayName.c_str(), &defaultScreen);
        if (!connection) {
            throw std::runtime_error("Failed to create XCB connection");
        }
        auto connectionGuard = std::unique_ptr<xcb_connection_t, decltype(&xcb_disconnect)> {
            connection,
            &xcb_disconnect
        };

        const auto error = ::xcb_connection_has_error(connection);
        if (error != 0) {
            throw std::runtime_error(fmtlib::format("XCB connection failed with error {}", error));
        }

        std::vector<XcbScreen> screens;
        auto iter = ::xcb_setup_roots_iterator(::xcb_get_setup(connection));
        for (auto index = 0U; iter.rem != 0; ::xcb_screen_next(&iter), ++index) {
            const auto *screen = iter.data;
            const auto width = static_cast<int32_t>(screen->width_in_pixels);
            const auto height = static_cast<int32_t>(screen->height_in_pixels);
            const auto widthMm = static_cast<int32_t>(screen->width_in_millimeters);
            const auto dpi = widthMm > 0 ? static_cast<int32_t>(std::lround(width * 25.4 / widthMm)) : 72;

            screens.push_back(XcbScreen {
                .index = index,
                .root = screen->root,
                .screen = ScreenInfo {
                    .x = 0,
                    .y = 0,
                    .width = width,
                    .height = height,
                    .dpi = dpi,
                    .name = fmtlib::format("screen-{}", index),
                    .primary = static_cast<int>(index) == defaultScreen,
                }
            });
        }

        {
            auto lock = std::scoped_lock(mScreenMutex);
            mScreens = std::move(screens);
        }

        for (const auto &screen : this->screens()) {
            SPDLOG_INFO(
                "X screen {} at ({}, {}) size {}x{} dpi={} primary={}",
                screen.name,
                screen.x,
                screen.y,
                screen.width,
                screen.height,
                screen.dpi,
                screen.primary
            );
        }
    }

    Display *mDisplay = nullptr;
    Window mRoot = 0;
    int mDefaultScreen = 0;
    int mXiOpcode = 0;
    std::string mDisplayName;

    std::weak_ptr<XcbInputCapture> mInputCapture;
    mutable std::mutex mScreenMutex;
    std::vector<XcbScreen> mScreens;
};

class XcbInputCapture final : public InputCapture {
public:
    explicit XcbInputCapture(std::shared_ptr<XcbPlatform> platform) : mPlatform(std::move(platform)) {}

    ~XcbInputCapture() override {
        stopThread();
    }

    auto initialize() -> IoTask<void> override {
        auto [sender, receiver] = ilias::mpsc::channel<InputEvent>(512);
        auto latch = ilias::Latch {1};
        auto error = std::error_code {};

        stopThread();
        mDroppedEvents.store(0, std::memory_order_relaxed);
        mSender = std::move(sender);
        mReceiver = std::move(receiver);

        mThread = std::jthread([this, &latch, &error](std::stop_token token) {
            run(token, &latch, &error);
        });

        co_await ilias::unstoppable(latch.wait());
        if (error) {
            stopThread();
            co_return Err(error);
        }
        co_return {};
    }

    auto shutdown() -> Task<void> override {
        stopThread();
        mSender = {};
        mReceiver = {};
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
private:
    auto stopThread() -> void {
        if (mThread.joinable()) {
            mThread.request_stop();
            mThread.join();
        }
    }

    auto run(std::stop_token token, ilias::Latch *latch, std::error_code *error) -> void {
        auto startupSignaled = false;
        auto signalStartup = [&]() {
            if (!startupSignaled) {
                startupSignaled = true;
                latch->countDown();
            }
        };

        auto *display = ::XOpenDisplay(mPlatform->displayName().empty() ? nullptr : mPlatform->displayName().c_str());
        if (!display) {
            *error = makeIoError(std::errc::no_such_device);
            signalStartup();
            return;
        }
        auto displayGuard = std::unique_ptr<Display, decltype(&XCloseDisplay)> {display, &XCloseDisplay};

        if (!selectRawInput(display)) {
            *error = systemError();
            if (!*error) {
                *error = makeIoError(std::errc::operation_not_supported);
            }
            signalStartup();
            return;
        }

        SPDLOG_INFO("XInput2 capture started");
        signalStartup();

        auto fd = ::XConnectionNumber(display);
        while (!token.stop_requested()) {
            pollfd pollInfo {
                .fd = fd,
                .events = POLLIN,
                .revents = 0,
            };

            const auto ready = ::poll(&pollInfo, 1, 100);
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                SPDLOG_ERROR("poll failed while waiting for X events: {}", std::strerror(errno));
                break;
            }
            if (ready == 0) {
                continue;
            }

            while (::XPending(display) > 0) {
                XEvent event {};
                ::XNextEvent(display, &event);
                handleEvent(display, &event);
            }
        }

        SPDLOG_INFO("XInput2 capture stopped");
    }

    auto selectRawInput(Display *display) const -> bool {
        unsigned char mask[XIMaskLen(XI_LASTEVENT)] {};
        XISetMask(mask, XI_RawKeyPress);
        XISetMask(mask, XI_RawKeyRelease);
        XISetMask(mask, XI_RawButtonPress);
        XISetMask(mask, XI_RawButtonRelease);
        XISetMask(mask, XI_RawMotion);

        XIEventMask eventMask {
            .deviceid = XIAllMasterDevices,
            .mask_len = sizeof(mask),
            .mask = mask,
        };

        const auto status = ::XISelectEvents(display, mPlatform->root(), &eventMask, 1);
        ::XFlush(display);
        if (status != Success) {
            SPDLOG_ERROR("XISelectEvents failed with status {}", status);
            return false;
        }
        return true;
    }

    auto handleEvent(Display *display, XEvent *event) -> void {
        if (event->xcookie.type != GenericEvent || event->xcookie.extension != mPlatform->xiOpcode()) {
            return;
        }
        if (::XGetEventData(display, &event->xcookie) == 0) {
            return;
        }
        auto guard = std::unique_ptr<XGenericEventCookie, EventCookieDeleter> {
            &event->xcookie,
            EventCookieDeleter {display}
        };

        switch (event->xcookie.evtype) {
            case XI_RawMotion:
                if (auto translated = translateMouseMove(display); translated) {
                    tryEnqueue(std::move(*translated));
                }
                break;
            case XI_RawButtonPress:
            case XI_RawButtonRelease:
                if (auto translated = translateButtonEvent(display, event->xcookie); translated) {
                    tryEnqueue(std::move(*translated));
                }
                break;
            case XI_RawKeyPress:
            case XI_RawKeyRelease:
                if (auto translated = translateKeyEvent(display, event->xcookie); translated) {
                    tryEnqueue(std::move(*translated));
                }
                break;
            default:
                break;
        }
    }

    auto tryEnqueue(InputEvent event) -> void {
        if (!mSender) {
            return;
        }
        if (auto result = mSender.trySend(std::move(event)); !result) {
            mDroppedEvents.fetch_add(1, std::memory_order_relaxed);
        }
    }

    auto queryPointer(Display *display) const -> std::optional<std::pair<int32_t, int32_t>> {
        Window root = 0;
        Window child = 0;
        int rootX = 0;
        int rootY = 0;
        int windowX = 0;
        int windowY = 0;
        unsigned int mask = 0;
        if (::XQueryPointer(display, mPlatform->root(), &root, &child, &rootX, &rootY, &windowX, &windowY, &mask) == 0) {
            return std::nullopt;
        }
        return std::pair {static_cast<int32_t>(rootX), static_cast<int32_t>(rootY)};
    }

    auto translateMouseMove(Display *display) const -> std::optional<InputEvent> {
        auto pointer = queryPointer(display);
        if (!pointer) {
            return std::nullopt;
        }
        auto [screenIndex, local] = mPlatform->globalToScreen(pointer->first, pointer->second);
        return InputEvent {
            MouseMoveEvent {
                .x = local.first,
                .y = local.second,
                .screenIndex = screenIndex,
            }
        };
    }

    auto translateButtonEvent(Display *display, const XGenericEventCookie &cookie) const -> std::optional<InputEvent> {
        const auto *raw = static_cast<const XIRawEvent *>(cookie.data);
        const auto release = cookie.evtype == XI_RawButtonRelease;
        const auto pointer = queryPointer(display);
        if (!pointer) {
            return std::nullopt;
        }

        auto [screenIndex, local] = mPlatform->globalToScreen(pointer->first, pointer->second);
        switch (raw->detail) {
            case 1:
            case 2:
            case 3: {
                auto button = MouseButton::Left;
                if (raw->detail == 2) {
                    button = MouseButton::Middle;
                }
                else if (raw->detail == 3) {
                    button = MouseButton::Right;
                }
                return InputEvent {
                    MouseButtonEvent {
                        .x = local.first,
                        .y = local.second,
                        .screenIndex = screenIndex,
                        .button = button,
                        .release = release,
                    }
                };
            }
            case 4:
            case 5:
            case 6:
            case 7:
                if (release) {
                    return std::nullopt;
                }
                return InputEvent {
                    MouseWheelEvent {
                        .x = local.first,
                        .y = local.second,
                        .deltaX = raw->detail == 6 ? -120 : (raw->detail == 7 ? 120 : 0),
                        .deltaY = raw->detail == 4 ? 120 : (raw->detail == 5 ? -120 : 0),
                    }
                };
            default:
                return std::nullopt;
        }
    }

    auto translateKeyEvent(Display *display, const XGenericEventCookie &cookie) const -> std::optional<InputEvent> {
        const auto *raw = static_cast<const XIRawEvent *>(cookie.data);
        const auto keySym = ::XkbKeycodeToKeysym(display, static_cast<KeyCode>(raw->detail), 0, 0);
        const auto key = translateKeySym(keySym);
        const auto release = cookie.evtype == XI_RawKeyRelease;
        auto modifiers = currentModifiers(display);

        if (auto modifier = keyModifierFor(key); modifier != KeyModifier::None) {
            if (release) {
                modifiers &= ~modifier;
            }
            else {
                modifiers |= modifier;
            }
        }

        return InputEvent {
            KeyEvent {
                .key = key,
                .modifiers = modifiers,
                .nativeCode = static_cast<uint32_t>(raw->detail),
                .repeat = false,
                .release = release,
            }
        };
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

    static auto currentModifiers(Display *display) -> KeyModifier {
        char keys[32] {};
        ::XQueryKeymap(display, keys);

        auto pressed = [&](KeySym keySym) {
            const auto keyCode = ::XKeysymToKeycode(display, keySym);
            return keyCode != 0 && (keys[keyCode / 8] & (1 << (keyCode % 8))) != 0;
        };

        auto modifiers = KeyModifier::None;
        if (pressed(XK_Control_L)) { modifiers |= KeyModifier::LeftCtrl; }
        if (pressed(XK_Control_R)) { modifiers |= KeyModifier::RightCtrl; }
        if (pressed(XK_Shift_L))   { modifiers |= KeyModifier::LeftShift; }
        if (pressed(XK_Shift_R))   { modifiers |= KeyModifier::RightShift; }
        if (pressed(XK_Alt_L))     { modifiers |= KeyModifier::LeftAlt; }
        if (pressed(XK_Alt_R))     { modifiers |= KeyModifier::RightAlt; }
        if (pressed(XK_Super_L))   { modifiers |= KeyModifier::LeftMeta; }
        if (pressed(XK_Super_R))   { modifiers |= KeyModifier::RightMeta; }
        return modifiers;
    }

    static auto translateKeySym(KeySym keySym) -> Key {
        using enum Key;

        if (keySym >= XK_a && keySym <= XK_z) {
            return static_cast<Key>(static_cast<uint32_t>(A) + (keySym - XK_a));
        }
        if (keySym >= XK_A && keySym <= XK_Z) {
            return static_cast<Key>(static_cast<uint32_t>(A) + (keySym - XK_A));
        }
        if (keySym >= XK_1 && keySym <= XK_9) {
            return static_cast<Key>(static_cast<uint32_t>(Digit1) + (keySym - XK_1));
        }

        switch (keySym) {
            case XK_0: return Digit0;
            case XK_Return: return Enter;
            case XK_KP_Enter: return KeypadEnter;
            case XK_Escape: return Esc;
            case XK_BackSpace: return Backspace;
            case XK_Tab: return Tab;
            case XK_space: return Space;
            case XK_minus: return Minus;
            case XK_equal: return Equal;
            case XK_bracketleft: return LeftBrace;
            case XK_bracketright: return RightBrace;
            case XK_backslash: return Backslash;
            case XK_semicolon: return Semicolon;
            case XK_apostrophe: return Apostrophe;
            case XK_grave: return Grave;
            case XK_comma: return Comma;
            case XK_period: return Dot;
            case XK_slash: return Slash;
            case XK_Caps_Lock: return CapsLock;
            case XK_F1: return F1;
            case XK_F2: return F2;
            case XK_F3: return F3;
            case XK_F4: return F4;
            case XK_F5: return F5;
            case XK_F6: return F6;
            case XK_F7: return F7;
            case XK_F8: return F8;
            case XK_F9: return F9;
            case XK_F10: return F10;
            case XK_F11: return F11;
            case XK_F12: return F12;
            case XK_Print: return SysRq;
            case XK_Scroll_Lock: return ScrollLock;
            case XK_Pause: return Pause;
            case XK_Insert: return Insert;
            case XK_Home: return Home;
            case XK_Page_Up: return PageUp;
            case XK_Delete: return Delete;
            case XK_End: return End;
            case XK_Page_Down: return PageDown;
            case XK_Right: return Right;
            case XK_Left: return Left;
            case XK_Down: return Down;
            case XK_Up: return Up;
            case XK_Num_Lock: return NumLock;
            case XK_KP_Divide: return KeypadSlash;
            case XK_KP_Multiply: return KeypadAsterisk;
            case XK_KP_Subtract: return KeypadMinus;
            case XK_KP_Add: return KeypadPlus;
            case XK_KP_0: return Keypad0;
            case XK_KP_1: return Keypad1;
            case XK_KP_2: return Keypad2;
            case XK_KP_3: return Keypad3;
            case XK_KP_4: return Keypad4;
            case XK_KP_5: return Keypad5;
            case XK_KP_6: return Keypad6;
            case XK_KP_7: return Keypad7;
            case XK_KP_8: return Keypad8;
            case XK_KP_9: return Keypad9;
            case XK_KP_Decimal: return KeypadDot;
            case XK_Control_L: return LeftCtrl;
            case XK_Control_R: return RightCtrl;
            case XK_Shift_L: return LeftShift;
            case XK_Shift_R: return RightShift;
            case XK_Alt_L: return LeftAlt;
            case XK_Alt_R: return RightAlt;
            case XK_Super_L: return LeftMeta;
            case XK_Super_R: return RightMeta;
            default: return None;
        }
    }

    struct EventCookieDeleter {
        Display *display = nullptr;

        auto operator()(XGenericEventCookie *cookie) const -> void {
            ::XFreeEventData(display, cookie);
        }
    };

    ilias::mpsc::Sender<InputEvent> mSender;
    ilias::mpsc::Receiver<InputEvent> mReceiver;
    std::shared_ptr<XcbPlatform> mPlatform;
    std::jthread mThread;
    std::atomic<uint64_t> mDroppedEvents {0};
};

auto XcbPlatform::createCapture() -> InputCapture::Ptr {
    if (!mInputCapture.expired()) {
        throw std::runtime_error("InputCapture already created");
    }
    auto capture = std::make_shared<XcbInputCapture>(shared_from_this());
    mInputCapture = capture;
    return capture;
}

auto XcbPlatform::createInjector() -> InputInjector::Ptr {
    return {};
}

auto Platform::create() -> Platform::Ptr {
    try {
        return std::make_shared<XcbPlatform>();
    }
    catch (const std::exception &ex) {
        SPDLOG_ERROR("Failed to create XCB platform: {}", ex.what());
        return nullptr;
    }
}

MKS_END

#endif
