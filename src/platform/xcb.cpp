#if defined(__linux__)

    #include "preinclude.hpp"

    #include <X11/XF86keysym.h>
    #include <X11/keysym.h>
    #include <poll.h>
    #include <xcb/randr.h>
    #include <xcb/xcb.h>
    #include <xcb/xcb_keysyms.h>
    #include <xcb/xinput.h>
    #include <xcb/xtest.h>

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
    #include "xcb_connection.hpp"

MKS_BEGIN

class XcbInputCapture;
class XcbInputInjector;
class XcbPlatform;

namespace
{

    struct XcbFree {
        template <typename T>
        auto operator()(T *pointer) const -> void
        {
            std::free(pointer);
        }
    };

    template <typename T>
    using XcbPtr = std::unique_ptr<T, XcbFree>;

    struct XcbKeySymbolsFree {
        auto operator()(xcb_key_symbols_t *symbols) const -> void { xcb_key_symbols_free(symbols); }
    };

    using XcbKeySymbolsPtr = std::unique_ptr<xcb_key_symbols_t, XcbKeySymbolsFree>;

    auto makeIoError(std::errc error) -> std::error_code
    {
        return std::make_error_code(error);
    }

    auto envValue(const char *name) -> std::string_view
    {
        const auto *value = std::getenv(name);
        return value && *value != '\0' ? value : "<unset>";
    }

    auto envString(const char *name) -> std::string
    {
        const auto *value = std::getenv(name);
        return value && *value != '\0' ? value : "";
    }

    auto envIsSet(const char *name) -> bool
    {
        const auto *value = std::getenv(name);
        return value && *value != '\0';
    }

    auto isWaylandSession() -> bool
    {
        const auto *sessionType = std::getenv("XDG_SESSION_TYPE");
        return sessionType && std::string_view{sessionType} == "wayland";
    }

    auto protocolError(xcb_generic_error_t *error, std::string_view operation) -> bool
    {
        if (!error) {
            return false;
        }
        SPDLOG_ERROR("{} failed: X11 error={} major={} minor={} resource={}", operation,
                     error->error_code, error->major_code, error->minor_code, error->resource_id);
        std::free(error);
        return true;
    }

    auto coordinateFits(int32_t value) -> bool
    {
        return value >= std::numeric_limits<int16_t>::min() &&
               value <= std::numeric_limits<int16_t>::max();
    }

    auto fixed3232(const xcb_input_fp3232_t &value) -> double
    {
        constexpr auto kFractionScale = 4294967296.0;
        return static_cast<double>(value.integral) +
               static_cast<double>(value.frac) / kFractionScale;
    }

} // namespace

struct XcbScreen {
    uint32_t     index         = 0;
    int          xScreenNumber = 0;
    xcb_window_t root          = XCB_NONE;
    ScreenInfo   screen{};
};

class XcbPlatform final : public Platform, public std::enable_shared_from_this<XcbPlatform> {
public:
    XcbPlatform() : mDisplayName(envString("DISPLAY"))
    {
        auto connection = XcbConnection::connect(mDisplayName);
        if (!connection) {
            throw std::system_error(connection.error(), "Failed to open XCB connection");
        }
        mConnection = std::move(*connection);

        const auto *defaultScreen = mConnection->screen(mConnection->defaultScreen());
        if (!defaultScreen) {
            throw std::runtime_error("XCB display has no default screen");
        }
        mRoot       = defaultScreen->root;
        mRootWidth  = defaultScreen->width_in_pixels;
        mRootHeight = defaultScreen->height_in_pixels;

        logSessionEnvironment();
        if (!queryXInput()) {
            throw std::runtime_error("XInput2 is not available");
        }
        enumerateScreens();
    }

    auto screens() const -> std::vector<ScreenInfo> override
    {
        auto                    lock = std::scoped_lock(mScreenMutex);
        std::vector<ScreenInfo> result;
        result.reserve(mScreens.size());
        for (const auto &screen : mScreens) {
            result.push_back(screen.screen);
        }
        return result;
    }

    auto globalToScreen(int32_t x, int32_t y) const
        -> std::pair<uint32_t, std::pair<int32_t, int32_t>>
    {
        auto lock = std::scoped_lock(mScreenMutex);
        if (mScreens.empty()) {
            return {
                0, {x, y}
            };
        }

        const auto *best         = &mScreens.front();
        auto        bestDistance = std::numeric_limits<int64_t>::max();
        for (const auto &screen : mScreens) {
            const auto &info = screen.screen;
            const auto  inside =
                x >= info.x && y >= info.y && x < info.x + info.width && y < info.y + info.height;
            if (inside) {
                best = &screen;
                break;
            }

            const auto clampedX = std::clamp(x, info.x, info.x + info.width);
            const auto clampedY = std::clamp(y, info.y, info.y + info.height);
            const auto deltaX   = static_cast<int64_t>(x) - clampedX;
            const auto deltaY   = static_cast<int64_t>(y) - clampedY;
            const auto distance = deltaX * deltaX + deltaY * deltaY;
            if (distance < bestDistance) {
                bestDistance = distance;
                best         = &screen;
            }
        }

        const auto &info = best->screen;
        return {
            best->index, {x - info.x, y - info.y}
        };
    }

    auto localToGlobal(uint32_t screenIndex, int32_t x, int32_t y) const
        -> std::optional<std::pair<int32_t, int32_t>>
    {
        auto lock = std::scoped_lock(mScreenMutex);
        if (screenIndex >= mScreens.size()) {
            return std::nullopt;
        }
        const auto &info = mScreens[screenIndex].screen;
        return std::pair{info.x + x, info.y + y};
    }

    auto displayName() const -> const std::string & { return mDisplayName; }

    auto root() const -> xcb_window_t { return mRoot; }

    auto root(uint32_t screenIndex) const -> std::optional<xcb_window_t>
    {
        auto lock = std::scoped_lock(mScreenMutex);
        if (screenIndex >= mScreens.size()) {
            return std::nullopt;
        }
        return mScreens[screenIndex].root;
    }

    auto rootWidth() const -> uint16_t { return mRootWidth; }

    auto rootHeight() const -> uint16_t { return mRootHeight; }

    auto xiOpcode() const -> uint8_t { return mXiOpcode; }

    auto rawEventsDuringGrab() const -> bool { return mRawEventsDuringGrab; }

