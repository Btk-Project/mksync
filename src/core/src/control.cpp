#include "mksync/core/control.hpp"
#include "mksync/proto/proto.hpp"

#include "mksync/core/app.hpp"
#include "mksync/core/mk_capture.hpp"
#include "mksync/core/mk_sender.hpp"

namespace mks::base
{
    using ::ilias::Error;
    using ::ilias::Task;
    using ::ilias::TaskScope;
    using ::ilias::Unexpected;

    Control::Control(IApp *app) : _app(app), _events(10) {}
    Control::~Control() {}

    auto Control::enable() -> ::ilias::Task<int>
    {
        _register_event_handler<ClientConnected>();
        _register_event_handler<ClientDisconnected>();
        _register_event_handler<MouseMotionEvent>();
        _register_event_handler<BorderEvent>();
        _register_event_handler<AppStatusChanged>();
        co_return 0;
    }

    ///> 停用节点。
    auto Control::disable() -> ::ilias::Task<int>
    {
        co_return 0;
    }

    ///> 获取节点名称。
    auto Control::name() -> const char *
    {
        return "Control";
    }

    ///> 预先订阅的事件类型集合。
    auto Control::get_subscribes() -> std::vector<int>
    {
        return _subscribes;
    }

    ///> 处理一个事件，需要订阅。
    auto Control::handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void>
    {
        if (auto item = _eventHandlers.find(event.type()); item != _eventHandlers.end()) {
            co_return co_await item->second(this, event);
        }
        SPDLOG_ERROR("unhandle event {}!", event.protoName());
        co_return;
    }

    // 新客户端连接
    auto Control::handle_event(const ClientConnected &event) -> ::ilias::Task<void>
    {
        auto item = _onlineClients.insert(event.peer);
        if (item.second) {
            _virtualScreens.insert(std::make_pair(*item.first, event.info));
        }
        else {
            _virtualScreens[*item.first] = event.info;
        }
        SPDLOG_INFO("client {} connect...\nscreent name : {}\nscreen id : {}\nscreen size : "
                    "{}x{}\ntimestamp : {}",
                    event.peer, event.info.name, event.info.screenId, event.info.width,
                    event.info.height, event.info.timestamp);
        co_return;
    }

    // 客户端断开连接
    auto Control::handle_event(const ClientDisconnected &event) -> ::ilias::Task<void>
    {
        _onlineClients.erase(event.peer);
        _virtualScreens.erase(event.peer);
        SPDLOG_INFO("client {} disconnect... [{}]", event.peer, event.reason);
        co_return;
    }

    // App模式状态改变
    auto Control::handle_event(const AppStatusChanged &event) -> ::ilias::Task<void>
    {
        SPDLOG_INFO("app status changed : {}", (int)event.status);
        if (event.status == AppStatusChanged::eStarted && event.mode == AppStatusChanged::eServer) {
            _app->communication()->subscribes({
                NekoProto::ProtoFactory::protoType<MouseButtonEvent>(),
                NekoProto::ProtoFactory::protoType<MouseMotionEventConversion>(),
                NekoProto::ProtoFactory::protoType<MouseWheelEvent>(),
                NekoProto::ProtoFactory::protoType<KeyboardEvent>(),
            });
            // 这两个节点在服务端与客户分别开启即可。因此通过Control导入并加以控制。
            _captureNode = _app->node_manager().add_node(MKCapture::make(_app));
            if (auto ret = co_await _app->node_manager().start_node(_captureNode); ret != 0) {
                SPDLOG_ERROR("start {} node failed.", _captureNode);
            }
        }
        else if (event.status == AppStatusChanged::eStopped &&
                 event.mode == AppStatusChanged::eServer) {
            _app->communication()->unsubscribes({
                NekoProto::ProtoFactory::protoType<MouseButtonEvent>(),
                NekoProto::ProtoFactory::protoType<MouseMotionEventConversion>(),
                NekoProto::ProtoFactory::protoType<MouseWheelEvent>(),
                NekoProto::ProtoFactory::protoType<KeyboardEvent>(),
            });
            if (auto ret = co_await _app->node_manager().stop_node(_captureNode); ret != 0) {
                SPDLOG_ERROR("stop {} node failed.", _captureNode);
            }
            _app->node_manager().destroy_node(_captureNode);
        }
        else if (event.status == AppStatusChanged::eStarted &&
                 event.mode == AppStatusChanged::eClient) {
            _senderNode = _app->node_manager().add_node(MKSender::make(_app));
            if (auto ret = co_await _app->node_manager().start_node(_senderNode); ret != 0) {
                SPDLOG_ERROR("start {} node failed.", _senderNode);
            }
        }
        else if (event.status == AppStatusChanged::eStopped &&
                 event.mode == AppStatusChanged::eClient) {
            if (auto ret = co_await _app->node_manager().stop_node(_senderNode); ret != 0) {
                SPDLOG_ERROR("stop {} node failed.", _senderNode);
            }
            _app->node_manager().destroy_node(_senderNode);
        }
        co_return;
    }

    // 鼠标靠近屏幕边界
    auto Control::handle_event(const BorderEvent &event) -> ::ilias::Task<void>
    {
        // TODO:
        co_return;
    }

    // 鼠标移动
    auto Control::handle_event(const MouseMotionEvent &event) -> ::ilias::Task<void>
    {
        // 将鼠标位置转换到当前屏幕的位置上。并输出MouseMotionEventConversion事件。
        // TODO: 对鼠标进行处理。
        co_return;
    }

    auto Control::get_event() -> ::ilias::IoTask<NekoProto::IProto>
    {
        if (_events.size() == 0) {
            _syncEvent.clear();
            auto ret = co_await _syncEvent;
            if (!ret) {
                co_return Unexpected<Error>(ret.error());
            }
        }
        NekoProto::IProto proto;
        _events.pop(proto);
        co_return std::move(proto);
    }

    auto Control::make(App &app) -> std::unique_ptr<Control, void (*)(NodeBase *)>
    {
        return std::unique_ptr<Control, void (*)(NodeBase *)>(new Control(&app),
                                                              [](NodeBase *ptr) { delete ptr; });
    }
} // namespace mks::base