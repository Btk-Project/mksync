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
#include "mksync/base/app.hpp"

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
        auto enable() -> ::ilias::Task<int> override;
        ///> 停用节点。
        [[nodiscard("coroutine function")]]
        auto disable() -> ::ilias::Task<int> override;
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

    private:
        IApp                                                 *_app = nullptr;
        std::map<std::string, VirtualScreenInfo, std::less<>> _virtualScreens;
        std::string                                           currentScreen;
    };
} // namespace mks::base