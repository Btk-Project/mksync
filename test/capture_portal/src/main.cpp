#include "mksync/base/osdep.h"

#include <iostream>
#include <codecvt>
#include <locale>
#include <thread>
#include <memory>
#if __cplusplus > 202300L
#else
#include <ztd/out_ptr.hpp>
namespace std
{
    using namespace ztd::out_ptr;
}
#endif

#define SPDLOG_ACTIVE_LEVEL 0
#include <so_5/all.hpp>
#include <spdlog/spdlog.h>
#include <libportal/portal.h>
#include <libportal/inputcapture.h>

#include "mksync/proto/proto.hpp"

struct GObjectDeleter {
    void operator()(void *obj)
    {
        if (obj != nullptr) {
            g_object_unref(obj);
        }
    }
};

struct GListDeleter {
    void operator()(GList *list)
    {
        if (list != nullptr) {
            g_list_free(list);
        }
    }
};

struct GMainLoopQuiter {
    void operator()(GMainLoop *loop)
    {
        if (loop != nullptr && g_main_loop_is_running(loop) == TRUE) {
            g_main_loop_quit(loop);
        }
    }
};

struct GMainLoopDeleter {
    void operator()(GMainLoop *loop)
    {
        if (loop != nullptr) {
            g_main_loop_unref(loop);
        }
    }
};

struct GErrorDeleter {
    void operator()(GError *errc)
    {
        if (errc != nullptr) {
            g_error_free(errc);
        }
    }
};

class TestAgent : public so_5::agent_t {
public:
    TestAgent(so_5::agent_context_t ctx) : so_5::agent_t(ctx) {}
    ~TestAgent() = default;

    void so_define_agent() override;
    void so_evt_start() override;
    void so_evt_finish() override;

public:
    struct DispatchSignal : so_5::signal_t {};

public:
    void so_dispatch(const so_5::mhood_t<DispatchSignal> &signal);

    void cb_capture_init(GObject *gobj, GAsyncResult *gres);
    void cb_set_pointer_barriers(GObject *gobj, GAsyncResult *gres);

    void cb_disabled(XdpInputCaptureSession *session);
    void cb_activated(XdpInputCaptureSession *session, uint32_t activationId, GVariant *options);
    void cb_deactivated(XdpInputCaptureSession *session, uint32_t activationId, GVariant *options);
    void cb_zones_changed(XdpInputCaptureSession *session, GVariant *options);
    void cb_session_closed(XdpSession *session);

private:
    std::unique_ptr<GMainLoop, GMainLoopDeleter>            _gloop;
    std::unique_ptr<GMainLoop, GMainLoopQuiter>             _gloopRun;
    std::unique_ptr<XdpPortal, GObjectDeleter>              _portal;
    std::unique_ptr<XdpInputCaptureSession, GObjectDeleter> _capSession;

    std::vector<guint> _signals;

    std::vector<std::unique_ptr<XdpInputCapturePointerBarrier, GObjectDeleter>> _capBarriers;
};

int main()
{
    spdlog::set_pattern("[%Y-%m-%d %T.%f] [%n] [%P] [%t] %^[%l]%$ [%s:%#]\n%v");
    spdlog::set_level(spdlog::level::trace);

    // so_5
    so_5::wrapped_env_t  sobj = {};
    so_5::environment_t &env  = sobj.environment();
    // work group and dispatcher
    auto coop = env.make_coop(); // main work group
    auto activeObjDisp =
        so_5::disp::active_obj::make_dispatcher(env); // dispatcher (each agent in one thread)
    // make work groups
    (void)coop->make_agent_with_binder<TestAgent>(activeObjDisp.binder());
    // register and start work groups
    env.register_coop(std::move(coop));

    std::this_thread::sleep_for(std::chrono::seconds(10));
    // stop so_5
    sobj.stop_then_join();
    return 0;
}

void TestAgent::so_define_agent()
{
    SPDLOG_TRACE("{}", __FUNCTION__);
    so_subscribe_self().event(&TestAgent::so_dispatch);
}

