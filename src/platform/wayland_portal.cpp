#if defined(__linux__)

    #include "preinclude.hpp"

    #include <gio/gio.h>
    #include <libei.h>
    #include <libportal/inputcapture.h>
    #include <libportal/portal.h>
    #include <libportal/remote.h>
    #include <linux/input-event-codes.h>
    #include <poll.h>

    #include <algorithm>
    #include <cmath>
    #include <cstdint>
    #include <cstdlib>
    #include <deque>
    #include <functional>
    #include <limits>
    #include <memory>
    #include <mutex>
    #include <optional>
    #include <string>
    #include <system_error>
    #include <thread>
    #include <utility>
    #include <vector>

    #include <ilias/net/poller.hpp>
    #include <ilias/sync/oneshot.hpp>
    #include <spdlog/spdlog.h>

    #include "backend.hpp"
    #include "platform.hpp"
    #include "wayland_keymap.hpp"

MKS_BEGIN

class PortalInputCapture;
class PortalInputInjector;

namespace
{
    auto makeIoError(std::errc error) -> std::error_code
    {
        return std::make_error_code(error);
    }

    auto portalError(const GError *error) -> std::error_code
    {
        if (!error) {
            return makeIoError(std::errc::io_error);
        }
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
            return makeIoError(std::errc::operation_canceled);
        }
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED) ||
            g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_ACCESS_DENIED) ||
            g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_AUTH_FAILED)) {
            return makeIoError(std::errc::permission_denied);
        }
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED) ||
            g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD) ||
            g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_INTERFACE)) {
            return makeIoError(std::errc::operation_not_supported);
        }
        return makeIoError(std::errc::io_error);
    }

    template <typename T>
    using PortalReply = ilias::oneshot::Sender<IoResult<T>>;

    template <typename T>
    auto sendReply(void *data, IoResult<T> result) -> void
    {
        auto reply = std::unique_ptr<PortalReply<T>>{static_cast<PortalReply<T> *>(data)};
        (void)reply->send(std::move(result));
    }

    template <typename T>
    auto receiveReply(ilias::oneshot::Receiver<IoResult<T>> receiver) -> IoTask<T>
    {
        auto reply = co_await std::move(receiver);
        if (!reply) {
            co_return Err(makeIoError(std::errc::operation_canceled));
        }
        co_return std::move(*reply);
    }

    class PortalRuntime final {
    public:
        using Ptr = std::shared_ptr<PortalRuntime>;

        PortalRuntime()
        {
            mContext = g_main_context_new();
            if (!mContext) {
                throw std::runtime_error("Failed to create GLib main context");
            }
            mLoop = g_main_loop_new(mContext, FALSE);
            if (!mLoop) {
                g_main_context_unref(mContext);
                mContext = nullptr;
                throw std::runtime_error("Failed to create GLib main loop");
            }
            mPortal = xdp_portal_new();
            if (!mPortal) {
                g_main_loop_unref(mLoop);
                g_main_context_unref(mContext);
                mLoop    = nullptr;
                mContext = nullptr;
                throw std::runtime_error("Failed to create libportal context");
            }

            mThread = std::jthread{[this] {
                g_main_context_push_thread_default(mContext);
                g_main_loop_run(mLoop);
                g_main_context_pop_thread_default(mContext);
            }};
        }

        ~PortalRuntime()
        {
            if (mLoop) {
                g_main_loop_quit(mLoop);
            }
            if (mContext) {
                g_main_context_wakeup(mContext);
            }
            if (mThread.joinable()) {
                mThread.join();
            }
            g_clear_object(&mPortal);
            if (mLoop) {
                g_main_loop_unref(mLoop);
            }
            if (mContext) {
                g_main_context_unref(mContext);
            }
        }

        PortalRuntime(const PortalRuntime &)            = delete;
        PortalRuntime &operator=(const PortalRuntime &) = delete;

        auto portal() const -> XdpPortal * { return mPortal; }

        auto invoke(std::function<void()> function) const -> void
        {
            auto *owned = new std::function<void()>{std::move(function)};
            g_main_context_invoke_full(
                mContext, G_PRIORITY_DEFAULT,
                [](gpointer data) -> gboolean {
                    (*static_cast<std::function<void()> *>(data))();
                    return G_SOURCE_REMOVE;
                },
                owned, [](gpointer data) { delete static_cast<std::function<void()> *>(data); });
        }

        auto createInputCapture(GCancellable *cancellable) const -> IoTask<XdpInputCaptureSession *>
        {
            auto [sender, receiver] = ilias::oneshot::channel<IoResult<XdpInputCaptureSession *>>();
            auto *reply             = new PortalReply<XdpInputCaptureSession *>{std::move(sender)};
            invoke([portal = mPortal, cancellable, reply] {
                xdp_portal_create_input_capture_session(
                    portal, nullptr,
                    static_cast<XdpInputCapability>(XDP_INPUT_CAPABILITY_POINTER |
                                                    XDP_INPUT_CAPABILITY_KEYBOARD),
                    cancellable,
                    [](GObject *source, GAsyncResult *result, gpointer data) {
                        auto *error   = static_cast<GError *>(nullptr);
                        auto *session = xdp_portal_create_input_capture_session_finish(
                            XDP_PORTAL(source), result, &error);
                        if (!session) {
                            SPDLOG_WARN("InputCapture portal request failed: {}",
                                        error ? error->message : "unknown error");
                            const auto code = portalError(error);
                            g_clear_error(&error);
                            sendReply<XdpInputCaptureSession *>(data, Err(code));
                            return;
                        }
                        sendReply<XdpInputCaptureSession *>(data, session);
                    },
                    reply);
            });
            co_return co_await receiveReply(std::move(receiver));
        }

        auto createRemoteDesktop(GCancellable *cancellable) const -> IoTask<XdpSession *>
        {
            auto [sender, receiver] = ilias::oneshot::channel<IoResult<XdpSession *>>();
            auto *reply             = new PortalReply<XdpSession *>{std::move(sender)};
            invoke([portal = mPortal, cancellable, reply] {
                xdp_portal_create_remote_desktop_session(
                    portal, static_cast<XdpDeviceType>(XDP_DEVICE_POINTER | XDP_DEVICE_KEYBOARD),
                    XDP_OUTPUT_NONE, XDP_REMOTE_DESKTOP_FLAG_NONE, XDP_CURSOR_MODE_HIDDEN,
                    cancellable,
                    [](GObject *source, GAsyncResult *result, gpointer data) {
                        auto *error   = static_cast<GError *>(nullptr);
                        auto *session = xdp_portal_create_remote_desktop_session_finish(
                            XDP_PORTAL(source), result, &error);
                        if (!session) {
                            SPDLOG_WARN("RemoteDesktop portal request failed: {}",
                                        error ? error->message : "unknown error");
                            const auto code = portalError(error);
                            g_clear_error(&error);
                            sendReply<XdpSession *>(data, Err(code));
                            return;
                        }
                        sendReply<XdpSession *>(data, session);
                    },
                    reply);
            });
            co_return co_await receiveReply(std::move(receiver));
        }

        auto startRemoteDesktop(XdpSession *session, GCancellable *cancellable) const
            -> IoTask<void>
        {
            auto [sender, receiver] = ilias::oneshot::channel<IoResult<void>>();
            auto *reply             = new PortalReply<void>{std::move(sender)};
            g_object_ref(session);
            invoke([session, cancellable, reply] {
                xdp_session_start(
                    session, nullptr, cancellable,
                    [](GObject *source, GAsyncResult *result, gpointer data) {
                        auto *error = static_cast<GError *>(nullptr);
                        if (!xdp_session_start_finish(XDP_SESSION(source), result, &error)) {
                            SPDLOG_WARN("RemoteDesktop portal start failed: {}",
                                        error ? error->message : "unknown error");
                            const auto code = portalError(error);
                            g_clear_error(&error);
                            sendReply<void>(data, Err(code));
                            return;
                        }
                        sendReply<void>(data, {});
                    },
                    reply);
                g_object_unref(session);
            });
            co_return co_await receiveReply(std::move(receiver));
        }

        template <typename Session>
        auto connectToEis(Session *session) const -> IoTask<int>
        {
            auto [sender, receiver] = ilias::oneshot::channel<IoResult<int>>();
            auto *reply             = new PortalReply<int>{std::move(sender)};
            g_object_ref(session);
            invoke([session, reply] {
                auto *error = static_cast<GError *>(nullptr);
                auto  fd    = int{-1};
                if constexpr (std::is_same_v<Session, XdpInputCaptureSession>) {
                    fd = xdp_input_capture_session_connect_to_eis(session, &error);
                }
                else {
                    fd = xdp_session_connect_to_eis(session, &error);
                }
                g_object_unref(session);
                if (fd < 0) {
                    SPDLOG_WARN("Portal ConnectToEIS failed: {}",
                                error ? error->message : "unknown error");
                    const auto code = portalError(error);
                    g_clear_error(&error);
                    sendReply<int>(reply, Err(code));
                    return;
                }
                sendReply<int>(reply, fd);
            });
            co_return co_await receiveReply(std::move(receiver));
        }

    private:
        GMainContext *mContext = nullptr;
        GMainLoop    *mLoop    = nullptr;
        XdpPortal    *mPortal  = nullptr;
        std::jthread  mThread;
    };

    struct BarrierSetup {
        std::vector<ScreenInfo>                      screens;
        std::vector<XdpInputCapturePointerBarrier *> barriers;
        size_t                                       active = 0;
    };

    struct BarrierReply {
        PortalReply<BarrierSetup> sender;
        BarrierSetup              setup;
    };

    auto destroyBarriers(std::vector<XdpInputCapturePointerBarrier *> &barriers) -> void
    {
        for (auto *barrier : barriers) {
            g_object_unref(barrier);
        }
        barriers.clear();
    }

    auto zoneScreens(XdpInputCaptureSession *session) -> std::vector<ScreenInfo>
    {
        auto result = std::vector<ScreenInfo>{};
        auto index  = uint32_t{0};
        for (auto *item = xdp_input_capture_session_get_zones(session); item; item = item->next) {
            auto *zone = XDP_INPUT_CAPTURE_ZONE(item->data);
            auto  x = gint{0}, y = gint{0};
            auto  width = guint{0}, height = guint{0};
            g_object_get(zone, "x", &x, "y", &y, "width", &width, "height", &height, nullptr);
            if (width == 0 || height == 0) {
                continue;
            }
            result.push_back(ScreenInfo{
                .x     = x,
                .y     = y,
                .width = static_cast<int32_t>(
                    std::min(width, static_cast<guint>(std::numeric_limits<int32_t>::max()))),
                .height = static_cast<int32_t>(
                    std::min(height, static_cast<guint>(std::numeric_limits<int32_t>::max()))),
                .dpi     = 72,
                .name    = fmtlib::format("portal-zone-{}", index),
                .primary = index == 0,
            });
            ++index;
        }
        return result;
    }

    auto makeBarrier(uint32_t id, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
        -> XdpInputCapturePointerBarrier *
    {
        return XDP_INPUT_CAPTURE_POINTER_BARRIER(
            g_object_new(XDP_TYPE_INPUT_CAPTURE_POINTER_BARRIER, "id", id, "x1", x1, "y1", y1, "x2",
                         x2, "y2", y2, nullptr));
    }

    auto configureBarriers(const PortalRuntime::Ptr &runtime, XdpInputCaptureSession *session,
                           GCancellable *cancellable) -> IoTask<BarrierSetup>
    {
        auto [sender, receiver] = ilias::oneshot::channel<IoResult<BarrierSetup>>();
        auto *reply             = new BarrierReply{.sender = std::move(sender), .setup = {}};
        g_object_ref(session);
        runtime->invoke([session, cancellable, reply] {
            reply->setup.screens = zoneScreens(session);
            auto *list           = static_cast<GList *>(nullptr);
            auto  barrierId      = uint32_t{1};
            for (const auto &screen : reply->setup.screens) {
                const auto lastX  = screen.x + screen.width - 1;
                const auto lastY  = screen.y + screen.height - 1;
                const auto right  = screen.x + screen.width;
                const auto bottom = screen.y + screen.height;
                const auto specs  = std::array{
                    std::array{screen.x, screen.y, screen.x, lastY   },
                    std::array{right,    screen.y, right,    lastY   },
                    std::array{screen.x, screen.y, lastX,    screen.y},
                    std::array{screen.x, bottom,   lastX,    bottom  },
                };
                for (const auto &spec : specs) {
                    auto *barrier = makeBarrier(barrierId++, spec[0], spec[1], spec[2], spec[3]);
                    if (!barrier) {
                        continue;
                    }
                    reply->setup.barriers.push_back(barrier);
                    list = g_list_append(list, barrier);
                }
            }

            if (!list) {
                g_object_unref(session);
                auto sender = std::move(reply->sender);
                delete reply;
                (void)sender.send(Err(makeIoError(std::errc::no_such_device)));
                return;
            }

            xdp_input_capture_session_set_pointer_barriers(
                session, list, cancellable,
                [](GObject *source, GAsyncResult *result, gpointer data) {
                    auto  owned  = std::unique_ptr<BarrierReply>{static_cast<BarrierReply *>(data)};
                    auto *error  = static_cast<GError *>(nullptr);
                    auto *failed = xdp_input_capture_session_set_pointer_barriers_finish(
                        XDP_INPUT_CAPTURE_SESSION(source), result, &error);
                    if (error) {
                        SPDLOG_WARN("InputCapture portal rejected pointer barriers: {}",
                                    error->message);
                        const auto code = portalError(error);
                        g_clear_error(&error);
                        destroyBarriers(owned->setup.barriers);
                        (void)owned->sender.send(Err(code));
                        return;
                    }
                    g_list_free(failed);
                    for (auto *barrier : owned->setup.barriers) {
                        auto active = gboolean{FALSE};
                        g_object_get(barrier, "is-active", &active, nullptr);
                        owned->setup.active += active ? 1U : 0U;
                    }
                    if (owned->setup.active == 0) {
                        destroyBarriers(owned->setup.barriers);
                        (void)owned->sender.send(
                            Err(makeIoError(std::errc::operation_not_supported)));
                        return;
                    }
                    (void)owned->sender.send(std::move(owned->setup));
                },
                reply);
            g_object_unref(session);
        });
        co_return co_await receiveReply(std::move(receiver));
    }

    auto keyModifierFor(Key key) -> KeyModifier
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

    auto pointerButton(uint32_t button) -> MouseButton
    {
        switch (button) {
        case BTN_LEFT:
            return MouseButton::Left;
        case BTN_RIGHT:
            return MouseButton::Right;
        case BTN_MIDDLE:
            return MouseButton::Middle;
        default:
            return MouseButton::None;
        }
    }

    auto evdevButton(MouseButton button) -> std::optional<uint32_t>
    {
        switch (button) {
        case MouseButton::Left:
            return BTN_LEFT;
        case MouseButton::Right:
            return BTN_RIGHT;
        case MouseButton::Middle:
            return BTN_MIDDLE;
        case MouseButton::None:
            return std::nullopt;
        }
        return std::nullopt;
    }
} // namespace

