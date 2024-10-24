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

private:
    std::unique_ptr<GMainLoop, GMainLoopDeleter> _gloop;
    std::unique_ptr<GMainLoop, GMainLoopQuiter>  _gloopRun;
    std::unique_ptr<XdpPortal, GObjectDeleter>   _portal;
};

int main()
{
    spdlog::set_pattern("[%Y-%m-%d %T.%f] [%n] [%P] [%t] %^[%l]%$ [%s:%#]\n%v");

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

    std::this_thread::sleep_for(std::chrono::seconds(5));
    // stop so_5
    sobj.stop_then_join();
    return 0;
}

void TestAgent::so_define_agent()
{
    SPDLOG_INFO("{}", __FUNCTION__);
    so_subscribe_self().event(&TestAgent::so_dispatch);
}

void TestAgent::so_evt_start()
{
    SPDLOG_INFO("{}", __FUNCTION__);
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
            SPDLOG_INFO("here");
            reinterpret_cast<TestAgent *>(data)->cb_capture_init(obj, res);
        },
        this);

    so_5::send<DispatchSignal>(so_direct_mbox());
}

void TestAgent::so_evt_finish()
{
    SPDLOG_INFO("{}", __FUNCTION__);
}

void TestAgent::so_dispatch(const so_5::mhood_t<DispatchSignal> & /*signal*/)
{
    GMainContext *context = g_main_loop_get_context(_gloop.get());
    if (g_main_loop_is_running(_gloop.get()) != 0) {
        SPDLOG_INFO("dispatch message");
        g_main_context_iteration(context, TRUE);
        return so_5::send<DispatchSignal>(so_direct_mbox());
    }
    SPDLOG_INFO("stop glib thread loop");
}

void TestAgent::cb_capture_init(GObject *gobj, GAsyncResult *gres)
{
    SPDLOG_INFO("on capture init");

    std::unique_ptr<GError, GErrorDeleter>                  gerrc         = {};
    std::unique_ptr<XdpInputCaptureSession, GObjectDeleter> xdpCapSession = {};

    SPDLOG_INFO("gobj: {}\tgres: {}", (void *)gobj, (void *)gres);
    xdpCapSession.reset(xdp_portal_create_input_capture_session_finish(XDP_PORTAL(gobj), gres,
                                                                       std::out_ptr(gerrc)));
    if (!xdpCapSession) {
        SPDLOG_ERROR("Failed to initialize InputCapture session, quitting: {}", gerrc->message);
        return g_main_loop_quit(_gloop.get());
    }
    SPDLOG_INFO("get capture session success");
}