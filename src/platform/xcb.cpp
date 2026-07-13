#if defined(__linux__)

#include "preinclude.hpp"

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/XF86keysym.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XTest.h>
#include <X11/keysym.h>
#include <poll.h>
#include <xcb/xcb.h>

#ifdef None
    #undef None
#endif

#include <cerrno>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <ilias/net/poller.hpp>
#include <ilias/task.hpp>
#include <spdlog/spdlog.h>

#include "backend.hpp"
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

    auto envValue(const char *name) -> std::string_view {
        const auto *value = std::getenv(name);
        if (!value || *value == '\0') {
            return "<unset>";
        }
        return value;
    }

    auto envIsSet(const char *name) -> bool {
        const auto *value = std::getenv(name);
        return value && *value != '\0';
    }

    auto isWaylandSession() -> bool {
        const auto *sessionType = std::getenv("XDG_SESSION_TYPE");
        return sessionType && std::string_view {sessionType} == "wayland";
    }
}

struct XcbScreen {
    uint32_t index = 0;
    int xScreenNumber = 0;
    Window root = 0;
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
        logSessionEnvironment();

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

        // XQueryPointer gives root/global coordinates. The rest of mksync uses
        // screen-local pixels plus screenIndex, so choose the containing screen
        // or the nearest one if the pointer is just outside every rect.
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

    auto root(uint32_t screenIndex) const -> std::optional<Window> {
        auto lock = std::scoped_lock(mScreenMutex);
        if (screenIndex >= mScreens.size()) {
            return std::nullopt;
        }
        return mScreens[screenIndex].root;
    }

    auto xScreenNumber(uint32_t screenIndex) const -> std::optional<int> {
        auto lock = std::scoped_lock(mScreenMutex);
        if (screenIndex >= mScreens.size()) {
            return std::nullopt;
        }
        return mScreens[screenIndex].xScreenNumber;
    }

    auto localToGlobal(uint32_t screenIndex, int32_t x, int32_t y) const -> std::optional<std::pair<int32_t, int32_t>> {
        auto lock = std::scoped_lock(mScreenMutex);
        if (screenIndex >= mScreens.size()) {
            return std::nullopt;
        }

        // Injection APIs take root/global coordinates, while RPC events carry
        // target screen-local coordinates.
        const auto &info = mScreens[screenIndex].screen;
        return std::pair {
            info.x + x,
            info.y + y,
        };
    }

    auto xiOpcode() const -> int {
        return mXiOpcode;
    }

    auto rawEventsDuringGrab() const -> bool {
        return mRawEventsDuringGrab;
    }

    auto createCapture() -> InputCapture::Ptr override;
    auto createInjector() -> InputInjector::Ptr override;
private:
    auto logSessionEnvironment() const -> void {
        SPDLOG_INFO(
            "X11 session display={} DISPLAY={} XDG_SESSION_TYPE={} WAYLAND_DISPLAY={} "
            "SSH_CONNECTION={} SSH_CLIENT={} XAUTHORITY={}",
            mDisplayName,
            envValue("DISPLAY"),
            envValue("XDG_SESSION_TYPE"),
            envValue("WAYLAND_DISPLAY"),
            envValue("SSH_CONNECTION"),
            envValue("SSH_CLIENT"),
            envValue("XAUTHORITY")
        );

        if (isWaylandSession() || envIsSet("WAYLAND_DISPLAY")) {
            SPDLOG_WARN(
                "Wayland session detected; XTest injection can be ignored by native Wayland "
                "windows. Use an Xorg session or a uinput backend for system-wide input."
            );
        }
        if (envIsSet("SSH_CONNECTION") || envIsSet("SSH_CLIENT")) {
            SPDLOG_WARN(
                "SSH session detected; verify DISPLAY/XAUTHORITY target the graphical desktop "
                "session, not an SSH-forwarded or headless X server."
            );
        }
    }

    auto queryXInput() -> bool {
        int event = 0;
        int error = 0;
        if (::XQueryExtension(mDisplay, "XInputExtension", &mXiOpcode, &event, &error) == 0) {
            SPDLOG_ERROR("XInputExtension is not available");
            return false;
        }

        auto major = 2;
        auto minor = 1;
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
        mRawEventsDuringGrab = major > 2 || (major == 2 && minor >= 1);
        if (!mRawEventsDuringGrab) {
            SPDLOG_WARN("XInput 2.1 raw-event grab semantics are unavailable");
        }
        return true;
    }