void TestAgent::so_evt_start()
{
    SPDLOG_TRACE("{}", __FUNCTION__);
    std::unique_ptr<GError, GErrorDeleter> gerrc = {};

    _gloop.reset(g_main_loop_new(nullptr, TRUE));
    _gloopRun.reset(_gloop.get());
    _portal.reset(xdp_portal_initable_new(std::out_ptr(gerrc)));
    if (gerrc) {
        return SPDLOG_ERROR("xdp_portal_initable_new error, info: {}", gerrc->message);
    }

    // Add a timer that returns to the sobjectizer dispatcher from the glib event loop
    g_timeout_add_seconds(
        1, []([[maybe_unused]] gpointer userData) -> gboolean { return TRUE; }, NULL);
    // Add input capture task
    xdp_portal_create_input_capture_session(
        _portal.get(),
        nullptr, // parent
        static_cast<XdpInputCapability>(XDP_INPUT_CAPABILITY_KEYBOARD |
                                        XDP_INPUT_CAPABILITY_POINTER),
        nullptr, // cancellable
        [](GObject *obj, GAsyncResult *res, gpointer data) {
            reinterpret_cast<TestAgent *>(data)->cb_capture_init(obj, res);
        },
        this);

    so_5::send<DispatchSignal>(so_direct_mbox());
}

void TestAgent::so_evt_finish()
{
    SPDLOG_TRACE("{}", __FUNCTION__);
}

void TestAgent::so_dispatch(const so_5::mhood_t<DispatchSignal> & /*signal*/)
{
    GMainContext *context = g_main_loop_get_context(_gloop.get());
    if (g_main_loop_is_running(_gloop.get()) != 0) {
        SPDLOG_DEBUG("dispatch message");
        g_main_context_iteration(context, TRUE);
        return so_5::send<DispatchSignal>(so_direct_mbox());
    }
    SPDLOG_INFO("stop glib thread loop");
}

void TestAgent::cb_capture_init(GObject *gobj, GAsyncResult *gres)
{
    SPDLOG_DEBUG("cb_capture_init");

    std::unique_ptr<GError, GErrorDeleter> gerrc = {};

    SPDLOG_DEBUG("gobj: {}\tgres: {}", (void *)gobj, (void *)gres);
    _capSession.reset(xdp_portal_create_input_capture_session_finish(XDP_PORTAL(gobj), gres,
                                                                     std::out_ptr(gerrc)));
    if (!_capSession) {
        SPDLOG_ERROR("Failed to initialize InputCapture session, quitting: {}", gerrc->message);
        return g_main_loop_quit(_gloop.get());
    }
    SPDLOG_INFO("get input capture session success");

    auto cbDisabled = [](XdpInputCaptureSession *session, gpointer data) -> void {
        reinterpret_cast<TestAgent *>(data)->cb_disabled(session);
    };
    auto cbActivated = [](XdpInputCaptureSession *session, uint32_t activationId, GVariant *options,
                          gpointer data) -> void {
        reinterpret_cast<TestAgent *>(data)->cb_activated(session, activationId, options);
    };
    auto cbDeactivated = [](XdpInputCaptureSession *session, uint32_t activationId,
                            GVariant *options, gpointer data) -> void {
        reinterpret_cast<TestAgent *>(data)->cb_deactivated(session, activationId, options);
    };
    auto cbZonesChanged = [](XdpInputCaptureSession *session, GVariant *options,
                             gpointer data) -> void {
        reinterpret_cast<TestAgent *>(data)->cb_zones_changed(session, options);
    };
    auto cbSesClosed = [](XdpSession *session, gpointer data) -> void {
        reinterpret_cast<TestAgent *>(data)->cb_session_closed(session);
    };
    typedef void (*PFN_DISABLED)(XdpInputCaptureSession *session, gpointer data);
    typedef void (*PFN_ACTIVATED)(XdpInputCaptureSession *session, uint32_t activationId,
                                  GVariant *options, gpointer data);
    typedef void (*PFN_DEACTIVATED)(XdpInputCaptureSession *session, uint32_t activationId,
                                    GVariant *options, gpointer data);
    typedef void (*PFN_ZONES_CHANGED)(XdpInputCaptureSession *session, GVariant *options,
                                      gpointer data);
    typedef void (*PFN_SESSION_CLOSED)(XdpSession *session, gpointer data);

    _signals.push_back(g_signal_connect(G_OBJECT(_capSession.get()), "disabled",
                                        G_CALLBACK(static_cast<PFN_DISABLED>(cbDisabled)), this));
    _signals.push_back(g_signal_connect(G_OBJECT(_capSession.get()), "activated",
                                        G_CALLBACK(static_cast<PFN_ACTIVATED>(cbActivated)), this));
    _signals.push_back(g_signal_connect(G_OBJECT(_capSession.get()), "deactivated",
                                        G_CALLBACK(static_cast<PFN_DEACTIVATED>(cbDeactivated)),
                                        this));
    _signals.push_back(g_signal_connect(G_OBJECT(_capSession.get()), "zones-changed",
                                        G_CALLBACK(static_cast<PFN_ZONES_CHANGED>(cbZonesChanged)),
                                        this));

    XdpSession *session = xdp_input_capture_session_get_session(_capSession.get());
    _signals.push_back(g_signal_connect(G_OBJECT(session), "closed",
                                        G_CALLBACK(static_cast<PFN_SESSION_CLOSED>(cbSesClosed)),
                                        this));

    //
    cb_zones_changed(_capSession.get(), nullptr);
}