class PortalPlatform final : public Platform, public std::enable_shared_from_this<PortalPlatform> {
public:
    struct LocalPoint {
        uint32_t screenIndex = 0;
        int32_t  x           = 0;
        int32_t  y           = 0;
    };

    PortalPlatform() : mRuntime(std::make_shared<PortalRuntime>()) {}

    auto screens() const -> std::vector<ScreenInfo> override
    {
        auto lock = std::scoped_lock{mMutex};
        return mScreens;
    }

    auto createCapture() -> InputCapture::Ptr override;
    auto createInjector() -> InputInjector::Ptr override;

    auto runtime() const -> const PortalRuntime::Ptr & { return mRuntime; }

    auto updateScreens(std::vector<ScreenInfo> screens) -> void
    {
        if (screens.empty()) {
            return;
        }
        auto lock = std::scoped_lock{mMutex};
        mScreens  = std::move(screens);
    }

    auto globalToLocal(double x, double y) const -> std::optional<LocalPoint>
    {
        auto lock = std::scoped_lock{mMutex};
        if (mScreens.empty()) {
            return std::nullopt;
        }
        for (auto index = uint32_t{0}; index < mScreens.size(); ++index) {
            const auto &screen = mScreens[index];
            if (x >= screen.x && y >= screen.y && x < screen.x + screen.width &&
                y < screen.y + screen.height) {
                return LocalPoint{
                    .screenIndex = index,
                    .x           = static_cast<int32_t>(std::floor(x - screen.x)),
                    .y           = static_cast<int32_t>(std::floor(y - screen.y)),
                };
            }
        }

        // Compositors may report a point exactly one unit beyond a barrier.
        // Associate it with the nearest output and clamp to that output.
        auto bestIndex    = uint32_t{0};
        auto bestDistance = std::numeric_limits<double>::max();
        for (auto index = uint32_t{0}; index < mScreens.size(); ++index) {
            const auto &screen   = mScreens[index];
            const auto  localX   = std::clamp(x, static_cast<double>(screen.x),
                                              static_cast<double>(screen.x + screen.width - 1));
            const auto  localY   = std::clamp(y, static_cast<double>(screen.y),
                                              static_cast<double>(screen.y + screen.height - 1));
            const auto  distance = std::hypot(x - localX, y - localY);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex    = index;
            }
        }
        const auto &screen = mScreens[bestIndex];
        return LocalPoint{
            .screenIndex = bestIndex,
            .x = std::clamp(static_cast<int32_t>(std::lround(x)) - screen.x, 0, screen.width - 1),
            .y = std::clamp(static_cast<int32_t>(std::lround(y)) - screen.y, 0, screen.height - 1),
        };
    }

    auto localToGlobal(uint32_t screenIndex, int32_t x, int32_t y) const
        -> std::optional<std::pair<double, double>>
    {
        auto lock = std::scoped_lock{mMutex};
        if (screenIndex >= mScreens.size()) {
            return std::nullopt;
        }
        const auto &screen = mScreens[screenIndex];
        return std::pair{
            static_cast<double>(screen.x + std::clamp(x, 0, screen.width - 1)),
            static_cast<double>(screen.y + std::clamp(y, 0, screen.height - 1)),
        };
    }

