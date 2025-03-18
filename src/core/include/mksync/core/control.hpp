/**
 * @file control.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-15
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include "mksync/core/core_library.h"
#include "mksync/base/nodebase.hpp"
#include "mksync/base/node_manager.hpp"
#include "mksync/base/settings.hpp"
#include "mksync/proto/proto.hpp"
#include "mksync/proto/config_proto.hpp"
#include "mksync/base/app.hpp"
#include "mksync/core/communication.hpp"
#include "mksync/base/ring_buffer.hpp"

namespace mks::base
{
    class App;

    struct VirtualScreen {
        int posX;
        int posY;
        int width;
        int height;
    };
    /**
     * @brief 控制节点
     * 负责主要业务逻辑的实现。
     * 对于服务端来说， 负责处理客户端的请求。
     */
    class MKS_CORE_API Control final : public NodeBase, public Consumer, public Producer {
    public:
        Control(IApp *app);
        ~Control();
        [[nodiscard("coroutine function")]]
        auto setup() -> ::ilias::Task<int> override;
        ///> 停用节点。
        [[nodiscard("coroutine function")]]
        auto teardown() -> ::ilias::Task<int> override;
        ///> 获取节点名称。
        auto name() -> const char * override;
        ///> 预先订阅的事件类型集合。
        auto get_subscribes() -> std::vector<int> override;
        ///> 处理一个事件，需要订阅。
        [[nodiscard("coroutine function")]]
        auto handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void> override;
        [[nodiscard("coroutine function")]]
        auto get_event() -> ::ilias::IoTask<NekoProto::IProto> override;

        static auto make(App &app) -> std::unique_ptr<Control, void (*)(NodeBase *)>;

    public:
        ///> 事件处理
        [[nodiscard("coroutine function")]]
        auto handle_event(const ClientConnected &event) -> ::ilias::Task<void>;
        [[nodiscard("coroutine function")]]
        auto handle_event(const ClientDisconnected &event) -> ::ilias::Task<void>;
        [[nodiscard("coroutine function")]]
        auto handle_event(const AppStatusChanged &event) -> ::ilias::Task<void>;
        [[nodiscard("coroutine function")]]
        auto handle_event(const BorderEvent &event) -> ::ilias::Task<void>;
        [[nodiscard("coroutine function")]]
        auto handle_event(const MouseMotionEvent &event) -> ::ilias::Task<void>;

    public:
        ///> 配置屏幕信息
        auto set_virtual_screen_positions(std::string_view srcScreen, std::string_view dstScreen,
                                          int direction) -> void;
        ///> 展示当前配置
        auto show_virtual_screen_positions() -> void;
        ///> 从配置中删除屏幕
        auto remove_virtual_screen(std::string_view screen) -> void;

    private:
        template <typename T>
        auto _register_event_handler() -> void;

    private:
        IApp                                                 *_app = nullptr;
        std::map<std::string, VirtualScreenInfo, std::less<>> _virtualScreens;
        std::string                                           _currentScreen;
        std::string_view                                      _captureNode;
        std::string_view                                      _senderNode;
        std::vector<int>                                      _subscribes;
        RingBuffer<NekoProto::IProto>                         _events;
        ::ilias::Event                                        _syncEvent;
        std::vector<VirtualScreenConfig>                      _vscreenConfig;
        std::map<std::string, std::map<std::string, VirtualScreenInfo, std::less<>>::iterator,
                 std::less<>>
            _screenNameTable;
        std::unordered_map<int, ::ilias::Task<void> (*)(Control *, const NekoProto::IProto &)>
            _eventHandlers;
    };

    template <typename T>
    auto Control::_register_event_handler() -> void
    {
        if (std::find(_subscribes.begin(), _subscribes.end(),
                      NekoProto::ProtoFactory::protoType<T>()) != _subscribes.end()) {
            return;
        }
        _subscribes.push_back(NekoProto::ProtoFactory::protoType<T>());
        _eventHandlers.insert(std::make_pair(
            NekoProto::ProtoFactory::protoType<T>(),
            [](Control *control, const NekoProto::IProto &event) -> ::ilias::Task<void> {
                const auto *ev = event.cast<T>();
                if (ev != nullptr) {
                    co_await control->handle_event(*ev);
                }
                co_return;
            }));
    }
} // namespace mks::base