    auto createCapture() -> InputCapture::Ptr override;
    auto createInjector() -> InputInjector::Ptr override;

private:
    auto logSessionEnvironment() const -> void
    {
        SPDLOG_INFO("X11/XCB session display={} DISPLAY={} XDG_SESSION_TYPE={} WAYLAND_DISPLAY={} "
                    "SSH_CONNECTION={} SSH_CLIENT={} XAUTHORITY={}",
                    mDisplayName.empty() ? "<default>" : mDisplayName, envValue("DISPLAY"),
                    envValue("XDG_SESSION_TYPE"), envValue("WAYLAND_DISPLAY"),
                    envValue("SSH_CONNECTION"), envValue("SSH_CLIENT"), envValue("XAUTHORITY"));

        if (isWaylandSession() || envIsSet("WAYLAND_DISPLAY")) {
            SPDLOG_WARN("Wayland session detected; XTest cannot control native Wayland windows");
        }
        if (envIsSet("SSH_CONNECTION") || envIsSet("SSH_CLIENT")) {
            SPDLOG_WARN("SSH session detected; verify DISPLAY and XAUTHORITY target the desktop");
        }
    }

    auto queryXInput() -> bool
    {
        const auto *extension = xcb_get_extension_data(mConnection->get(), &xcb_input_id);
        if (!extension || extension->present == 0) {
            SPDLOG_ERROR("XInputExtension is not available");
            return false;
        }
        mXiOpcode = extension->major_opcode;

        xcb_generic_error_t                       *error = nullptr;
        XcbPtr<xcb_input_xi_query_version_reply_t> reply{xcb_input_xi_query_version_reply(
            mConnection->get(), xcb_input_xi_query_version(mConnection->get(), 2, 1), &error)};
        if (protocolError(error, "XIQueryVersion") || !reply) {
            return false;
        }

        SPDLOG_INFO("Using XInput {}.{} through XCB", reply->major_version, reply->minor_version);
        mRawEventsDuringGrab =
            reply->major_version > 2 || (reply->major_version == 2 && reply->minor_version >= 1);
        if (!mRawEventsDuringGrab) {
            SPDLOG_WARN("XInput 2.1 raw-event grab semantics are unavailable");
        }
        return true;
    }

    auto monitorName(xcb_atom_t atom, uint32_t index) const -> std::string
    {
        if (atom == XCB_NONE) {
            return fmtlib::format("monitor-{}", index);
        }

        xcb_generic_error_t              *error = nullptr;
        XcbPtr<xcb_get_atom_name_reply_t> reply{xcb_get_atom_name_reply(
            mConnection->get(), xcb_get_atom_name(mConnection->get(), atom), &error)};
        if (protocolError(error, "GetAtomName") || !reply) {
            return fmtlib::format("monitor-{}", index);
        }
        const auto length = xcb_get_atom_name_name_length(reply.get());
        if (length <= 0) {
            return fmtlib::format("monitor-{}", index);
        }
        return {xcb_get_atom_name_name(reply.get()), static_cast<size_t>(length)};
    }

    auto enumerateRandrMonitors() -> std::vector<XcbScreen>
    {
        const auto *extension = xcb_get_extension_data(mConnection->get(), &xcb_randr_id);
        if (!extension || extension->present == 0) {
            SPDLOG_INFO("RandR is unavailable; falling back to X root screens");
            return {};
        }

        xcb_generic_error_t                    *error = nullptr;
        XcbPtr<xcb_randr_query_version_reply_t> version{xcb_randr_query_version_reply(
            mConnection->get(), xcb_randr_query_version(mConnection->get(), 1, 5), &error)};
        if (protocolError(error, "RandR QueryVersion") || !version) {
            return {};
        }
        if (version->major_version < 1 ||
            (version->major_version == 1 && version->minor_version < 5)) {
            SPDLOG_WARN("RandR monitor API requires 1.5; server provides {}.{}",
                        version->major_version, version->minor_version);
            return {};
        }

        error = nullptr;
        XcbPtr<xcb_randr_get_monitors_reply_t> reply{xcb_randr_get_monitors_reply(
            mConnection->get(), xcb_randr_get_monitors(mConnection->get(), mRoot, 1), &error)};
        if (protocolError(error, "RandR GetMonitors") || !reply || reply->nMonitors == 0) {
            return {};
        }

        SPDLOG_INFO("Using RandR {}.{} for monitor layout", version->major_version,
                    version->minor_version);
        std::vector<XcbScreen> screens;
        screens.reserve(reply->nMonitors);
        auto iterator = xcb_randr_get_monitors_monitors_iterator(reply.get());
        while (iterator.rem != 0) {
            const auto &monitor = *iterator.data;
            if (monitor.width != 0 && monitor.height != 0) {
                const auto widthMm = static_cast<int32_t>(monitor.width_in_millimeters);
                const auto dpi =
                    widthMm > 0 ? static_cast<int32_t>(std::lround(monitor.width * 25.4 / widthMm))
                                : 72;
                const auto index = static_cast<uint32_t>(screens.size());
                screens.push_back(XcbScreen{
                    .index         = index,
                    .xScreenNumber = mConnection->defaultScreen(),
                    .root          = mRoot,
                    .screen =
                        ScreenInfo{
                                   .x       = monitor.x,
                                   .y       = monitor.y,
                                   .width   = monitor.width,
                                   .height  = monitor.height,
                                   .dpi     = dpi,
                                   .name    = monitorName(monitor.name, index),
                                   .primary = monitor.primary != 0,
                                   },
                });
            }
            xcb_randr_monitor_info_next(&iterator);
        }
        return screens;
    }

    auto enumerateScreens() -> void
    {
        auto screens  = enumerateRandrMonitors();
        auto iterator = xcb_setup_roots_iterator(xcb_get_setup(mConnection->get()));
        for (auto index = 0U; screens.empty() && iterator.rem != 0;
             xcb_screen_next(&iterator), ++index) {
            const auto *screen  = iterator.data;
            const auto  width   = static_cast<int32_t>(screen->width_in_pixels);
            const auto  height  = static_cast<int32_t>(screen->height_in_pixels);
            const auto  widthMm = static_cast<int32_t>(screen->width_in_millimeters);
            const auto  dpi =
                widthMm > 0 ? static_cast<int32_t>(std::lround(width * 25.4 / widthMm)) : 72;
            screens.push_back(XcbScreen{
                .index         = index,
                .xScreenNumber = static_cast<int>(index),
                .root          = screen->root,
                .screen =
                    ScreenInfo{
                               .x       = 0,
                               .y       = 0,
                               .width   = width,
                               .height  = height,
                               .dpi     = dpi,
                               .name    = fmtlib::format("screen-{}", index),
                               .primary = static_cast<int>(index) == mConnection->defaultScreen(),
                               },
            });
        }
        if (screens.empty()) {
            throw std::runtime_error("XCB display has no screens");
        }

        {
            auto lock = std::scoped_lock(mScreenMutex);
            mScreens  = std::move(screens);
        }
        for (const auto &screen : this->screens()) {
            SPDLOG_INFO("X screen {} at ({}, {}) size {}x{} dpi={} primary={}", screen.name,
                        screen.x, screen.y, screen.width, screen.height, screen.dpi,
                        screen.primary);
        }
    }