private:
    PortalRuntime::Ptr                 mRuntime;
    mutable std::mutex                 mMutex;
    std::vector<ScreenInfo>            mScreens;
    std::weak_ptr<PortalInputCapture>  mCapture;
    std::weak_ptr<PortalInputInjector> mInjector;
};

namespace
{
    struct CaptureState {
        std::mutex                    mutex;
        std::weak_ptr<PortalPlatform> platform;
        std::deque<InputEvent>        events;
        double                        globalX      = 0;
        double                        globalY      = 0;
        uint32_t                      activationId = 0;
        KeyModifier                   modifiers    = KeyModifier::None;
        bool                          active       = false;
        bool                          closed       = false;
    };

    auto captureStateDestroy(gpointer data, GClosure *) -> void
    {
        delete static_cast<std::shared_ptr<CaptureState> *>(data);
    }

    auto captureActivated(XdpInputCaptureSession *, guint activationId, GVariant *options,
                          gpointer data) -> void
    {
        const auto state = *static_cast<std::shared_ptr<CaptureState> *>(data);
        auto       x     = gdouble{0};
        auto       y     = gdouble{0};
        const auto hasPosition =
            options && g_variant_lookup(options, "cursor_position", "(dd)", &x, &y);
        auto lock           = std::scoped_lock{state->mutex};
        state->activationId = activationId;
        state->active       = true;
        if (hasPosition) {
            state->globalX = x;
            state->globalY = y;
        }
        SPDLOG_DEBUG("InputCapture portal activated id={} position=({}, {})", activationId,
                     state->globalX, state->globalY);
    }

