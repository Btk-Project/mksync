#include "mksync/core/controller.hpp"

#include "mksync/core/client_controller.hpp"
#include "mksync/core/mk_sender.hpp"

namespace mks::base
{
    using ::ilias::Error;
    using ::ilias::Task;
    using ::ilias::TaskScope;
    using ::ilias::Unexpected;

    ClientController::ClientController(Controller *self, IApp *app) : ControllerImp(self, app) {}

    ClientController::~ClientController()
    {
        if (!_senderNode.empty()) { // 收个尾
            teardown().wait();
        }
    }

    [[nodiscard("coroutine function")]]
    auto ClientController::setup() -> ::ilias::Task<int>
    {
        _senderNode = _app->node_manager().add_node(MKSender::make(_app));
        if (auto ret = co_await _app->node_manager().setup_node(_senderNode); ret != 0) {
            SPDLOG_ERROR("setup {} node failed.", _senderNode);
        }
        co_return 0;
    }

    ///> 停用节点。
    [[nodiscard("coroutine function")]]
    auto ClientController::teardown() -> ::ilias::Task<int>
    {
        if (_senderNode.empty()) {
            co_return 0;
        }
        if (auto ret = co_await _app->node_manager().teardown_node(_senderNode); ret != 0) {
            SPDLOG_ERROR("teardown {} node failed.", _senderNode);
        }
        _app->node_manager().destroy_node(_senderNode);
        _senderNode = "";
        co_return 0;
    }

    [[nodiscard("coroutine function")]]
    auto ClientController::handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void>
    {
        SPDLOG_INFO("handle event: {}", event.type());
        co_return;
    }
}; // namespace mks::base