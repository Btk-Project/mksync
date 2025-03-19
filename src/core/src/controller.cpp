#include "mksync/core/controller.hpp"

#include "mksync/core/app.hpp"
#include "mksync/core/client_controller.hpp"
#include "mksync/core/server_controller.hpp"

namespace mks::base
{
    using ::ilias::Error;
    using ::ilias::Task;
    using ::ilias::TaskScope;
    using ::ilias::Unexpected;

    Controller::Controller(IApp *app) : _app(app), _events(10) {}
    Controller::~Controller()
    {
        _app->command_uninstaller(this);
    }

    auto Controller::setup() -> ::ilias::Task<int>
    {
        SPDLOG_INFO("node {}<{}> setup", name(), (void *)this);
        co_return 0;
    }

    ///> 停用节点。
    auto Controller::teardown() -> ::ilias::Task<int>
    {
        _imp.reset();
        _syncEvent.set();
        SPDLOG_INFO("node {}<{}> teardown", name(), (void *)this);
        co_return 0;
    }

    ///> 获取节点名称。
    auto Controller::name() -> const char *
    {
        return "Controller";
    }

    ///> 预先订阅的事件类型集合。
    auto Controller::get_subscribes() -> std::vector<int>
    {
        return {NekoProto::ProtoFactory::protoType<AppStatusChanged>()};
    }

    ///> 处理一个事件，需要订阅。
    auto Controller::handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void>
    {
        if (event.type() == NekoProto::ProtoFactory::protoType<AppStatusChanged>()) {
            co_await handle_event(*event.cast<AppStatusChanged>());
        }
        if (_imp != nullptr) {
            co_return co_await _imp->handle_event(event);
        }
        co_return;
    }

    // App模式状态改变
    auto Controller::handle_event(const AppStatusChanged &event) -> ::ilias::Task<void>
    {
        SPDLOG_INFO("app status changed : {}", (int)event.status);
        if (event.status == AppStatusChanged::eStarted && event.mode == AppStatusChanged::eServer) {
            _imp = std::make_unique<ServerController>(this, _app);
            co_await _imp->setup();
        }
        else if (event.status == AppStatusChanged::eStarted &&
                 event.mode == AppStatusChanged::eClient) {
            _imp = std::make_unique<ClientController>(this, _app);
            co_await _imp->setup();
        }
        else {
            co_await _imp->teardown();
            _imp.reset();
        }
        co_return;
    }

    auto Controller::get_event() -> ::ilias::IoTask<NekoProto::IProto>
    {
        if (_events.size() == 0) {
            _syncEvent.clear();
            auto ret = co_await _syncEvent;
            if (!ret) {
                co_return Unexpected<Error>(ret.error());
            }
        }
        NekoProto::IProto proto;
        if (_events.pop(proto)) {
            co_return proto;
        }
        else {
            co_return Unexpected<Error>(Error::Unknown);
        }
    }

    auto Controller::pust_event(NekoProto::IProto &&event) -> void
    {
        _events.emplace(std::forward<NekoProto::IProto>(event));
        _syncEvent.set();
    }

    auto Controller::make(App &app) -> std::unique_ptr<Controller, void (*)(NodeBase *)>
    {
        return std::unique_ptr<Controller, void (*)(NodeBase *)>(new Controller(&app),
                                                                 [](NodeBase *ptr) { delete ptr; });
    }
} // namespace mks::base