    std::string                    mDisplayName;
    std::unique_ptr<XcbConnection> mConnection;
    xcb_window_t                   mRoot                = XCB_NONE;
    uint16_t                       mRootWidth           = 0;
    uint16_t                       mRootHeight          = 0;
    uint8_t                        mXiOpcode            = 0;
    bool                           mRawEventsDuringGrab = false;

    std::weak_ptr<XcbInputCapture>  mInputCapture;
    std::weak_ptr<XcbInputInjector> mInputInjector;
    mutable std::mutex              mScreenMutex;
    std::vector<XcbScreen>          mScreens;
};

class XcbInputCapture final : public InputCapture {
public:
    explicit XcbInputCapture(std::shared_ptr<XcbPlatform> platform) : mPlatform(std::move(platform))
    {
    }

    ~XcbInputCapture() override { closeConnection(); }

    auto initialize() -> IoTask<void> override
    {
        closeConnection();
        auto connection = XcbConnection::connect(mPlatform->displayName());
        if (!connection) {
            co_return Err(connection.error());
        }
        mConnection = std::move(*connection);

        if (auto selected = selectRawInput(); !selected) {
            auto error = selected.error();
            closeConnection();
            co_return Err(error);
        }

        mKeySymbols.reset(xcb_key_symbols_alloc(mConnection->get()));
        if (!mKeySymbols) {
            closeConnection();
            co_return Err(makeIoError(std::errc::not_enough_memory));
        }

        ILIAS_CO_TRY(auto poller, co_await ilias::Poller::make(mConnection->fileDescriptor(),
                                                               ilias::IoDescriptor::Socket));
        mPoller = std::move(poller);
        SPDLOG_INFO("XInput2 capture started through a dedicated XCB connection");
        co_return {};
    }

    auto shutdown() -> Task<void> override
    {
        closeConnection();
        co_return;
    }

