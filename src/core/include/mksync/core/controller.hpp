/**
 * @file controller.hpp
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
    class Controller;
    class MKS_CORE_API ControllerImp {
    public:
        ControllerImp(Controller *self, IApp *app) : _self(self), _app(app) {}
        virtual ~ControllerImp() = default;
        [[nodiscard("coroutine function")]]
        virtual auto setup() -> ::ilias::Task<int> = 0;
        ///> 停用节点。
        [[nodiscard("coroutine function")]]
        virtual auto teardown() -> ::ilias::Task<int> = 0;
        [[nodiscard("coroutine function")]]
        virtual auto handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void> = 0;

    protected:
        Controller *_self;
        IApp       *_app;
    };

    /**
     * @brief 控制节点
     * 负责主要业务逻辑的实现。
     * 对于服务端来说， 负责处理客户端的请求。
     */
    class MKS_CORE_API Controller final : public NodeBase, public Consumer {
    public:
        Controller(IApp *app);
        ~Controller();
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
        auto handle_event(const AppStatusChanged &event) -> ::ilias::Task<void>;

        static auto make(App &app) -> std::unique_ptr<Controller, void (*)(NodeBase *)>;

    protected:
        IApp                          *_app = nullptr;
        RingBuffer<NekoProto::IProto>  _events;
        ::ilias::Event                 _syncEvent;
        std::unique_ptr<ControllerImp> _imp;
    };
} // namespace mks::base