    auto enumerateRandrMonitors() -> std::vector<XcbScreen> {
        auto eventBase = 0;
        auto errorBase = 0;
        if (::XRRQueryExtension(mDisplay, &eventBase, &errorBase) == 0) {
            SPDLOG_INFO("XRandR extension is not available; falling back to X root screens");
            return {};
        }

        auto major = 1;
        auto minor = 5;
        if (::XRRQueryVersion(mDisplay, &major, &minor) == 0) {
            SPDLOG_WARN("XRRQueryVersion failed; falling back to X root screens");
            return {};
        }

        SPDLOG_INFO("Using XRandR {}.{} for monitor layout", major, minor);
        if (major < 1 || (major == 1 && minor < 5)) {
            SPDLOG_WARN("XRandR monitor API requires 1.5; falling back to X root screens");
            return {};
        }

        auto monitorCount = 0;
        auto *rawMonitors = ::XRRGetMonitors(mDisplay, mRoot, True, &monitorCount);
        auto monitors = std::unique_ptr<XRRMonitorInfo, decltype(&XRRFreeMonitors)> {
            rawMonitors,
            &XRRFreeMonitors
        };
        if (!rawMonitors || monitorCount <= 0) {
            SPDLOG_WARN("XRRGetMonitors returned no active monitors; falling back to X root screens");
            return {};
        }

        std::vector<XcbScreen> screens;
        screens.reserve(static_cast<size_t>(monitorCount));
        for (auto monitorIndex = 0; monitorIndex < monitorCount; ++monitorIndex) {
            const auto &monitor = rawMonitors[monitorIndex];
            if (monitor.width <= 0 || monitor.height <= 0) {
                SPDLOG_WARN(
                    "Ignoring invalid XRandR monitor index={} rect=({}, {}) {}x{}",
                    monitorIndex,
                    monitor.x,
                    monitor.y,
                    monitor.width,
                    monitor.height
                );
                continue;
            }

            const auto widthMm = static_cast<int32_t>(monitor.mwidth);
            const auto dpi = widthMm > 0
                ? static_cast<int32_t>(std::lround(monitor.width * 25.4 / widthMm))
                : 72;
            const auto index = static_cast<uint32_t>(screens.size());
            screens.push_back(XcbScreen {
                .index = index,
                .xScreenNumber = mDefaultScreen,
                .root = mRoot,
                .screen = ScreenInfo {
                    .x = static_cast<int32_t>(monitor.x),
                    .y = static_cast<int32_t>(monitor.y),
                    .width = static_cast<int32_t>(monitor.width),
                    .height = static_cast<int32_t>(monitor.height),
                    .dpi = dpi,
                    .name = monitorName(monitor.name, index),
                    .primary = monitor.primary != 0,
                }
            });
        }

        return screens;
    }