void TestAgent::cb_set_pointer_barriers([[maybe_unused]] GObject *gobj, GAsyncResult *gres)
{
    SPDLOG_DEBUG("cb_set_pointer_barriers");

    std::unique_ptr<GError, GErrorDeleter> gerrc = {};

    std::unique_ptr<GList, GListDeleter> failedList(
        xdp_input_capture_session_set_pointer_barriers_finish(_capSession.get(), gres,
                                                              std::out_ptr(gerrc)));
    if (gerrc) {
        return SPDLOG_ERROR("xdp_input_capture_session_set_pointer_barriers_finish error, info: {}",
                            gerrc->message);
    }

    for (GList *it = failedList.get(); it != nullptr; it = it->next) {
        gint id;
        g_object_get(it->data, "id", &id, nullptr);
        SPDLOG_DEBUG("Failed to apply barrier {}", id);
    }

    xdp_input_capture_session_enable(_capSession.get());
}

void TestAgent::cb_disabled(XdpInputCaptureSession *session)
{
    SPDLOG_TRACE("{}", __FUNCTION__);
}

void TestAgent::cb_activated(XdpInputCaptureSession *session, uint32_t activationId,
                             GVariant *options)
{
    SPDLOG_TRACE("{}", __FUNCTION__);
}

void TestAgent::cb_deactivated(XdpInputCaptureSession *session, uint32_t activationId,
                               GVariant *options)
{
    SPDLOG_TRACE("{}", __FUNCTION__);
}

void TestAgent::cb_zones_changed(XdpInputCaptureSession    *session,
                                 [[maybe_unused]] GVariant *options)
{
    GList *zones = xdp_input_capture_session_get_zones(session);
    for (; zones != nullptr; zones = zones->next) {
        guint width, height;
        gint  posx, posy;
        g_object_get(zones->data, "width", &width, "height", &height, "x", &posx, "y", &posy,
                     nullptr);

        gint id = _capBarriers.size() + 1, x1 = posx, y1 = posy, x2 = posx + width - 1, y2 = posy;
        _capBarriers.push_back(std::unique_ptr<XdpInputCapturePointerBarrier, GObjectDeleter>(
            XDP_INPUT_CAPTURE_POINTER_BARRIER(g_object_new(XDP_TYPE_INPUT_CAPTURE_POINTER_BARRIER,
                                                           "id", id, "x1", x1, "y1", y1, "x2", x2,
                                                           "y2", y2, nullptr))));

        id = _capBarriers.size() + 1, x1 = posx + width, y1 = posy, x2 = posx + width,
        y2 = posy + height - 1;
        _capBarriers.push_back(std::unique_ptr<XdpInputCapturePointerBarrier, GObjectDeleter>(
            XDP_INPUT_CAPTURE_POINTER_BARRIER(g_object_new(XDP_TYPE_INPUT_CAPTURE_POINTER_BARRIER,
                                                           "id", id, "x1", x1, "y1", y1, "x2", x2,
                                                           "y2", y2, nullptr))));
        SPDLOG_INFO("Zone at {}x{}@{},{}", width, height, posx, posy);
    }

    GList *list = nullptr;
    for (auto const &barrier : _capBarriers) {
        list = g_list_append(list, barrier.get());
    }

    xdp_input_capture_session_set_pointer_barriers(
        session, list,
        nullptr, // cancellable
        [](GObject *obj, GAsyncResult *res, gpointer data) {
            reinterpret_cast<TestAgent *>(data)->cb_set_pointer_barriers(obj, res);
        },
        this);
}

void TestAgent::cb_session_closed(XdpSession *session)
{
    SPDLOG_TRACE("{}", __FUNCTION__);
}