    auto captureDeactivated(XdpInputCaptureSession *, guint activationId, GVariant *, gpointer data)
        -> void
    {
        const auto state = *static_cast<std::shared_ptr<CaptureState> *>(data);
        auto       lock  = std::scoped_lock{state->mutex};
        if (state->activationId == activationId) {
            state->activationId = 0;
            state->active       = false;
        }
    }

    auto captureDisabled(XdpInputCaptureSession *, GVariant *, gpointer data) -> void
    {
        const auto state    = *static_cast<std::shared_ptr<CaptureState> *>(data);
        auto       lock     = std::scoped_lock{state->mutex};
        state->activationId = 0;
        state->active       = false;
    }

    auto captureZonesChanged(XdpInputCaptureSession *session, GVariant *, gpointer data) -> void
    {
        const auto state    = *static_cast<std::shared_ptr<CaptureState> *>(data);
        const auto platform = state->platform.lock();
        if (platform) {
            platform->updateScreens(zoneScreens(session));
        }
        SPDLOG_WARN("InputCapture portal zones changed; restart the backend to "
                    "rebuild pointer "
                    "barriers");
    }
} // namespace

class PortalInputCapture final : public InputCapture {
public:
    explicit PortalInputCapture(std::shared_ptr<PortalPlatform> platform)
        : mPlatform(std::move(platform)), mState(std::make_shared<CaptureState>())
    {
        mState->platform = mPlatform;
    }

    ~PortalInputCapture() override { close(); }

    auto initialize() -> IoTask<void> override
    {
        close();
        mState           = std::make_shared<CaptureState>();
        mState->platform = mPlatform;
        mCancellable     = g_cancellable_new();
        if (!mCancellable) {
            co_return Err(makeIoError(std::errc::not_enough_memory));
        }

        auto created = co_await mPlatform->runtime()->createInputCapture(mCancellable);
        if (!created) {
            close();
            co_return Err(created.error());
        }
        mSession = *created;
        connectSignals();

        auto barriers = co_await configureBarriers(mPlatform->runtime(), mSession, mCancellable);
        if (!barriers) {
            close();
            co_return Err(barriers.error());
        }
        mPlatform->updateScreens(barriers->screens);
        mBarriers = std::move(barriers->barriers);

        auto connected = co_await mPlatform->runtime()->connectToEis(mSession);
        if (!connected) {
            close();
            co_return Err(connected.error());
        }
        mEi = ei_new_receiver(nullptr);
        if (!mEi) {
            ::close(*connected);
            close();
            co_return Err(makeIoError(std::errc::not_enough_memory));
        }
        ei_configure_name(mEi, "mksync Wayland input capture");
        const auto setup = ei_setup_backend_fd(mEi, *connected);
        if (setup < 0) {
            close();
            co_return Err(std::error_code{-setup, std::generic_category()});
        }

        auto poller = co_await ilias::Poller::make(ei_get_fd(mEi), ilias::IoDescriptor::Socket);
        if (!poller) {
            close();
            co_return Err(poller.error());
        }
        mPoller = std::move(*poller);
        mPlatform->runtime()->invoke(
            [session = mSession] { xdp_input_capture_session_enable(session); });

        SPDLOG_INFO("Wayland portal capture started outputs={} barriers={} libei={}",
                    mPlatform->screens().size(), barriers->active, ei_get_fd(mEi));
        co_return {};
    }

    auto shutdown() -> Task<void> override
    {
        close();
        co_return;
    }