    auto monitorName(Atom atom, uint32_t index) const -> std::string {
        if (atom == 0) {
            return fmtlib::format("monitor-{}", index);
        }

        auto *name = ::XGetAtomName(mDisplay, atom);
        if (!name) {
            return fmtlib::format("monitor-{}", index);
        }
        auto guard = std::unique_ptr<char, decltype(&XFree)> {name, &XFree};
        if (*name == '\0') {
            return fmtlib::format("monitor-{}", index);
        }
        return name;
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

        auto screens = enumerateRandrMonitors();
        auto iter = ::xcb_setup_roots_iterator(::xcb_get_setup(connection));
        for (auto index = 0U; screens.empty() && iter.rem != 0; ::xcb_screen_next(&iter), ++index) {
            const auto *screen = iter.data;
            const auto width = static_cast<int32_t>(screen->width_in_pixels);
            const auto height = static_cast<int32_t>(screen->height_in_pixels);
            const auto widthMm = static_cast<int32_t>(screen->width_in_millimeters);
            const auto dpi = widthMm > 0 ? static_cast<int32_t>(std::lround(width * 25.4 / widthMm)) : 72;

            screens.push_back(XcbScreen {
                .index = index,
                .xScreenNumber = static_cast<int>(index),
                .root = static_cast<Window>(screen->root),
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
    bool mRawEventsDuringGrab = false;
    std::string mDisplayName;

    std::weak_ptr<XcbInputCapture> mInputCapture;
    std::weak_ptr<XcbInputInjector> mInputInjector;
    mutable std::mutex mScreenMutex;
    std::vector<XcbScreen> mScreens;
};

class XcbInputCapture final : public InputCapture {
public:
    explicit XcbInputCapture(std::shared_ptr<XcbPlatform> platform) : mPlatform(std::move(platform)) {}

    ~XcbInputCapture() override {
        closeDisplay();
    }

    auto initialize() -> IoTask<void> override {
        closeDisplay();

        mDisplay = ::XOpenDisplay(mPlatform->displayName().empty() ? nullptr : mPlatform->displayName().c_str());
        if (!mDisplay) {
            co_return Err(makeIoError(std::errc::no_such_device));
        }

        if (!selectRawInput(mDisplay)) {
            auto error = systemError();
            if (!error) {
                error = makeIoError(std::errc::operation_not_supported);
            }
            closeDisplay();
            co_return Err(error);
        }

        // The X display owns the actual socket fd. Poller only borrows it so ilias can wait
        // for readability; shutdown must close the poller before XCloseDisplay closes the fd.
        const auto fd = ::XConnectionNumber(mDisplay);
        ILIAS_CO_TRY(auto poller, co_await ilias::Poller::make(fd, ilias::IoDescriptor::Socket));
        mPoller = std::move(poller);

        SPDLOG_INFO("XInput2 capture started");
        co_return {};
    }

    auto shutdown() -> Task<void> override {
        closeDisplay();
        co_return;
    }

    auto setRemoteControlActive(bool active) -> IoResult<void> override {
        if (!mDisplay) {
            return Err(std::make_error_code(std::errc::not_connected));
        }
        if (active == mRemoteControlActive) {
            return {};
        }
        if (!active) {
            releaseRemoteControl();
            return {};
        }
        return acquireRemoteControl();
    }

    auto moveLocalCursor(uint32_t screenIndex, int32_t x, int32_t y) -> IoResult<void> override {
        if (!mDisplay) {
            return Err(std::make_error_code(std::errc::not_connected));
        }

        auto global = mPlatform->localToGlobal(screenIndex, x, y);
        if (!global) {
            return Err(std::make_error_code(std::errc::invalid_argument));
        }
        auto root = mPlatform->root(screenIndex).value_or(mPlatform->root());

        if (::XWarpPointer(mDisplay, 0, root, 0, 0, 0, 0, global->first, global->second) == 0) {
            return Err(inputError());
        }
        mLastCorePointer = *global;
        ::XFlush(mDisplay);
        SPDLOG_TRACE(
            "XInput2 capture moved local cursor screen={} local=({}, {}) global=({}, {})",
            screenIndex,
            x,
            y,
            global->first,
            global->second
        );
        return {};
    }

    auto nextEvent() -> Task<InputEvent> override {
        if (!mDisplay || !mPoller) {
            throw std::runtime_error("InputCapture::nextEvent called after shutdown");
        }

        while (true) {
            // XPending may drain bytes from the fd into Xlib's internal queue, so always
            // consume queued X events before waiting on the fd again.
            while (::XPending(mDisplay) > 0) {
                XEvent event {};
                ::XNextEvent(mDisplay, &event);
                if (auto translated = translateEvent(mDisplay, &event)) {
                    SPDLOG_TRACE("XInput2 capture event {}", *translated);
                    co_return std::move(*translated);
                }
            }

            auto pollResult = co_await mPoller.poll(POLLIN);
            if (!pollResult) {
                throw std::system_error(pollResult.error(), "XInput2 capture poll failed");
            }

            const auto events = *pollResult;
            if ((events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                throw std::runtime_error("XInput2 capture fd closed or failed");
            }
            if ((events & POLLIN) == 0) {
                continue;
            }
        }
    }
private:
    auto closeDisplay() -> void {
        // Cancel first to wake any coroutine currently suspended in poll(); then unregister
        // the borrowed fd from ilias before XCloseDisplay closes the underlying socket.
        if (mPoller) {
            auto _ = mPoller.cancel();
            mPoller.close();
        }
        if (mDisplay) {
            releaseRemoteControl();
            if (mHiddenCursor != 0) {
                ::XFreeCursor(mDisplay, mHiddenCursor);
                mHiddenCursor = 0;
            }
            ::XCloseDisplay(mDisplay);
            mDisplay = nullptr;
            SPDLOG_INFO("XInput2 capture stopped");
        }
    }

    static auto grabStatusName(int status) -> std::string_view {
        switch (status) {
            case GrabSuccess: return "GrabSuccess";
            case AlreadyGrabbed: return "AlreadyGrabbed";
            case GrabInvalidTime: return "GrabInvalidTime";
            case GrabNotViewable: return "GrabNotViewable";
            case GrabFrozen: return "GrabFrozen";
            default: return "Unknown";
        }
    }

    static auto grabStatusError(int status) -> std::error_code {
        switch (status) {
            case AlreadyGrabbed:
            case GrabFrozen:
                return makeIoError(std::errc::device_or_resource_busy);
            case GrabInvalidTime:
            case GrabNotViewable:
                return makeIoError(std::errc::invalid_argument);
            default:
                return inputError();
        }
    }

    static auto inputError() -> std::error_code {
        return makeIoError(std::errc::io_error);
    }

    auto ensureHiddenCursor() -> IoResult<Cursor> {
        if (mHiddenCursor != 0) {
            return mHiddenCursor;
        }

        const char blank[] = {0};
        auto bitmap = ::XCreateBitmapFromData(mDisplay, mPlatform->root(), blank, 1, 1);
        if (bitmap == 0) {
            return Err(makeIoError(std::errc::not_enough_memory));
        }

        XColor black {};
        auto cursor = ::XCreatePixmapCursor(mDisplay, bitmap, bitmap, &black, &black, 0, 0);
        ::XFreePixmap(mDisplay, bitmap);
        if (cursor == 0) {
            return Err(makeIoError(std::errc::not_enough_memory));
        }

        mHiddenCursor = cursor;
        return mHiddenCursor;
    }

    auto destroyGrabWindow() -> void {
        if (mGrabWindow == 0) {
            return;
        }
        ::XDestroyWindow(mDisplay, mGrabWindow);
        mGrabWindow = 0;
    }

    auto ensureGrabWindow(Cursor cursor) -> IoResult<Window> {
        if (mGrabWindow != 0) {
            return mGrabWindow;
        }

        XWindowAttributes rootAttributes {};
        if (::XGetWindowAttributes(mDisplay, mPlatform->root(), &rootAttributes) == 0) {
            return Err(inputError());
        }

        XSetWindowAttributes attributes {};
        attributes.override_redirect = True;
        attributes.cursor = cursor;
        attributes.event_mask = PointerMotionMask
            | ButtonMotionMask
            | ButtonPressMask
            | ButtonReleaseMask
            | KeyPressMask
            | KeyReleaseMask;

        auto window = ::XCreateWindow(
            mDisplay,
            mPlatform->root(),
            0,
            0,
            static_cast<unsigned int>(rootAttributes.width),
            static_cast<unsigned int>(rootAttributes.height),
            0,
            0,
            InputOnly,
            CopyFromParent,
            CWOverrideRedirect | CWCursor | CWEventMask,
            &attributes
        );
        if (window == 0) {
            return Err(inputError());
        }

        mGrabWindow = window;
        ::XMapRaised(mDisplay, mGrabWindow);
        ::XSync(mDisplay, False);
        return mGrabWindow;
    }

    auto acquireRemoteControl() -> IoResult<void> {
        auto hiddenCursor = ensureHiddenCursor();
        if (!hiddenCursor) {
            return Err(hiddenCursor.error());
        }
        auto grabWindow = ensureGrabWindow(*hiddenCursor);
        if (!grabWindow) {
            return Err(grabWindow.error());
        }

        const auto eventMask = static_cast<unsigned int>(
            PointerMotionMask | ButtonMotionMask | ButtonPressMask | ButtonReleaseMask
        );
        const auto pointerStatus = ::XGrabPointer(
            mDisplay,
            *grabWindow,
            False,
            eventMask,
            GrabModeAsync,
            GrabModeAsync,
            0,
            *hiddenCursor,
            CurrentTime
        );
        if (pointerStatus != GrabSuccess) {
            SPDLOG_WARN("XGrabPointer failed with {}", grabStatusName(pointerStatus));
            destroyGrabWindow();
            return Err(grabStatusError(pointerStatus));
        }
        mPointerGrabbed = true;

        const auto keyboardStatus = ::XGrabKeyboard(
            mDisplay,
            *grabWindow,
            False,
            GrabModeAsync,
            GrabModeAsync,
            CurrentTime
        );
        if (keyboardStatus == GrabSuccess) {
            mKeyboardGrabbed = true;
        }
        else {
            SPDLOG_WARN("XGrabKeyboard failed with {}; local key events may leak", grabStatusName(keyboardStatus));
        }

        mRemoteControlActive = true;
        mLastCorePointer = queryPointer(mDisplay);
        ::XFlush(mDisplay);
        SPDLOG_INFO("X11 remote control grab enabled");
        return {};
    }

    auto releaseRemoteControl() -> void {
        if (!mDisplay) {
            return;
        }
        if (mKeyboardGrabbed) {
            ::XUngrabKeyboard(mDisplay, CurrentTime);
            mKeyboardGrabbed = false;
        }
        if (mPointerGrabbed) {
            ::XUngrabPointer(mDisplay, CurrentTime);
            mPointerGrabbed = false;
        }
        destroyGrabWindow();
        mLastCorePointer.reset();
        if (mRemoteControlActive) {
            mRemoteControlActive = false;
            ::XFlush(mDisplay);
            SPDLOG_INFO("X11 remote control grab disabled");
        }
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

    auto translateEvent(Display *display, XEvent *event) -> std::optional<InputEvent> {
        if (mRemoteControlActive && !mPlatform->rawEventsDuringGrab()) {
            if (auto translated = translateCoreEvent(display, event)) {
                return translated;
            }
        }

        if (event->xcookie.type != GenericEvent || event->xcookie.extension != mPlatform->xiOpcode()) {
            return std::nullopt;
        }
        if (::XGetEventData(display, &event->xcookie) == 0) {
            return std::nullopt;
        }
        // XGetEventData attaches extension-owned memory to the cookie. The cookie must be
        // released on every return path, otherwise raw input events leak steadily.
        auto guard = std::unique_ptr<XGenericEventCookie, EventCookieDeleter> {
            &event->xcookie,
            EventCookieDeleter {display}
        };

        switch (event->xcookie.evtype) {
            case XI_RawMotion:
                return translateMouseMove(display, event->xcookie);
            case XI_RawButtonPress:
            case XI_RawButtonRelease:
                return translateButtonEvent(display, event->xcookie);
            case XI_RawKeyPress:
            case XI_RawKeyRelease:
                return translateKeyEvent(display, event->xcookie);
            default:
                return std::nullopt;
        }
    }

    auto translateCoreEvent(Display *display, XEvent *event) -> std::optional<InputEvent> {
        switch (event->type) {
            case MotionNotify:
                return translateCoreMouseMove(event->xmotion);
            case ButtonPress:
            case ButtonRelease:
                return translateButtonEvent(
                    display,
                    event->xbutton.button,
                    event->type == ButtonRelease,
                    std::pair {
                        static_cast<int32_t>(event->xbutton.x_root),
                        static_cast<int32_t>(event->xbutton.y_root),
                    }
                );
            case KeyPress:
            case KeyRelease:
                return translateKeyEvent(
                    display,
                    static_cast<KeyCode>(event->xkey.keycode),
                    event->type == KeyRelease
                );
            default:
                return std::nullopt;
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

    static auto rawMotionDelta(const XGenericEventCookie &cookie) -> std::pair<int32_t, int32_t> {
        const auto *raw = static_cast<const XIRawEvent *>(cookie.data);
        auto deltaX = 0.0;
        auto deltaY = 0.0;
        auto *value = raw->raw_values;
        for (auto axis = 0; axis < raw->valuators.mask_len * 8; ++axis) {
            if (!XIMaskIsSet(raw->valuators.mask, axis)) {
                continue;
            }
            if (axis == 0) {
                deltaX = *value;
            }
            else if (axis == 1) {
                deltaY = *value;
            }
            ++value;
        }
        return {
            static_cast<int32_t>(std::lround(deltaX)),
            static_cast<int32_t>(std::lround(deltaY)),
        };
    }

    auto screenLocalPoint(
        Display *display,
        std::optional<std::pair<int32_t, int32_t>> rootPoint
    ) const -> std::optional<std::pair<uint32_t, std::pair<int32_t, int32_t>>> {
        auto pointer = rootPoint;
        if (!pointer) {
            pointer = queryPointer(display);
        }
        if (!pointer) {
            return std::nullopt;
        }
        return mPlatform->globalToScreen(pointer->first, pointer->second);
    }

    auto translateCoreMouseMove(const XMotionEvent &event) -> std::optional<InputEvent> {
        auto rootPoint = std::pair {
            static_cast<int32_t>(event.x_root),
            static_cast<int32_t>(event.y_root),
        };
        auto [screenIndex, local] = mPlatform->globalToScreen(rootPoint.first, rootPoint.second);

        auto deltaX = 0;
        auto deltaY = 0;
        if (mLastCorePointer) {
            deltaX = rootPoint.first - mLastCorePointer->first;
            deltaY = rootPoint.second - mLastCorePointer->second;
        }
        mLastCorePointer = rootPoint;

        return InputEvent {
            MouseMoveEvent {
                .x = local.first,
                .y = local.second,
                .screenIndex = screenIndex,
                .deltaX = deltaX,
                .deltaY = deltaY,
            }
        };
    }

    auto translateMouseMove(Display *display, const XGenericEventCookie &cookie) const -> std::optional<InputEvent> {
        // Keep absolute coordinates for edge detection while also preserving
        // raw device deltas so remote screens can keep moving after the local
        // OS cursor reaches a physical screen boundary.
        auto pointer = queryPointer(display);
        if (!pointer) {
            return std::nullopt;
        }
        auto [screenIndex, local] = mPlatform->globalToScreen(pointer->first, pointer->second);
        auto [deltaX, deltaY] = rawMotionDelta(cookie);
        return InputEvent {
            MouseMoveEvent {
                .x = local.first,
                .y = local.second,
                .screenIndex = screenIndex,
                .deltaX = deltaX,
                .deltaY = deltaY,
            }
        };
    }

    auto translateButtonEvent(Display *display, const XGenericEventCookie &cookie) const -> std::optional<InputEvent> {
        const auto *raw = static_cast<const XIRawEvent *>(cookie.data);
        const auto release = cookie.evtype == XI_RawButtonRelease;
        return translateButtonEvent(display, raw->detail, release, std::nullopt);
    }

    auto translateButtonEvent(
        Display *display,
        unsigned int detail,
        bool release,
        std::optional<std::pair<int32_t, int32_t>> rootPoint
    ) const -> std::optional<InputEvent> {
        const auto pointer = screenLocalPoint(display, rootPoint);
        if (!pointer) {
            return std::nullopt;
        }

        const auto &[screenIndex, local] = *pointer;
        switch (detail) {
            case 1:
            case 2:
            case 3: {
                auto button = MouseButton::Left;
                if (detail == 2) {
                    button = MouseButton::Middle;
                }
                else if (detail == 3) {
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
                // X11 reports wheel steps as virtual buttons: 4/5 are vertical, 6/7 are
                // horizontal. Keep the 120-unit wheel delta convention used by Win32.
                return InputEvent {
                    MouseWheelEvent {
                        .x = local.first,
                        .y = local.second,
                        .deltaX = detail == 6 ? -120 : (detail == 7 ? 120 : 0),
                        .deltaY = detail == 4 ? 120 : (detail == 5 ? -120 : 0),
                    }
                };
            default:
                return std::nullopt;
        }
    }

    auto translateKeyEvent(Display *display, const XGenericEventCookie &cookie) const -> std::optional<InputEvent> {
        const auto *raw = static_cast<const XIRawEvent *>(cookie.data);
        return translateKeyEvent(
            display,
            static_cast<KeyCode>(raw->detail),
            cookie.evtype == XI_RawKeyRelease
        );
    }

    auto translateKeyEvent(Display *display, KeyCode keyCode, bool release) const -> std::optional<InputEvent> {
        const auto keySym = ::XkbKeycodeToKeysym(display, keyCode, 0, 0);
        const auto key = translateKeySym(keySym);
        auto modifiers = currentModifiers(display);

        // XQueryKeymap observes the state before/around this raw event depending on server
        // timing. Adjust the modifier represented by the event itself so forwarded key
        // events have a stable post-event modifier view.
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
                .nativeCode = static_cast<uint32_t>(keyCode),
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

    Display *mDisplay = nullptr;
    Cursor mHiddenCursor = 0;
    Window mGrabWindow = 0;
    bool mRemoteControlActive = false;
    bool mPointerGrabbed = false;
    bool mKeyboardGrabbed = false;
    std::optional<std::pair<int32_t, int32_t>> mLastCorePointer;
    ilias::Poller mPoller;
    std::shared_ptr<XcbPlatform> mPlatform;
};

class XcbInputInjector final : public InputInjector {
public:
    explicit XcbInputInjector(std::shared_ptr<XcbPlatform> platform) : mPlatform(std::move(platform)) {}

    ~XcbInputInjector() override {
        closeDisplay();
    }

    auto initialize() -> IoTask<void> override {
        closeDisplay();

        mDisplay = ::XOpenDisplay(mPlatform->displayName().empty() ? nullptr : mPlatform->displayName().c_str());
        if (!mDisplay) {
            co_return Err(makeIoError(std::errc::no_such_device));
        }

        auto eventBase = 0;
        auto errorBase = 0;
        auto major = 0;
        auto minor = 0;
        if (::XTestQueryExtension(mDisplay, &eventBase, &errorBase, &major, &minor) == 0) {
            closeDisplay();
            co_return Err(makeIoError(std::errc::operation_not_supported));
        }

        SPDLOG_INFO(
            "Using XTest {}.{} for input injection display={} eventBase={} errorBase={}",
            major,
            minor,
            mPlatform->displayName(),
            eventBase,
            errorBase
        );
        co_return {};
    }

    auto shutdown() -> Task<void> override {
        closeDisplay();
        co_return;
    }

    auto inject(const InputEvent &event) -> IoTask<void> override {
        if (!mDisplay) {
            co_return Err(std::make_error_code(std::errc::not_connected));
        }

        SPDLOG_TRACE("XTest injecting event {}", event);
        auto error = std::error_code {};
        std::visit([&](const auto &value) {
            error = injectOne(value);
        }, event);

        if (error) {
            SPDLOG_WARN("XTest failed to inject event {}: {}", event, error.message());
            co_return Err(error);
        }
        SPDLOG_TRACE("XTest injected event {}", event);
        co_return {};
    }
private:
    auto closeDisplay() -> void {
        if (!mDisplay) {
            return;
        }
        ::XCloseDisplay(mDisplay);
        mDisplay = nullptr;
    }

    static auto inputError() -> std::error_code {
        return makeIoError(std::errc::io_error);
    }

    static auto buttonFor(MouseButton button) -> std::optional<unsigned int> {
        switch (button) {
            case MouseButton::Left: return 1U;
            case MouseButton::Middle: return 2U;
            case MouseButton::Right: return 3U;
            case MouseButton::None: return std::nullopt;
        }
        return std::nullopt;
    }

    auto flush() -> std::error_code {
        ::XFlush(mDisplay);
        return {};
    }

    auto moveCursor(uint32_t screenIndex, int32_t x, int32_t y) -> std::error_code {
        auto global = mPlatform->localToGlobal(screenIndex, x, y);
        if (!global) {
            return std::make_error_code(std::errc::invalid_argument);
        }
        const auto xScreenNumber = mPlatform->xScreenNumber(screenIndex);
        if (!xScreenNumber) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        // The server already mapped the target screen entry point. Inject a
        // real XTest motion event so the remote desktop updates its cursor.
        if (::XTestFakeMotionEvent(
            mDisplay,
            *xScreenNumber,
            global->first,
            global->second,
            CurrentTime
        ) == 0) {
            return inputError();
        }
        if (auto error = flush(); error) {
            return error;
        }
        const auto observed = queryPointer();
        if (!observed) {
            SPDLOG_WARN(
                "XTest moved cursor request accepted but XQueryPointer failed screen={} "
                "local=({}, {}) global=({}, {})",
                screenIndex,
                x,
                y,
                global->first,
                global->second
            );
        }
        else if (observed->first != global->first || observed->second != global->second) {
            SPDLOG_WARN(
                "XTest moved cursor request accepted but pointer is at global=({}, {}), "
                "expected global=({}, {}) screen={} local=({}, {})",
                observed->first,
                observed->second,
                global->first,
                global->second,
                screenIndex,
                x,
                y
            );
        }
        SPDLOG_TRACE(
            "XTest moved cursor screen={} local=({}, {}) global=({}, {})",
            screenIndex,
            x,
            y,
            global->first,
            global->second
        );
        return {};
    }

    auto queryPointer() const -> std::optional<std::pair<int32_t, int32_t>> {
        Window root = 0;
        Window child = 0;
        int rootX = 0;
        int rootY = 0;
        int windowX = 0;
        int windowY = 0;
        unsigned int mask = 0;
        if (::XQueryPointer(mDisplay, mPlatform->root(), &root, &child, &rootX, &rootY, &windowX, &windowY, &mask) == 0) {
            return std::nullopt;
        }
        return std::pair {static_cast<int32_t>(rootX), static_cast<int32_t>(rootY)};
    }

    auto fakeButton(unsigned int button, bool press) -> std::error_code {
        if (::XTestFakeButtonEvent(mDisplay, button, press ? True : False, CurrentTime) == 0) {
            return inputError();
        }
        return {};
    }

    auto fakeButtonClick(unsigned int button, uint32_t count) -> std::error_code {
        for (auto index = 0U; index < count; ++index) {
            if (auto error = fakeButton(button, true); error) {
                return error;
            }
            if (auto error = fakeButton(button, false); error) {
                return error;
            }
        }
        return flush();
    }

    static auto wheelClickCount(int32_t delta) -> uint32_t {
        const auto magnitude = static_cast<uint32_t>(std::abs(delta));
        return std::max(1U, (magnitude + 119U) / 120U);
    }

    static auto keySymFor(Key key) -> std::optional<KeySym> {
        using enum Key;

        if (key >= A && key <= Z) {
            return static_cast<KeySym>(XK_a + (static_cast<uint32_t>(key) - static_cast<uint32_t>(A)));
        }
        if (key >= Digit1 && key <= Digit9) {
            return static_cast<KeySym>(XK_1 + (static_cast<uint32_t>(key) - static_cast<uint32_t>(Digit1)));
        }
        if (key >= F1 && key <= F12) {
            return static_cast<KeySym>(XK_F1 + (static_cast<uint32_t>(key) - static_cast<uint32_t>(F1)));
        }
        if (key >= F13 && key <= F24) {
            return static_cast<KeySym>(XK_F13 + (static_cast<uint32_t>(key) - static_cast<uint32_t>(F13)));
        }
        if (key >= Keypad1 && key <= Keypad9) {
            return static_cast<KeySym>(XK_KP_1 + (static_cast<uint32_t>(key) - static_cast<uint32_t>(Keypad1)));
        }

        switch (key) {
            case Digit0: return XK_0;
            case Enter: return XK_Return;
            case KeypadEnter: return XK_KP_Enter;
            case Esc: return XK_Escape;
            case Backspace: return XK_BackSpace;
            case Tab: return XK_Tab;
            case Space: return XK_space;
            case Minus: return XK_minus;
            case Equal: return XK_equal;
            case LeftBrace: return XK_bracketleft;
            case RightBrace: return XK_bracketright;
            case Backslash: return XK_backslash;
            case Semicolon: return XK_semicolon;
            case Apostrophe: return XK_apostrophe;
            case Grave: return XK_grave;
            case Comma: return XK_comma;
            case Dot: return XK_period;
            case Slash: return XK_slash;
            case CapsLock: return XK_Caps_Lock;
            case SysRq: return XK_Print;
            case ScrollLock: return XK_Scroll_Lock;
            case Pause: return XK_Pause;
            case Insert: return XK_Insert;
            case Home: return XK_Home;
            case PageUp: return XK_Page_Up;
            case Delete: return XK_Delete;
            case End: return XK_End;
            case PageDown: return XK_Page_Down;
            case Right: return XK_Right;
            case Left: return XK_Left;
            case Down: return XK_Down;
            case Up: return XK_Up;
            case NumLock: return XK_Num_Lock;
            case KeypadSlash: return XK_KP_Divide;
            case KeypadAsterisk: return XK_KP_Multiply;
            case KeypadMinus: return XK_KP_Subtract;
            case KeypadPlus: return XK_KP_Add;
            case Keypad0: return XK_KP_0;
            case KeypadDot: return XK_KP_Decimal;
            case LeftCtrl: return XK_Control_L;
            case RightCtrl: return XK_Control_R;
            case LeftShift: return XK_Shift_L;
            case RightShift: return XK_Shift_R;
            case LeftAlt: return XK_Alt_L;
            case RightAlt: return XK_Alt_R;
            case LeftMeta: return XK_Super_L;
            case RightMeta: return XK_Super_R;
            case VolumeUp:
            case MediaVolumeUp: return XF86XK_AudioRaiseVolume;
            case VolumeDown:
            case MediaVolumeDown: return XF86XK_AudioLowerVolume;
            case Mute:
            case MediaMute: return XF86XK_AudioMute;
            case MediaPlayPause: return XF86XK_AudioPlay;
            case MediaStopCd:
            case MediaStop: return XF86XK_AudioStop;
            case MediaPreviousSong: return XF86XK_AudioPrev;
            case MediaNextSong: return XF86XK_AudioNext;
            default: return std::nullopt;
        }
    }

    auto keyCodeFor(const KeyEvent &event) const -> std::optional<KeyCode> {
        // Prefer the portable Key enum. nativeCode is only a fallback for keys
        // we have not mapped yet, and may not be portable across platforms.
        if (auto keySym = keySymFor(event.key)) {
            const auto keyCode = ::XKeysymToKeycode(mDisplay, *keySym);
            if (keyCode != 0) {
                return keyCode;
            }
        }

        if (event.nativeCode > 0 && event.nativeCode <= std::numeric_limits<KeyCode>::max()) {
            return static_cast<KeyCode>(event.nativeCode);
        }
        return std::nullopt;
    }

    auto injectOne(const MouseMoveEvent &event) -> std::error_code {
        return moveCursor(event.screenIndex, event.x, event.y);
    }

    auto injectOne(const MouseButtonEvent &event) -> std::error_code {
        if (auto error = moveCursor(event.screenIndex, event.x, event.y); error) {
            return error;
        }

        auto button = buttonFor(event.button);
        if (!button) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        if (auto error = fakeButton(*button, !event.release); error) {
            return error;
        }
        return flush();
    }

    auto injectOne(const MouseWheelEvent &event) -> std::error_code {
        if (event.deltaY > 0) {
            if (auto error = fakeButtonClick(4, wheelClickCount(event.deltaY)); error) {
                return error;
            }
        }
        else if (event.deltaY < 0) {
            if (auto error = fakeButtonClick(5, wheelClickCount(event.deltaY)); error) {
                return error;
            }
        }

        if (event.deltaX < 0) {
            if (auto error = fakeButtonClick(6, wheelClickCount(event.deltaX)); error) {
                return error;
            }
        }
        else if (event.deltaX > 0) {
            if (auto error = fakeButtonClick(7, wheelClickCount(event.deltaX)); error) {
                return error;
            }
        }

        return {};
    }

    auto injectOne(const KeyEvent &event) -> std::error_code {
        auto keyCode = keyCodeFor(event);
        if (!keyCode) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        if (::XTestFakeKeyEvent(mDisplay, *keyCode, event.release ? False : True, CurrentTime) == 0) {
            return inputError();
        }
        return flush();
    }

    Display *mDisplay = nullptr;
    std::shared_ptr<XcbPlatform> mPlatform;
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
    if (!mInputInjector.expired()) {
        throw std::runtime_error("InputInjector already created");
    }
    auto injector = std::make_shared<XcbInputInjector>(shared_from_this());
    mInputInjector = injector;
    return injector;
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

namespace {
    auto createX11Backend() -> Platform::Ptr {
        return Platform::create();
    }

    auto checkX11Backend() -> Task<BackendCheck> {
        if (isWaylandSession() || envIsSet("WAYLAND_DISPLAY")) {
            co_return BackendCheck {
                .available = false,
                .detail = "Wayland session detected; XWayland must be a separate backend",
                .screens = {
                    .supported = false,
                    .detail = "X11 output discovery is not used in a Wayland session",
                },
                .capture = {
                    .supported = false,
                    .detail = "XWayland cannot capture native Wayland input",
                },
                .injection = {
                    .supported = false,
                    .detail = "XWayland cannot inject into native Wayland windows",
                },
            };
        }
        co_return co_await probeBackend("X11", createX11Backend);
    }

    const BackendRegistration kX11BackendRegistration {BackendDescriptor {
        .name = "x11",
        .displayName = "X11 (legacy Xlib/XInput2/XTest)",
        .order = 200,
        .check = checkX11Backend,
        .create = createX11Backend,
    }};
}

MKS_END

#endif
