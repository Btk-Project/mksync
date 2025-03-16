#include "mksync/core/control.hpp"
#include "mksync/proto/proto.hpp"

#include "mksync/core/app.hpp"

namespace mks::base
{
    Control::Control(IApp *app) : _app(app) {}
    Control::~Control() {}

    auto Control::enable() -> ::ilias::Task<int>
    {
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
        return {
            NekoProto::ProtoFactory::protoType<ClientConnected>(),    // 新的客户端连接
            NekoProto::ProtoFactory::protoType<ClientDisconnected>(), // 客户端断开连接
            NekoProto::ProtoFactory::protoType<MouseMotionEvent>(),   // 鼠标事件
            NekoProto::ProtoFactory::protoType<BorderEvent>(),        // 鼠标移动到边界事件
            NekoProto::ProtoFactory::protoType<AppStatusChanged>(),   // 应用状态改变
        };
    }

    ///> 处理一个事件，需要订阅。
    auto Control::handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void>
    {
        if (event.type() == NekoProto::ProtoFactory::protoType<ClientConnected>()) {
            const auto *clientInfo = event.cast<ClientConnected>();
            SPDLOG_INFO("client {} connect...\nscreent name : {}\nscreen id : {}\nscreen size : "
                        "{}x{}\ntimestamp : {}",
                        clientInfo->peer, clientInfo->info.name, clientInfo->info.screenId,
                        clientInfo->info.width, clientInfo->info.height,
                        clientInfo->info.timestamp);
        }
        else if (event.type() == NekoProto::ProtoFactory::protoType<ClientDisconnected>()) {
            const auto *clientInfo = event.cast<ClientDisconnected>();
            SPDLOG_INFO("client {} disconnect...", clientInfo->peer);
            // TODO:
        }
        else if (event.type() == NekoProto::ProtoFactory::protoType<AppStatusChanged>()) {
            const auto *status = event.cast<AppStatusChanged>();
            SPDLOG_INFO("app status changed : {}", (int)status->status);
            if (status->status == AppStatusChanged::eStarted &&
                status->mode == AppStatusChanged::eServer) {
                _app->communication()->declare_proto_to_send({
                    NekoProto::ProtoFactory::protoType<MouseButtonEvent>(),
                    NekoProto::ProtoFactory::protoType<MouseMotionEventConversion>(),
                    NekoProto::ProtoFactory::protoType<MouseWheelEvent>(),
                    NekoProto::ProtoFactory::protoType<KeyboardEvent>(),

                });
            }
        }
        else if (
            event.type() ==
            NekoProto::ProtoFactory::protoType<
                MouseMotionEvent>()) { // 将鼠标位置转换到当前屏幕的位置上。并输出MouseMotionEventConversion事件。
            // TODO: 对鼠标进行处理。
        }
        else if (event.type() == NekoProto::ProtoFactory::protoType<BorderEvent>()) {
            // TODO: 响应鼠标进入边界。
        }
        else {
            SPDLOG_ERROR("unhandle event {}.", event.protoName());
        }
        co_return;
    }

    auto Control::get_event() -> ::ilias::IoTask<NekoProto::IProto>
    {
        co_return NekoProto::IProto{};
    }

    auto Control::make(App &app) -> std::unique_ptr<Control, void (*)(NodeBase *)>
    {
        return std::unique_ptr<Control, void (*)(NodeBase *)>(new Control(&app),
                                                              [](NodeBase *ptr) { delete ptr; });
    }

} // namespace mks::base