    auto nextEvent() -> Task<InputEvent> override
    {
        while (true) {
            dispatchEi();
            {
                auto lock = std::scoped_lock{mState->mutex};
                if (!mState->events.empty()) {
                    auto event = std::move(mState->events.front());
                    mState->events.pop_front();
                    co_return event;
                }
                if (mState->closed) {
                    throw std::system_error(makeIoError(std::errc::not_connected));
                }
            }

            auto polled = co_await mPoller.poll(POLLIN);
            if (!polled) {
                throw std::system_error(polled.error());
            }
            if ((*polled & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                throw std::system_error(makeIoError(std::errc::connection_reset));
            }
        }
    }

    auto setRemoteControlActive(bool active) -> IoResult<void> override
    {
        if (!mSession) {
            return Err(makeIoError(std::errc::not_connected));
        }
        if (active) {
            return {};
        }
        // The router calls moveLocalCursor immediately after switching back to
        // a local screen. That call uses ReleaseAt with the desired position.
        return {};
    }

    auto moveLocalCursor(uint32_t screenIndex, int32_t x, int32_t y) -> IoResult<void> override
    {
        if (!mSession) {
            return Err(makeIoError(std::errc::not_connected));
        }
        const auto position = mPlatform->localToGlobal(screenIndex, x, y);
        if (!position) {
            return Err(makeIoError(std::errc::invalid_argument));
        }

        auto activationId = uint32_t{0};
        {
            auto lock            = std::scoped_lock{mState->mutex};
            activationId         = mState->activationId;
            mState->globalX      = position->first;
            mState->globalY      = position->second;
            mState->active       = false;
            mState->activationId = 0;
        }
        if (activationId == 0) {
            return {};
        }

        g_object_ref(mSession);
        mPlatform->runtime()->invoke([session = mSession, activationId, position = *position] {
            xdp_input_capture_session_release_at(session, activationId, position.first,
                                                 position.second);
            g_object_unref(session);
        });
        return {};
    }

private:
    auto connectSignals() -> void
    {
        const auto connect = [&](const char *name, GCallback callback) {
            auto *state = new std::shared_ptr<CaptureState>{mState};
            mSignalIds.push_back(g_signal_connect_data(mSession, name, callback, state,
                                                       captureStateDestroy, G_CONNECT_DEFAULT));
        };
        connect("activated", G_CALLBACK(captureActivated));
        connect("deactivated", G_CALLBACK(captureDeactivated));
        connect("disabled", G_CALLBACK(captureDisabled));
        connect("zones-changed", G_CALLBACK(captureZonesChanged));
    }

    auto close() -> void
    {
        if (mState) {
            auto lock      = std::scoped_lock{mState->mutex};
            mState->closed = true;
            mState->active = false;
        }
        if (mCancellable) {
            g_cancellable_cancel(mCancellable);
        }
        if (mPoller) {
            auto canceled = mPoller.cancel();
            (void)canceled;
            mPoller.close();
        }
        if (mSession) {
            for (const auto id : mSignalIds) {
                if (id != 0 && g_signal_handler_is_connected(mSession, id)) {
                    g_signal_handler_disconnect(mSession, id);
                }
            }
            mSignalIds.clear();
            xdp_input_capture_session_disable(mSession);
            xdp_session_close(xdp_input_capture_session_get_session(mSession));
        }
        if (mEi) {
            ei_unref(mEi);
            mEi = nullptr;
        }
        destroyBarriers(mBarriers);
        g_clear_object(&mSession);
        g_clear_object(&mCancellable);
    }

    auto queue(InputEvent event) -> void
    {
        auto lock = std::scoped_lock{mState->mutex};
        mState->events.push_back(std::move(event));
    }

    auto pointerPoint() const -> std::optional<PortalPlatform::LocalPoint>
    {
        auto lock = std::scoped_lock{mState->mutex};
        return mPlatform->globalToLocal(mState->globalX, mState->globalY);
    }

    auto dispatchEi() -> void
    {
        if (!mEi) {
            return;
        }
        ei_dispatch(mEi);
        while (auto *event = ei_get_event(mEi)) {
            processEiEvent(event);
            ei_event_unref(event);
        }
    }

    auto processEiEvent(ei_event *event) -> void
    {
        switch (ei_event_get_type(event)) {
        case EI_EVENT_SEAT_ADDED:
            ei_seat_bind_capabilities(ei_event_get_seat(event), EI_DEVICE_CAP_POINTER,
                                      EI_DEVICE_CAP_POINTER_ABSOLUTE, EI_DEVICE_CAP_BUTTON,
                                      EI_DEVICE_CAP_SCROLL, EI_DEVICE_CAP_KEYBOARD, nullptr);
            break;
        case EI_EVENT_POINTER_MOTION: {
            const auto dx = ei_event_pointer_get_dx(event);
            const auto dy = ei_event_pointer_get_dy(event);
            auto       x  = double{0};
            auto       y  = double{0};
            {
                auto lock = std::scoped_lock{mState->mutex};
                mState->globalX += dx;
                mState->globalY += dy;
                x = mState->globalX;
                y = mState->globalY;
            }
            if (const auto point = mPlatform->globalToLocal(x, y)) {
                queue(MouseMoveEvent{
                    .x           = point->x,
                    .y           = point->y,
                    .screenIndex = point->screenIndex,
                    .deltaX      = static_cast<int32_t>(std::lround(dx)),
                    .deltaY      = static_cast<int32_t>(std::lround(dy)),
                });
            }
            break;
        }
        case EI_EVENT_POINTER_MOTION_ABSOLUTE: {
            const auto x = ei_event_pointer_get_absolute_x(event);
            const auto y = ei_event_pointer_get_absolute_y(event);
            {
                auto lock       = std::scoped_lock{mState->mutex};
                mState->globalX = x;
                mState->globalY = y;
            }
            if (const auto point = mPlatform->globalToLocal(x, y)) {
                queue(MouseMoveEvent{
                    .x = point->x, .y = point->y, .screenIndex = point->screenIndex});
            }
            break;
        }
        case EI_EVENT_BUTTON_BUTTON: {
            const auto button = pointerButton(ei_event_button_get_button(event));
            const auto point  = pointerPoint();
            if (button != MouseButton::None && point) {
                queue(MouseButtonEvent{
                    .x           = point->x,
                    .y           = point->y,
                    .screenIndex = point->screenIndex,
                    .button      = button,
                    .release     = !ei_event_button_get_is_press(event),
                });
            }
            break;
        }
        case EI_EVENT_SCROLL_DISCRETE: {
            const auto point = pointerPoint();
            if (point) {
                queue(MouseWheelEvent{
                    .x      = point->x,
                    .y      = point->y,
                    .deltaX = ei_event_scroll_get_discrete_dx(event),
                    .deltaY = -ei_event_scroll_get_discrete_dy(event),
                });
            }
            break;
        }
        case EI_EVENT_SCROLL_DELTA: {
            const auto point = pointerPoint();
            if (point) {
                queue(MouseWheelEvent{
                    .x = point->x,
                    .y = point->y,
                    .deltaX =
                        static_cast<int32_t>(std::lround(ei_event_scroll_get_dx(event) * 12.0)),
                    .deltaY =
                        static_cast<int32_t>(std::lround(-ei_event_scroll_get_dy(event) * 12.0)),
                });
            }
            break;
        }
        case EI_EVENT_KEYBOARD_KEY: {
            const auto native  = ei_event_keyboard_get_key(event);
            const auto key     = wayland::keyFromEvdev(native);
            const auto release = !ei_event_keyboard_get_key_is_press(event);
            if (key == Key::None) {
                SPDLOG_DEBUG("Ignoring unmapped libei key code {}", native);
                break;
            }
            auto modifiers = KeyModifier::None;
            {
                auto lock = std::scoped_lock{mState->mutex};
                if (const auto modifier = keyModifierFor(key); modifier != KeyModifier::None) {
                    if (release) {
                        mState->modifiers &= ~modifier;
                    }
                    else {
                        mState->modifiers |= modifier;
                    }
                }
                modifiers = mState->modifiers;
            }
            queue(KeyEvent{
                .key        = key,
                .modifiers  = modifiers,
                .nativeCode = native,
                .repeat     = false,
                .release    = release,
            });
            break;
        }
        case EI_EVENT_DISCONNECT: {
            auto lock      = std::scoped_lock{mState->mutex};
            mState->closed = true;
            break;
        }
        default:
            break;
        }
    }

    std::shared_ptr<PortalPlatform>              mPlatform;
    std::shared_ptr<CaptureState>                mState;
    XdpInputCaptureSession                      *mSession     = nullptr;
    GCancellable                                *mCancellable = nullptr;
    ei                                          *mEi          = nullptr;
    ilias::Poller                                mPoller;
    std::vector<gulong>                          mSignalIds;
    std::vector<XdpInputCapturePointerBarrier *> mBarriers;
};

class PortalInputInjector final : public InputInjector {
public:
    explicit PortalInputInjector(std::shared_ptr<PortalPlatform> platform)
        : mPlatform(std::move(platform))
    {
    }

