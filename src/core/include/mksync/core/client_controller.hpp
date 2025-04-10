/**
 * @file client_controller.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-19
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include "mksync/core/controller.hpp"

MKS_BEGIN
/**
 * @brief 客户端程序控制器
 *
 */
class MKS_CORE_API ClientController final : public ControllerImp {
public:
    ClientController(Controller *self, IApp *app);
    virtual ~ClientController();

    ///> 启动节点。加载MKSender节点并初始化。
    [[nodiscard("coroutine function")]]
    auto setup() -> ::ilias::Task<int> override;
    ///> 停用节点。卸载MKSender节点并清理。
    [[nodiscard("coroutine function")]]
    auto teardown() -> ::ilias::Task<int> override;
    /**
     * @brief 处理事件
     * @note 目前客户端节点没有需要处理的事件
     * @param event
     * @return ::ilias::Task<void>
     */
    [[nodiscard("coroutine function")]]
    auto handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void> override;

private:
    std::string_view _senderNode;
    std::vector<int> _subscribes;
};
MKS_END