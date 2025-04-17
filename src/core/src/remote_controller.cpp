#include "mksync/core/remote_controller.hpp"

#include <ilias/net/tcp.hpp>
#include <ilias/net/udp.hpp>
#include <ilias/fs/pipe.hpp>
#include <ilias/sync/scope.hpp>

#include "mksync/base/app.hpp"
#include "mksync/base/default_configs.hpp"
#include "mksync/proto/command_proto.hpp"

MKS_BEGIN
using ::ilias::Error;
using ::ilias::Event;
using ::ilias::IoTask;
using ::ilias::IPEndpoint;
using ::ilias::Task;

RemoteController::RemoteController(IApp *app)
    : _app(app), _rpcServer(*app->get_io_context()), _taskScop(*app->get_io_context())
{
    _taskScop.setAutoCancel(true);
}

RemoteController::~RemoteController() {}

auto RemoteController::setup() -> Task<int>
{
    SPDLOG_INFO("RemoteController setup");
    if (auto ret = co_await _rpcServer.start(
            _app->settings().get(remote_controller_config_name, remote_controller_default_value));
        !ret) {
        co_return -1;
    }
    _rpcServer->executeCommand = [this](std::string cmd) -> IoTask<std::string> {
        co_return co_await _app->command_invoker().execute(cmd);
    };
    _rpcServer->reloadConfigFile = std::function([this](std::string file) -> IoTask<void> {
        if (_app->settings().load(file)) {
            co_await _app->reconfigure();
        }
        co_return {};
    });
    _rpcServer->server           = [this](ServerControl::Command cmd, std::string ip,
                                uint16_t port) -> IoTask<std::string> {
        co_return co_await _app->command_invoker().execute(
            ServerControl::emplaceProto(cmd, ip, port));
    };
    _rpcServer->client = [this](ClientControl::Command cmd, std::string ip,
                                uint16_t port) -> IoTask<std::string> {
        co_return co_await _app->command_invoker().execute(
            ClientControl::emplaceProto(cmd, ip, port));
    };
    _rpcServer->localScreenInfo = [this]() { return _app->get_screen_info(); };

    co_return 0;
}

auto RemoteController::reconfigure([[maybe_unused]] Settings &settings) -> ::ilias::Task<void>
{
    // TODO: 动态修改配置
    co_return;
}

auto RemoteController::teardown() -> Task<int>
{
    _rpcServer.close();
    co_return 0;
}

auto RemoteController::name() -> const char *
{
    return "RemoteController";
}

auto RemoteController::rpc_server() -> NekoProto::JsonRpcServer<BaseMethods> &
{
    return _rpcServer;
}
MKS_END