    ~PortalInputInjector() override { close(); }

    auto initialize() -> IoTask<void> override
    {
        close();
        mCancellable = g_cancellable_new();
        if (!mCancellable) {
            co_return Err(makeIoError(std::errc::not_enough_memory));
        }

        auto created = co_await mPlatform->runtime()->createRemoteDesktop(mCancellable);
        if (!created) {
            close();
            co_return Err(created.error());
        }
        mSession     = *created;
        auto started = co_await mPlatform->runtime()->startRemoteDesktop(mSession, mCancellable);
        if (!started) {
            close();
            co_return Err(started.error());
        }

        const auto granted = xdp_session_get_devices(mSession);
        if ((granted & XDP_DEVICE_POINTER) == 0 || (granted & XDP_DEVICE_KEYBOARD) == 0) {
            SPDLOG_WARN("RemoteDesktop portal did not grant pointer and keyboard control");
            close();
            co_return Err(makeIoError(std::errc::permission_denied));
        }

        auto connected = co_await mPlatform->runtime()->connectToEis(mSession);
        if (!connected) {
            close();
            co_return Err(connected.error());
        }
        mEi = ei_new_sender(nullptr);
        if (!mEi) {
            ::close(*connected);
            close();
            co_return Err(makeIoError(std::errc::not_enough_memory));
        }
        ei_configure_name(mEi, "mksync Wayland input injection");
        const auto setup = ei_setup_backend_fd(mEi, *connected);
        if (setup < 0) {
            close();
            co_return Err(std::error_code{-setup, std::generic_category()});
        }
        auto poller = co_await ilias::Poller::make(ei_get_fd(mEi), ilias::IoDescriptor::Socket);
        if (!poller) {
            close();
            co_return Err(poller.error());
        }
        mPoller = std::move(*poller);

        while (!ready()) {
            dispatchEi();
            if (mDisconnected) {
                close();
                co_return Err(makeIoError(std::errc::connection_refused));
            }
            if (ready()) {
                break;
            }
            ILIAS_CO_TRY(auto events, co_await mPoller.poll(POLLIN));
            if ((events & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                close();
                co_return Err(makeIoError(std::errc::connection_reset));
            }
        }

        updateScreensFromRegions();
        SPDLOG_INFO("Wayland portal injection started outputs={} devices={}",
                    mPlatform->screens().size(), mDevices.size());
        co_return {};
    }

    auto shutdown() -> Task<void> override
    {
        close();
        co_return;
    }

    auto inject(const InputEvent &event) -> IoTask<void> override
    {
        if (!mEi || !ready()) {
            co_return Err(makeIoError(std::errc::not_connected));
        }
        dispatchEi();
        if (mDisconnected) {
            co_return Err(makeIoError(std::errc::connection_reset));
        }

        auto error = std::error_code{};
        std::visit([&](const auto &value) { error = injectOne(value); }, event);
        if (error) {
            co_return Err(error);
        }
        ei_dispatch(mEi);
        co_return {};
    }

private:
    struct Device {
        ei_device *object    = nullptr;
        bool       resumed   = false;
        bool       emulating = false;
    };

    auto close() -> void
    {
        if (mCancellable) {
            g_cancellable_cancel(mCancellable);
        }
        if (mPoller) {
            auto canceled = mPoller.cancel();
            (void)canceled;
            mPoller.close();
        }
        for (auto &device : mDevices) {
            if (device.emulating) {
                ei_device_stop_emulating(device.object);
            }
            ei_device_unref(device.object);
        }
        mDevices.clear();
        if (mEi) {
            ei_unref(mEi);
            mEi = nullptr;
        }
        if (mSession) {
            xdp_session_close(mSession);
        }
        g_clear_object(&mSession);
        g_clear_object(&mCancellable);
        mDisconnected = false;
    }

    auto device(enum ei_device_capability capability) const -> ei_device *
    {
        const auto it = std::ranges::find_if(mDevices, [&](const Device &candidate) {
            return candidate.resumed && ei_device_has_capability(candidate.object, capability);
        });
        return it == mDevices.end() ? nullptr : it->object;
    }

    auto ready() const -> bool
    {
        return device(EI_DEVICE_CAP_POINTER_ABSOLUTE) && device(EI_DEVICE_CAP_BUTTON) &&
               device(EI_DEVICE_CAP_SCROLL) && device(EI_DEVICE_CAP_KEYBOARD);
    }

    auto dispatchEi() -> void
    {
        if (!mEi) {
            return;
        }
        ei_dispatch(mEi);
        while (auto *event = ei_get_event(mEi)) {
            processEiEvent(event);
            ei_event_unref(event);
        }
    }

    auto processEiEvent(ei_event *event) -> void
    {
        auto *object = ei_event_get_device(event);
        switch (ei_event_get_type(event)) {
        case EI_EVENT_SEAT_ADDED:
            ei_seat_bind_capabilities(ei_event_get_seat(event), EI_DEVICE_CAP_POINTER,
                                      EI_DEVICE_CAP_POINTER_ABSOLUTE, EI_DEVICE_CAP_BUTTON,
                                      EI_DEVICE_CAP_SCROLL, EI_DEVICE_CAP_KEYBOARD, nullptr);
            break;
        case EI_EVENT_DEVICE_ADDED:
            mDevices.push_back(Device{.object = ei_device_ref(object)});
            break;
        case EI_EVENT_DEVICE_RESUMED:
            if (auto it = findDevice(object); it != mDevices.end()) {
                it->resumed = true;
                if (!it->emulating) {
                    ei_device_start_emulating(it->object, mSequence++);
                    it->emulating = true;
                }
            }
            break;
        case EI_EVENT_DEVICE_PAUSED:
            if (auto it = findDevice(object); it != mDevices.end()) {
                it->resumed   = false;
                it->emulating = false;
            }
            break;
        case EI_EVENT_DEVICE_REMOVED:
            if (auto it = findDevice(object); it != mDevices.end()) {
                ei_device_unref(it->object);
                mDevices.erase(it);
            }
            break;
        case EI_EVENT_DISCONNECT:
            mDisconnected = true;
            break;
        default:
            break;
        }
    }

    auto findDevice(ei_device *object) -> std::vector<Device>::iterator
    {
        return std::ranges::find(mDevices, object, &Device::object);
    }

    auto updateScreensFromRegions() -> void
    {
        auto screens = std::vector<ScreenInfo>{};
        auto seen    = std::vector<std::array<uint32_t, 4>>{};
        for (const auto &deviceInfo : mDevices) {
            if (!ei_device_has_capability(deviceInfo.object, EI_DEVICE_CAP_POINTER_ABSOLUTE)) {
                continue;
            }
            for (auto index = size_t{0};; ++index) {
                auto *region = ei_device_get_region(deviceInfo.object, index);
                if (!region) {
                    break;
                }
                const auto rect =
                    std::array{ei_region_get_x(region), ei_region_get_y(region),
                               ei_region_get_width(region), ei_region_get_height(region)};
                if (rect[2] == 0 || rect[3] == 0 || std::ranges::find(seen, rect) != seen.end()) {
                    continue;
                }
                seen.push_back(rect);
                const auto screenIndex = screens.size();
                screens.push_back(ScreenInfo{
                    .x      = static_cast<int32_t>(rect[0]),
                    .y      = static_cast<int32_t>(rect[1]),
                    .width  = static_cast<int32_t>(rect[2]),
                    .height = static_cast<int32_t>(rect[3]),
                    .dpi    = static_cast<int32_t>(
                        std::lround(72.0 * ei_region_get_physical_scale(region))),
                    .name    = fmtlib::format("portal-region-{}", screenIndex),
                    .primary = screenIndex == 0,
                });
            }
        }
        mPlatform->updateScreens(std::move(screens));
    }

    auto movePointer(uint32_t screenIndex, int32_t x, int32_t y) -> std::error_code
    {
        auto      *pointer  = device(EI_DEVICE_CAP_POINTER_ABSOLUTE);
        const auto position = mPlatform->localToGlobal(screenIndex, x, y);
        if (!pointer || !position) {
            return makeIoError(std::errc::invalid_argument);
        }
        ei_device_pointer_motion_absolute(pointer, position->first, position->second);
        ei_device_frame(pointer, ei_now(mEi));
        return {};
    }

    auto injectOne(const MouseMoveEvent &event) -> std::error_code
    {
        return movePointer(event.screenIndex, event.x, event.y);
    }

    auto injectOne(const MouseButtonEvent &event) -> std::error_code
    {
        if (auto error = movePointer(event.screenIndex, event.x, event.y); error) {
            return error;
        }
        auto      *buttonDevice = device(EI_DEVICE_CAP_BUTTON);
        const auto button       = evdevButton(event.button);
        if (!buttonDevice || !button) {
            return makeIoError(std::errc::invalid_argument);
        }
        ei_device_button_button(buttonDevice, *button, !event.release);
        ei_device_frame(buttonDevice, ei_now(mEi));
        return {};
    }

    auto injectOne(const MouseWheelEvent &event) -> std::error_code
    {
        auto *scroll = device(EI_DEVICE_CAP_SCROLL);
        if (!scroll) {
            return makeIoError(std::errc::operation_not_supported);
        }
        ei_device_scroll_discrete(scroll, event.deltaX, -event.deltaY);
        ei_device_frame(scroll, ei_now(mEi));
        return {};
    }

    auto injectOne(const KeyEvent &event) -> std::error_code
    {
        auto      *keyboard = device(EI_DEVICE_CAP_KEYBOARD);
        const auto native   = wayland::evdevKeyCode(event.key);
        if (!keyboard || !native) {
            return makeIoError(std::errc::invalid_argument);
        }
        ei_device_keyboard_key(keyboard, *native, !event.release);
        ei_device_frame(keyboard, ei_now(mEi));
        return {};
    }

    std::shared_ptr<PortalPlatform> mPlatform;
    XdpSession                     *mSession     = nullptr;
    GCancellable                   *mCancellable = nullptr;
    ei                             *mEi          = nullptr;
    ilias::Poller                   mPoller;
    std::vector<Device>             mDevices;
    uint32_t                        mSequence     = 1;
    bool                            mDisconnected = false;
};

auto PortalPlatform::createCapture() -> InputCapture::Ptr
{
    if (!mCapture.expired()) {
        throw std::runtime_error("InputCapture already created");
    }
    auto capture = std::make_shared<PortalInputCapture>(shared_from_this());
    mCapture     = capture;
    return capture;
}

auto PortalPlatform::createInjector() -> InputInjector::Ptr
{
    if (!mInjector.expired()) {
        throw std::runtime_error("InputInjector already created");
    }
    auto injector = std::make_shared<PortalInputInjector>(shared_from_this());
    mInjector     = injector;
    return injector;
}

namespace
{
    struct PortalInterfaceInfo {
        bool        available    = false;
        uint32_t    version      = 0;
        uint32_t    capabilities = 0;
        std::string error;
    };