    auto setRemoteControlActive(bool active) -> IoResult<void> override
    {
        if (!mConnection) {
            return Err(makeIoError(std::errc::not_connected));
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

    auto moveLocalCursor(uint32_t screenIndex, int32_t x, int32_t y) -> IoResult<void> override
    {
        if (!mConnection) {
            return Err(makeIoError(std::errc::not_connected));
        }
        const auto global = mPlatform->localToGlobal(screenIndex, x, y);
        if (!global || !coordinateFits(global->first) || !coordinateFits(global->second)) {
            return Err(makeIoError(std::errc::invalid_argument));
        }
        const auto root  = mPlatform->root(screenIndex).value_or(mPlatform->root());
        auto       moved = mConnection->check(xcb_warp_pointer_checked(
            mConnection->get(), XCB_NONE, root, 0, 0, 0, 0, static_cast<int16_t>(global->first),
            static_cast<int16_t>(global->second)));
        if (!moved) {
            return Err(moved.error());
        }
        mLastCorePointer = *global;
        return mConnection->flush();
    }

    auto nextEvent() -> Task<InputEvent> override
    {
        if (!mConnection || !mPoller) {
            throw std::runtime_error("InputCapture::nextEvent called after shutdown");
        }

        while (true) {
            while (auto *rawEvent = xcb_poll_for_event(mConnection->get())) {
                XcbPtr<xcb_generic_event_t> event{rawEvent};
                if (auto translated = translateEvent(event.get())) {
                    SPDLOG_TRACE("XInput2 capture event {}", *translated);
                    co_return std::move(*translated);
                }
            }
            if (xcb_connection_has_error(mConnection->get()) != 0) {
                throw std::runtime_error("XInput2 XCB connection failed");
            }

            auto pollResult = co_await mPoller.poll(POLLIN);
            if (!pollResult) {
                throw std::system_error(pollResult.error(), "XInput2 capture poll failed");
            }
            if ((*pollResult & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                throw std::runtime_error("XInput2 capture fd closed or failed");
            }
        }
    }

private:
    struct XiEventMask {
        xcb_input_event_mask_t header;
        uint32_t               mask;
    };

    auto selectRawInput() const -> IoResult<void>
    {
        const auto mask = static_cast<uint32_t>(
            XCB_INPUT_XI_EVENT_MASK_RAW_KEY_PRESS | XCB_INPUT_XI_EVENT_MASK_RAW_KEY_RELEASE |
            XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_PRESS | XCB_INPUT_XI_EVENT_MASK_RAW_BUTTON_RELEASE |
            XCB_INPUT_XI_EVENT_MASK_RAW_MOTION);
        const auto events = XiEventMask{
            .header =
                {
                         .deviceid = XCB_INPUT_DEVICE_ALL_MASTER,
                         .mask_len = 1,
                         },
            .mask = mask,
        };
        auto selected = mConnection->check(xcb_input_xi_select_events_checked(
            mConnection->get(), mPlatform->root(), 1, &events.header));
        if (!selected) {
            SPDLOG_ERROR("XISelectEvents failed through XCB");
            return Err(selected.error());
        }
        return mConnection->flush();
    }

    auto closeConnection() -> void
    {
        if (mPoller) {
            auto ignored = mPoller.cancel();
            mPoller.close();
        }
        if (mConnection) {
            releaseRemoteControl();
            if (mHiddenCursor != XCB_NONE) {
                xcb_free_cursor(mConnection->get(), mHiddenCursor);
                mHiddenCursor = XCB_NONE;
            }
        }
        mKeySymbols.reset();
        mConnection.reset();
        SPDLOG_INFO("XInput2 capture stopped");
    }

    static auto grabStatusName(uint8_t status) -> std::string_view
    {
        switch (status) {
        case XCB_GRAB_STATUS_SUCCESS:
            return "success";
        case XCB_GRAB_STATUS_ALREADY_GRABBED:
            return "already grabbed";
        case XCB_GRAB_STATUS_INVALID_TIME:
            return "invalid time";
        case XCB_GRAB_STATUS_NOT_VIEWABLE:
            return "not viewable";
        case XCB_GRAB_STATUS_FROZEN:
            return "frozen";
        default:
            return "unknown";
        }
    }

    static auto grabStatusError(uint8_t status) -> std::error_code
    {
        switch (status) {
        case XCB_GRAB_STATUS_ALREADY_GRABBED:
        case XCB_GRAB_STATUS_FROZEN:
            return makeIoError(std::errc::device_or_resource_busy);
        case XCB_GRAB_STATUS_INVALID_TIME:
        case XCB_GRAB_STATUS_NOT_VIEWABLE:
            return makeIoError(std::errc::invalid_argument);
        default:
            return makeIoError(std::errc::io_error);
        }
    }

    auto ensureHiddenCursor() -> IoResult<xcb_cursor_t>
    {
        if (mHiddenCursor != XCB_NONE) {
            return mHiddenCursor;
        }

        const auto pixmap = xcb_generate_id(mConnection->get());
        auto       result = mConnection->check(
            xcb_create_pixmap_checked(mConnection->get(), 1, pixmap, mPlatform->root(), 1, 1));
        if (!result) {
            return Err(result.error());
        }

        const auto     gc           = xcb_generate_id(mConnection->get());
        const uint32_t foreground[] = {0};
        result                      = mConnection->check(
            xcb_create_gc_checked(mConnection->get(), gc, pixmap, XCB_GC_FOREGROUND, foreground));
        if (!result) {
            xcb_free_pixmap(mConnection->get(), pixmap);
            return Err(result.error());
        }
        const xcb_rectangle_t rectangle{.x = 0, .y = 0, .width = 1, .height = 1};
        result = mConnection->check(
            xcb_poly_fill_rectangle_checked(mConnection->get(), pixmap, gc, 1, &rectangle));

        const auto cursor = xcb_generate_id(mConnection->get());
        if (result) {
            result = mConnection->check(xcb_create_cursor_checked(
                mConnection->get(), cursor, pixmap, pixmap, 0, 0, 0, 0, 0, 0, 0, 0));
        }
        xcb_free_gc(mConnection->get(), gc);
        xcb_free_pixmap(mConnection->get(), pixmap);
        if (!result) {
            return Err(result.error());
        }
        mHiddenCursor = cursor;
        return mHiddenCursor;
    }

    auto destroyGrabWindow() -> void
    {
        if (mGrabWindow != XCB_NONE) {
            xcb_destroy_window(mConnection->get(), mGrabWindow);
            mGrabWindow = XCB_NONE;
        }
    }

    auto ensureGrabWindow() -> IoResult<xcb_window_t>
    {
        if (mGrabWindow != XCB_NONE) {
            return mGrabWindow;
        }

        const auto window = xcb_generate_id(mConnection->get());
        const auto eventMask =
            static_cast<uint32_t>(XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_MOTION |
                                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
                                  XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE);
        const uint32_t values[]  = {1, eventMask};
        const auto     valueMask = XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
        auto           result    = mConnection->check(xcb_create_window_checked(
            mConnection->get(), XCB_COPY_FROM_PARENT, window, mPlatform->root(), 0, 0,
            mPlatform->rootWidth(), mPlatform->rootHeight(), 0, XCB_WINDOW_CLASS_INPUT_ONLY,
            XCB_COPY_FROM_PARENT, valueMask, values));
        if (!result) {
            return Err(result.error());
        }
        result = mConnection->check(xcb_map_window_checked(mConnection->get(), window));
        if (!result) {
            xcb_destroy_window(mConnection->get(), window);
            return Err(result.error());
        }
        mGrabWindow = window;
        return mGrabWindow;
    }

    auto acquireRemoteControl() -> IoResult<void>
    {
        auto cursor = ensureHiddenCursor();
        if (!cursor) {
            return Err(cursor.error());
        }
        auto window = ensureGrabWindow();
        if (!window) {
            return Err(window.error());
        }

        const auto eventMask =
            static_cast<uint16_t>(XCB_EVENT_MASK_POINTER_MOTION | XCB_EVENT_MASK_BUTTON_MOTION |
                                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE);
        xcb_generic_error_t             *error = nullptr;
        XcbPtr<xcb_grab_pointer_reply_t> pointer{xcb_grab_pointer_reply(
            mConnection->get(),
            xcb_grab_pointer(mConnection->get(), 0, *window, eventMask, XCB_GRAB_MODE_ASYNC,
                             XCB_GRAB_MODE_ASYNC, XCB_NONE, *cursor, XCB_CURRENT_TIME),
            &error)};
        if (protocolError(error, "GrabPointer") || !pointer) {
            destroyGrabWindow();
            return Err(makeIoError(std::errc::io_error));
        }
        if (pointer->status != XCB_GRAB_STATUS_SUCCESS) {
            SPDLOG_WARN("GrabPointer failed: {}", grabStatusName(pointer->status));
            destroyGrabWindow();
            return Err(grabStatusError(pointer->status));
        }
        mPointerGrabbed = true;

        error = nullptr;
        XcbPtr<xcb_grab_keyboard_reply_t> keyboard{xcb_grab_keyboard_reply(
            mConnection->get(),
            xcb_grab_keyboard(mConnection->get(), 0, *window, XCB_CURRENT_TIME, XCB_GRAB_MODE_ASYNC,
                              XCB_GRAB_MODE_ASYNC),
            &error)};
        if (!protocolError(error, "GrabKeyboard") && keyboard &&
            keyboard->status == XCB_GRAB_STATUS_SUCCESS) {
            mKeyboardGrabbed = true;
        }
        else if (keyboard) {
            SPDLOG_WARN("GrabKeyboard failed: {}; local key events may leak",
                        grabStatusName(keyboard->status));
        }

        mRemoteControlActive = true;
        mLastCorePointer     = queryPointer();
        auto flushed         = mConnection->flush();
        if (!flushed) {
            releaseRemoteControl();
            return Err(flushed.error());
        }
        SPDLOG_INFO("X11 remote control grab enabled through XCB");
        return {};
    }

    auto releaseRemoteControl() -> void
    {
        if (!mConnection) {
            return;
        }
        if (mKeyboardGrabbed) {
            xcb_ungrab_keyboard(mConnection->get(), XCB_CURRENT_TIME);
            mKeyboardGrabbed = false;
        }
        if (mPointerGrabbed) {
            xcb_ungrab_pointer(mConnection->get(), XCB_CURRENT_TIME);
            mPointerGrabbed = false;
        }
        destroyGrabWindow();
        mLastCorePointer.reset();
        if (mRemoteControlActive) {
            mRemoteControlActive = false;
            auto ignored         = mConnection->flush();
            SPDLOG_INFO("X11 remote control grab disabled");
        }
    }

    auto queryPointer() const -> std::optional<std::pair<int32_t, int32_t>>
    {
        xcb_generic_error_t              *error = nullptr;
        XcbPtr<xcb_query_pointer_reply_t> reply{xcb_query_pointer_reply(
            mConnection->get(), xcb_query_pointer(mConnection->get(), mPlatform->root()), &error)};
        if (protocolError(error, "QueryPointer") || !reply || reply->same_screen == 0) {
            return std::nullopt;
        }
        return std::pair{static_cast<int32_t>(reply->root_x), static_cast<int32_t>(reply->root_y)};
    }

    static auto rawMotionDelta(const xcb_input_raw_motion_event_t &event)
        -> std::pair<int32_t, int32_t>
    {
        const auto *mask       = xcb_input_raw_button_press_valuator_mask(&event);
        const auto *values     = xcb_input_raw_button_press_axisvalues_raw(&event);
        auto        valueIndex = 0;
        auto        deltaX     = 0.0;
        auto        deltaY     = 0.0;
        for (auto axis = 0; axis < event.valuators_len * 32; ++axis) {
            if ((mask[axis / 32] & (uint32_t{1} << (axis % 32))) == 0) {
                continue;
            }
            if (axis == 0) {
                deltaX = fixed3232(values[valueIndex]);
            }
            else if (axis == 1) {
                deltaY = fixed3232(values[valueIndex]);
            }
            ++valueIndex;
        }
        return {static_cast<int32_t>(std::lround(deltaX)),
                static_cast<int32_t>(std::lround(deltaY))};
    }

    auto translateCoreMouseMove(const xcb_motion_notify_event_t &event) -> std::optional<InputEvent>
    {
        const auto rootPoint =
            std::pair{static_cast<int32_t>(event.root_x), static_cast<int32_t>(event.root_y)};
        auto [screenIndex, local] = mPlatform->globalToScreen(rootPoint.first, rootPoint.second);
        auto deltaX               = 0;
        auto deltaY               = 0;
        if (mLastCorePointer) {
            deltaX = rootPoint.first - mLastCorePointer->first;
            deltaY = rootPoint.second - mLastCorePointer->second;
        }
        mLastCorePointer = rootPoint;
        return InputEvent{
            MouseMoveEvent{
                           .x           = local.first,
                           .y           = local.second,
                           .screenIndex = screenIndex,
                           .deltaX      = deltaX,
                           .deltaY      = deltaY,
                           }
        };
    }

    auto screenLocalPoint(std::optional<std::pair<int32_t, int32_t>> rootPoint = {}) const
        -> std::optional<std::pair<uint32_t, std::pair<int32_t, int32_t>>>
    {
        auto pointer = rootPoint ? rootPoint : queryPointer();
        if (!pointer) {
            return std::nullopt;
        }
        return mPlatform->globalToScreen(pointer->first, pointer->second);
    }

    auto translateButton(uint32_t detail, bool release,
                         std::optional<std::pair<int32_t, int32_t>> rootPoint = {}) const
        -> std::optional<InputEvent>
    {
        const auto pointer = screenLocalPoint(rootPoint);
        if (!pointer) {
            return std::nullopt;
        }
        const auto &[screenIndex, local] = *pointer;
        if (detail >= 1 && detail <= 3) {
            const auto button = detail == 1   ? MouseButton::Left
                                : detail == 2 ? MouseButton::Middle
                                              : MouseButton::Right;
            return InputEvent{
                MouseButtonEvent{
                                 .x           = local.first,
                                 .y           = local.second,
                                 .screenIndex = screenIndex,
                                 .button      = button,
                                 .release     = release,
                                 }
            };
        }
        if (detail >= 4 && detail <= 7 && !release) {
            return InputEvent{
                MouseWheelEvent{
                                .x      = local.first,
                                .y      = local.second,
                                .deltaX = detail == 6 ? -120 : (detail == 7 ? 120 : 0),
                                .deltaY = detail == 4 ? 120 : (detail == 5 ? -120 : 0),
                                }
            };
        }
        return std::nullopt;
    }

    static auto keyModifierFor(Key key) -> KeyModifier
    {
        switch (key) {
        case Key::LeftCtrl:
            return KeyModifier::LeftCtrl;
        case Key::RightCtrl:
            return KeyModifier::RightCtrl;
        case Key::LeftShift:
            return KeyModifier::LeftShift;
        case Key::RightShift:
            return KeyModifier::RightShift;
        case Key::LeftAlt:
            return KeyModifier::LeftAlt;
        case Key::RightAlt:
            return KeyModifier::RightAlt;
        case Key::LeftMeta:
            return KeyModifier::LeftMeta;
        case Key::RightMeta:
            return KeyModifier::RightMeta;
        default:
            return KeyModifier::None;
        }
    }

    static auto translateKeySym(xcb_keysym_t keySym) -> Key
    {
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
        case XK_0:
            return Digit0;
        case XK_Return:
            return Enter;
        case XK_KP_Enter:
            return KeypadEnter;
        case XK_Escape:
            return Esc;
        case XK_BackSpace:
            return Backspace;
        case XK_Tab:
            return Tab;
        case XK_space:
            return Space;
        case XK_minus:
            return Minus;
        case XK_equal:
            return Equal;
        case XK_bracketleft:
            return LeftBrace;
        case XK_bracketright:
            return RightBrace;
        case XK_backslash:
            return Backslash;
        case XK_semicolon:
            return Semicolon;
        case XK_apostrophe:
            return Apostrophe;
        case XK_grave:
            return Grave;
        case XK_comma:
            return Comma;
        case XK_period:
            return Dot;
        case XK_slash:
            return Slash;
        case XK_Caps_Lock:
            return CapsLock;
        case XK_F1:
            return F1;
        case XK_F2:
            return F2;
        case XK_F3:
            return F3;
        case XK_F4:
            return F4;
        case XK_F5:
            return F5;
        case XK_F6:
            return F6;
        case XK_F7:
            return F7;
        case XK_F8:
            return F8;
        case XK_F9:
            return F9;
        case XK_F10:
            return F10;
        case XK_F11:
            return F11;
        case XK_F12:
            return F12;
        case XK_Print:
            return SysRq;
        case XK_Scroll_Lock:
            return ScrollLock;
        case XK_Pause:
            return Pause;
        case XK_Insert:
            return Insert;
        case XK_Home:
            return Home;
        case XK_Page_Up:
            return PageUp;
        case XK_Delete:
            return Delete;
        case XK_End:
            return End;
        case XK_Page_Down:
            return PageDown;
        case XK_Right:
            return Right;
        case XK_Left:
            return Left;
        case XK_Down:
            return Down;
        case XK_Up:
            return Up;
        case XK_Num_Lock:
            return NumLock;
        case XK_KP_Divide:
            return KeypadSlash;
        case XK_KP_Multiply:
            return KeypadAsterisk;
        case XK_KP_Subtract:
            return KeypadMinus;
        case XK_KP_Add:
            return KeypadPlus;
        case XK_KP_0:
            return Keypad0;
        case XK_KP_1:
            return Keypad1;
        case XK_KP_2:
            return Keypad2;
        case XK_KP_3:
            return Keypad3;
        case XK_KP_4:
            return Keypad4;
        case XK_KP_5:
            return Keypad5;
        case XK_KP_6:
            return Keypad6;
        case XK_KP_7:
            return Keypad7;
        case XK_KP_8:
            return Keypad8;
        case XK_KP_9:
            return Keypad9;
        case XK_KP_Decimal:
            return KeypadDot;
        case XK_Control_L:
            return LeftCtrl;
        case XK_Control_R:
            return RightCtrl;
        case XK_Shift_L:
            return LeftShift;
        case XK_Shift_R:
            return RightShift;
        case XK_Alt_L:
            return LeftAlt;
        case XK_Alt_R:
            return RightAlt;
        case XK_Super_L:
            return LeftMeta;
        case XK_Super_R:
            return RightMeta;
        default:
            return None;
        }
    }

    auto currentModifiers() const -> KeyModifier
    {
        xcb_generic_error_t             *error = nullptr;
        XcbPtr<xcb_query_keymap_reply_t> reply{xcb_query_keymap_reply(
            mConnection->get(), xcb_query_keymap(mConnection->get()), &error)};
        if (protocolError(error, "QueryKeymap") || !reply) {
            return KeyModifier::None;
        }

        const auto pressed = [&](xcb_keysym_t keySym) {
            XcbPtr<xcb_keycode_t> codes{xcb_key_symbols_get_keycode(mKeySymbols.get(), keySym)};
            for (auto *code = codes.get(); code && *code != XCB_NO_SYMBOL; ++code) {
                if ((reply->keys[*code / 8] & (1 << (*code % 8))) != 0) {
                    return true;
                }
            }
            return false;
        };

        auto modifiers = KeyModifier::None;
        if (pressed(XK_Control_L)) {
            modifiers |= KeyModifier::LeftCtrl;
        }
        if (pressed(XK_Control_R)) {
            modifiers |= KeyModifier::RightCtrl;
        }
        if (pressed(XK_Shift_L)) {
            modifiers |= KeyModifier::LeftShift;
        }
        if (pressed(XK_Shift_R)) {
            modifiers |= KeyModifier::RightShift;
        }
        if (pressed(XK_Alt_L)) {
            modifiers |= KeyModifier::LeftAlt;
        }
        if (pressed(XK_Alt_R)) {
            modifiers |= KeyModifier::RightAlt;
        }
        if (pressed(XK_Super_L)) {
            modifiers |= KeyModifier::LeftMeta;
        }
        if (pressed(XK_Super_R)) {
            modifiers |= KeyModifier::RightMeta;
        }
        return modifiers;
    }

    auto translateKey(xcb_keycode_t keyCode, bool release) const -> std::optional<InputEvent>
    {
        const auto key = translateKeySym(xcb_key_symbols_get_keysym(mKeySymbols.get(), keyCode, 0));
        auto       modifiers = currentModifiers();
        if (const auto modifier = keyModifierFor(key); modifier != KeyModifier::None) {
            if (release) {
                modifiers &= ~modifier;
            }
            else {
                modifiers |= modifier;
            }
        }
        return InputEvent{
            KeyEvent{
                     .key        = key,
                     .modifiers  = modifiers,
                     .nativeCode = keyCode,
                     .repeat     = false,
                     .release    = release,
                     }
        };
    }

    auto translateCoreEvent(xcb_generic_event_t *event) -> std::optional<InputEvent>
    {
        switch (event->response_type & 0x7f) {
        case XCB_MOTION_NOTIFY:
            return translateCoreMouseMove(*reinterpret_cast<xcb_motion_notify_event_t *>(event));
        case XCB_BUTTON_PRESS:
        case XCB_BUTTON_RELEASE: {
            const auto &button = *reinterpret_cast<xcb_button_press_event_t *>(event);
            return translateButton(button.detail,
                                   (event->response_type & 0x7f) == XCB_BUTTON_RELEASE,
                                   std::pair{static_cast<int32_t>(button.root_x),
                                             static_cast<int32_t>(button.root_y)});
        }
        case XCB_KEY_PRESS:
        case XCB_KEY_RELEASE: {
            const auto &key = *reinterpret_cast<xcb_key_press_event_t *>(event);
            return translateKey(key.detail, (event->response_type & 0x7f) == XCB_KEY_RELEASE);
        }
        default:
            return std::nullopt;
        }
    }

    auto translateEvent(xcb_generic_event_t *event) -> std::optional<InputEvent>
    {
        if ((event->response_type & 0x7f) == 0) {
            const auto *error = reinterpret_cast<xcb_generic_error_t *>(event);
            SPDLOG_ERROR("Asynchronous XCB error={} major={} minor={}", error->error_code,
                         error->major_code, error->minor_code);
            return std::nullopt;
        }
        if ((event->response_type & 0x7f) == XCB_MAPPING_NOTIFY) {
            xcb_refresh_keyboard_mapping(mKeySymbols.get(),
                                         reinterpret_cast<xcb_mapping_notify_event_t *>(event));
            return std::nullopt;
        }
        if (mRemoteControlActive && !mPlatform->rawEventsDuringGrab()) {
            if (auto core = translateCoreEvent(event)) {
                return core;
            }
        }
        if ((event->response_type & 0x7f) != XCB_GE_GENERIC) {
            return std::nullopt;
        }

        const auto *generic = reinterpret_cast<xcb_ge_generic_event_t *>(event);
        if (generic->extension != mPlatform->xiOpcode()) {
            return std::nullopt;
        }
        switch (generic->event_type) {
        case XCB_INPUT_RAW_MOTION: {
            const auto &motion  = *reinterpret_cast<xcb_input_raw_motion_event_t *>(event);
            const auto  pointer = queryPointer();
            if (!pointer) {
                return std::nullopt;
            }
            auto [screenIndex, local] = mPlatform->globalToScreen(pointer->first, pointer->second);
            auto [deltaX, deltaY]     = rawMotionDelta(motion);
            return InputEvent{
                MouseMoveEvent{
                               .x           = local.first,
                               .y           = local.second,
                               .screenIndex = screenIndex,
                               .deltaX      = deltaX,
                               .deltaY      = deltaY,
                               }
            };
        }
        case XCB_INPUT_RAW_BUTTON_PRESS:
        case XCB_INPUT_RAW_BUTTON_RELEASE: {
            const auto &button = *reinterpret_cast<xcb_input_raw_button_press_event_t *>(event);
            return translateButton(button.detail,
                                   generic->event_type == XCB_INPUT_RAW_BUTTON_RELEASE);
        }
        case XCB_INPUT_RAW_KEY_PRESS:
        case XCB_INPUT_RAW_KEY_RELEASE: {
            const auto &key = *reinterpret_cast<xcb_input_raw_key_press_event_t *>(event);
            if (key.detail > std::numeric_limits<xcb_keycode_t>::max()) {
                return std::nullopt;
            }
            return translateKey(static_cast<xcb_keycode_t>(key.detail),
                                generic->event_type == XCB_INPUT_RAW_KEY_RELEASE);
        }
        default:
            return std::nullopt;
        }
    }

    std::unique_ptr<XcbConnection>             mConnection;
    XcbKeySymbolsPtr                           mKeySymbols;
    xcb_cursor_t                               mHiddenCursor        = XCB_NONE;
    xcb_window_t                               mGrabWindow          = XCB_NONE;
    bool                                       mRemoteControlActive = false;
    bool                                       mPointerGrabbed      = false;
    bool                                       mKeyboardGrabbed     = false;
    std::optional<std::pair<int32_t, int32_t>> mLastCorePointer;
    ilias::Poller                              mPoller;
    std::shared_ptr<XcbPlatform>               mPlatform;
};

class XcbInputInjector final : public InputInjector {
public:
    explicit XcbInputInjector(std::shared_ptr<XcbPlatform> platform)
        : mPlatform(std::move(platform))
    {
    }

    ~XcbInputInjector() override { closeConnection(); }

    auto initialize() -> IoTask<void> override
    {
        closeConnection();
        auto connection = XcbConnection::connect(mPlatform->displayName());
        if (!connection) {
            co_return Err(connection.error());
        }
        mConnection = std::move(*connection);

        const auto *extension = xcb_get_extension_data(mConnection->get(), &xcb_test_id);
        if (!extension || extension->present == 0) {
            closeConnection();
            co_return Err(makeIoError(std::errc::operation_not_supported));
        }

        xcb_generic_error_t                 *error = nullptr;
        XcbPtr<xcb_test_get_version_reply_t> reply{xcb_test_get_version_reply(
            mConnection->get(),
            xcb_test_get_version(mConnection->get(), XCB_TEST_MAJOR_VERSION,
                                 XCB_TEST_MINOR_VERSION),
            &error)};
        if (protocolError(error, "XTest GetVersion") || !reply) {
            closeConnection();
            co_return Err(makeIoError(std::errc::operation_not_supported));
        }

        mKeySymbols.reset(xcb_key_symbols_alloc(mConnection->get()));
        if (!mKeySymbols) {
            closeConnection();
            co_return Err(makeIoError(std::errc::not_enough_memory));
        }
        SPDLOG_INFO("Using XTest {}.{} through a dedicated XCB connection", reply->major_version,
                    reply->minor_version);
        co_return {};
    }

    auto shutdown() -> Task<void> override
    {
        closeConnection();
        co_return;
    }

    auto inject(const InputEvent &event) -> IoTask<void> override
    {
        if (!mConnection) {
            co_return Err(makeIoError(std::errc::not_connected));
        }

        SPDLOG_TRACE("XTest/XCB injecting event {}", event);
        auto error = std::error_code{};
        std::visit([&](const auto &value) { error = injectOne(value); }, event);
        if (error) {
            SPDLOG_WARN("XTest/XCB failed to inject event {}: {}", event, error.message());
            co_return Err(error);
        }
        co_return {};
    }

private:
    auto closeConnection() -> void
    {
        mKeySymbols.reset();
        mConnection.reset();
    }

    static auto buttonFor(MouseButton button) -> std::optional<uint8_t>
    {
        switch (button) {
        case MouseButton::Left:
            return 1;
        case MouseButton::Middle:
            return 2;
        case MouseButton::Right:
            return 3;
        case MouseButton::None:
            return std::nullopt;
        }
        return std::nullopt;
    }

    auto fakeInput(uint8_t type, uint8_t detail, xcb_window_t root = XCB_NONE, int16_t x = 0,
                   int16_t y = 0) -> std::error_code
    {
        auto result = mConnection->check(xcb_test_fake_input_checked(
            mConnection->get(), type, detail, XCB_CURRENT_TIME, root, x, y, 0));
        return result ? std::error_code{} : result.error();
    }

    auto queryPointer() const -> std::optional<std::pair<int32_t, int32_t>>
    {
        xcb_generic_error_t              *error = nullptr;
        XcbPtr<xcb_query_pointer_reply_t> reply{xcb_query_pointer_reply(
            mConnection->get(), xcb_query_pointer(mConnection->get(), mPlatform->root()), &error)};
        if (protocolError(error, "QueryPointer") || !reply || reply->same_screen == 0) {
            return std::nullopt;
        }
        return std::pair{static_cast<int32_t>(reply->root_x), static_cast<int32_t>(reply->root_y)};
    }

    auto moveCursor(uint32_t screenIndex, int32_t x, int32_t y) -> std::error_code
    {
        const auto global = mPlatform->localToGlobal(screenIndex, x, y);
        const auto root   = mPlatform->root(screenIndex);
        if (!global || !root || !coordinateFits(global->first) || !coordinateFits(global->second)) {
            return makeIoError(std::errc::invalid_argument);
        }
        if (auto error = fakeInput(XCB_MOTION_NOTIFY, 0, *root, static_cast<int16_t>(global->first),
                                   static_cast<int16_t>(global->second));
            error) {
            return error;
        }
        auto flushed = mConnection->flush();
        if (!flushed) {
            return flushed.error();
        }

        const auto observed = queryPointer();
        if (observed && (observed->first != global->first || observed->second != global->second)) {
            SPDLOG_WARN("XTest pointer is at ({}, {}), expected ({}, {})", observed->first,
                        observed->second, global->first, global->second);
        }
        return {};
    }

    auto fakeButton(uint8_t button, bool press) -> std::error_code
    {
        return fakeInput(press ? XCB_BUTTON_PRESS : XCB_BUTTON_RELEASE, button);
    }

    auto fakeButtonClick(uint8_t button, uint32_t count) -> std::error_code
    {
        for (auto index = 0U; index < count; ++index) {
            if (auto error = fakeButton(button, true); error) {
                return error;
            }
            if (auto error = fakeButton(button, false); error) {
                return error;
            }
        }
        auto flushed = mConnection->flush();
        return flushed ? std::error_code{} : flushed.error();
    }

    static auto wheelClickCount(int32_t delta) -> uint32_t
    {
        const auto magnitude = static_cast<uint32_t>(std::abs(delta));
        return std::max(1U, (magnitude + 119U) / 120U);
    }

    static auto keySymFor(Key key) -> std::optional<xcb_keysym_t>
    {
        using enum Key;
        if (key >= A && key <= Z) {
            return XK_a + (static_cast<uint32_t>(key) - static_cast<uint32_t>(A));
        }
        if (key >= Digit1 && key <= Digit9) {
            return XK_1 + (static_cast<uint32_t>(key) - static_cast<uint32_t>(Digit1));
        }
        if (key >= F1 && key <= F12) {
            return XK_F1 + (static_cast<uint32_t>(key) - static_cast<uint32_t>(F1));
        }
        if (key >= F13 && key <= F24) {
            return XK_F13 + (static_cast<uint32_t>(key) - static_cast<uint32_t>(F13));
        }
        if (key >= Keypad1 && key <= Keypad9) {
            return XK_KP_1 + (static_cast<uint32_t>(key) - static_cast<uint32_t>(Keypad1));
        }

        switch (key) {
        case Digit0:
            return XK_0;
        case Enter:
            return XK_Return;
        case KeypadEnter:
            return XK_KP_Enter;
        case Esc:
            return XK_Escape;
        case Backspace:
            return XK_BackSpace;
        case Tab:
            return XK_Tab;
        case Space:
            return XK_space;
        case Minus:
            return XK_minus;
        case Equal:
            return XK_equal;
        case LeftBrace:
            return XK_bracketleft;
        case RightBrace:
            return XK_bracketright;
        case Backslash:
            return XK_backslash;
        case Semicolon:
            return XK_semicolon;
        case Apostrophe:
            return XK_apostrophe;
        case Grave:
            return XK_grave;
        case Comma:
            return XK_comma;
        case Dot:
            return XK_period;
        case Slash:
            return XK_slash;
        case CapsLock:
            return XK_Caps_Lock;
        case SysRq:
            return XK_Print;
        case ScrollLock:
            return XK_Scroll_Lock;
        case Pause:
            return XK_Pause;
        case Insert:
            return XK_Insert;
        case Home:
            return XK_Home;
        case PageUp:
            return XK_Page_Up;
        case Delete:
            return XK_Delete;
        case End:
            return XK_End;
        case PageDown:
            return XK_Page_Down;
        case Right:
            return XK_Right;
        case Left:
            return XK_Left;
        case Down:
            return XK_Down;
        case Up:
            return XK_Up;
        case NumLock:
            return XK_Num_Lock;
        case KeypadSlash:
            return XK_KP_Divide;
        case KeypadAsterisk:
            return XK_KP_Multiply;
        case KeypadMinus:
            return XK_KP_Subtract;
        case KeypadPlus:
            return XK_KP_Add;
        case Keypad0:
            return XK_KP_0;
        case KeypadDot:
            return XK_KP_Decimal;
        case LeftCtrl:
            return XK_Control_L;
        case RightCtrl:
            return XK_Control_R;
        case LeftShift:
            return XK_Shift_L;
        case RightShift:
            return XK_Shift_R;
        case LeftAlt:
            return XK_Alt_L;
        case RightAlt:
            return XK_Alt_R;
        case LeftMeta:
            return XK_Super_L;
        case RightMeta:
            return XK_Super_R;
        case VolumeUp:
        case MediaVolumeUp:
            return XF86XK_AudioRaiseVolume;
        case VolumeDown:
        case MediaVolumeDown:
            return XF86XK_AudioLowerVolume;
        case Mute:
        case MediaMute:
            return XF86XK_AudioMute;
        case MediaPlayPause:
            return XF86XK_AudioPlay;
        case MediaStopCd:
        case MediaStop:
            return XF86XK_AudioStop;
        case MediaPreviousSong:
            return XF86XK_AudioPrev;
        case MediaNextSong:
            return XF86XK_AudioNext;
        default:
            return std::nullopt;
        }
    }

    auto keyCodeFor(const KeyEvent &event) const -> std::optional<xcb_keycode_t>
    {
        if (const auto keySym = keySymFor(event.key)) {
            XcbPtr<xcb_keycode_t> codes{xcb_key_symbols_get_keycode(mKeySymbols.get(), *keySym)};
            if (codes && codes.get()[0] != XCB_NO_SYMBOL) {
                return codes.get()[0];
            }
        }
        if (event.nativeCode > 0 && event.nativeCode <= std::numeric_limits<xcb_keycode_t>::max()) {
            return static_cast<xcb_keycode_t>(event.nativeCode);
        }
        return std::nullopt;
    }

    auto injectOne(const MouseMoveEvent &event) -> std::error_code
    {
        return moveCursor(event.screenIndex, event.x, event.y);
    }

    auto injectOne(const MouseButtonEvent &event) -> std::error_code
    {
        if (auto error = moveCursor(event.screenIndex, event.x, event.y); error) {
            return error;
        }
        const auto button = buttonFor(event.button);
        if (!button) {
            return makeIoError(std::errc::invalid_argument);
        }
        if (auto error = fakeButton(*button, !event.release); error) {
            return error;
        }
        auto flushed = mConnection->flush();
        return flushed ? std::error_code{} : flushed.error();
    }

    auto injectOne(const MouseWheelEvent &event) -> std::error_code
    {
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

    auto injectOne(const KeyEvent &event) -> std::error_code
    {
        const auto keyCode = keyCodeFor(event);
        if (!keyCode) {
            return makeIoError(std::errc::invalid_argument);
        }
        if (auto error = fakeInput(event.release ? XCB_KEY_RELEASE : XCB_KEY_PRESS, *keyCode);
            error) {
            return error;
        }
        auto flushed = mConnection->flush();
        return flushed ? std::error_code{} : flushed.error();
    }

    std::unique_ptr<XcbConnection> mConnection;
    XcbKeySymbolsPtr               mKeySymbols;
    std::shared_ptr<XcbPlatform>   mPlatform;
};

auto XcbPlatform::createCapture() -> InputCapture::Ptr
{
    if (!mInputCapture.expired()) {
        throw std::runtime_error("InputCapture already created");
    }
    auto capture  = std::make_shared<XcbInputCapture>(shared_from_this());
    mInputCapture = capture;
    return capture;
}

auto XcbPlatform::createInjector() -> InputInjector::Ptr
{
    if (!mInputInjector.expired()) {
        throw std::runtime_error("InputInjector already created");
    }
    auto injector  = std::make_shared<XcbInputInjector>(shared_from_this());
    mInputInjector = injector;
    return injector;
}

namespace
{

    auto createX11Backend() -> Platform::Ptr
    {
        try {
            return std::make_shared<XcbPlatform>();
        }
        catch (const std::exception &exception) {
            SPDLOG_ERROR("Failed to create XCB platform: {}", exception.what());
            return nullptr;
        }
    }

    auto checkX11Backend() -> Task<BackendCheck>
    {
        if (isWaylandSession() || envIsSet("WAYLAND_DISPLAY")) {
            co_return BackendCheck{
                .available = false,
                .detail = "Wayland session detected; XWayland is not a system-wide input backend",
                .screens =
                    {
                              .supported = false,
                              .detail    = "X11 output discovery is not used in a Wayland session",
                              },
                .capture =
                    {
                              .supported = false,
                              .detail    = "XWayland cannot capture native Wayland input",
                              },
                .injection =
                    {
                              .supported = false,
                              .detail    = "XWayland cannot inject into native Wayland windows",
                              },
            };
        }
        co_return co_await probeBackend("X11/XCB", createX11Backend);
    }

    const BackendRegistration kX11BackendRegistration{
        BackendDescriptor{
                          .name        = "x11",
                          .displayName = "X11 (XCB/XInput2/XTest)",
                          .order       = 200,
                          .check       = checkX11Backend,
                          .create      = createX11Backend,
                          }
    };

} // namespace

MKS_END

#endif