    auto portalInterface(std::string_view interfaceName, std::string_view capabilityProperty)
        -> PortalInterfaceInfo
    {
        auto *error = static_cast<GError *>(nullptr);
        auto *proxy = g_dbus_proxy_new_for_bus_sync(
            G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, nullptr, "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop", std::string{interfaceName}.c_str(), nullptr, &error);
        if (!proxy || !g_dbus_proxy_get_name_owner(proxy)) {
            auto result = PortalInterfaceInfo{
                .error = error ? error->message : "Portal interface has no D-Bus owner"};
            g_clear_error(&error);
            g_clear_object(&proxy);
            return result;
        }

        auto result =
            PortalInterfaceInfo{.available = true, .version = 0, .capabilities = 0, .error = {}};
        if (auto *version = g_dbus_proxy_get_cached_property(proxy, "version")) {
            result.version = g_variant_get_uint32(version);
            g_variant_unref(version);
        }
        if (auto *capabilities =
                g_dbus_proxy_get_cached_property(proxy, std::string{capabilityProperty}.c_str())) {
            result.capabilities = g_variant_get_uint32(capabilities);
            g_variant_unref(capabilities);
        }
        g_object_unref(proxy);
        return result;
    }

    auto checkPortalBackend() -> Task<BackendCheck>
    {
        const auto *waylandDisplay = std::getenv("WAYLAND_DISPLAY");
        const auto *sessionType    = std::getenv("XDG_SESSION_TYPE");
        if ((!waylandDisplay || !*waylandDisplay) &&
            (!sessionType || std::string_view{sessionType} != "wayland")) {
            co_return BackendCheck{
                .available = false,
                .detail    = "Not running in a Wayland session",
                .screens   = {.supported = false, .detail = "Wayland session unavailable"},
                .capture   = {.supported = false, .detail = "Wayland session unavailable"},
                .injection = {.supported = false, .detail = "Wayland session unavailable"},
            };
        }

        const auto capture =
            portalInterface("org.freedesktop.portal.InputCapture", "SupportedCapabilities");
        const auto remote =
            portalInterface("org.freedesktop.portal.RemoteDesktop", "AvailableDeviceTypes");
        const auto captureMask = XDP_INPUT_CAPABILITY_POINTER | XDP_INPUT_CAPABILITY_KEYBOARD;
        const auto remoteMask  = XDP_DEVICE_POINTER | XDP_DEVICE_KEYBOARD;
        const auto canCapture =
            capture.available && (capture.capabilities & captureMask) == captureMask;
        // RemoteDesktop.ConnectToEIS was added in portal interface version 2.
        const auto canInject = remote.available && remote.version >= 2 &&
                               (remote.capabilities & remoteMask) == remoteMask;
        const auto available = capture.available || remote.available;

        co_return BackendCheck{
            .available = available,
            .detail    = available ? fmtlib::format("Portal interfaces detected (InputCapture v{}, "
                                                       "RemoteDesktop v{}); authorization is requested "
                                                       "only when the backend starts",
                                                    capture.version, remote.version)
                                   : fmtlib::format("Desktop portal unavailable: {}; {}",
                                                    capture.error, remote.error),
            .screens =
                {
                          .supported = canCapture || canInject,
                          .detail = canCapture ? "Outputs are provided by authorized InputCapture zones"
                                         : (canInject ? "Outputs are provided by authorized libei "
                                                        "regions"
                                                      : "No portal topology source is available"),
                          },
            .capture =
                {
                          .supported = canCapture,
                          .detail    = canCapture
                                     ? fmtlib::format("InputCapture v{} supports pointer and keyboard",
                          capture.version)
                                     : (capture.error.empty() ? "InputCapture lacks "
                                                                "pointer/keyboard capabilities"
                                                              : capture.error),
                          },
            .injection =
                {
                          .supported = canInject,
                          .detail =
                        canInject
                            ? fmtlib::format("RemoteDesktop v{} supports libei pointer and "
                                             "keyboard injection", remote.version)
                            : (remote.error.empty() ? "RemoteDesktop v2/libei pointer and keyboard "
                                                      "are required"
                                                    : remote.error),
                          },
        };
    }

    auto createPortalPlatform() -> Platform::Ptr
    {
        try {
            return std::make_shared<PortalPlatform>();
        }
        catch (const std::exception &error) {
            SPDLOG_ERROR("Failed to create Wayland portal backend: {}", error.what());
            return nullptr;
        }
    }

    const BackendRegistration kPortalBackendRegistration{
        BackendDescriptor{
                          .name        = "wayland-portal",
                          .displayName = "Wayland (XDG Portal/libei)",
                          .order       = 50,
                          .check       = checkPortalBackend,
                          .create      = createPortalPlatform,
                          }
    };
} // namespace

MKS_END

#endif
