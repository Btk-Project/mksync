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

namespace mks::base
{
    class MKS_CORE_API ClientController final : public ControllerImp {
    public:
        ClientController(Controller *self, IApp *app);
        virtual ~ClientController();
        [[nodiscard("coroutine function")]]
        auto setup() -> ::ilias::Task<int> override;
        ///> 停用节点。
        [[nodiscard("coroutine function")]]
        auto teardown() -> ::ilias::Task<int> override;
        [[nodiscard("coroutine function")]]
        auto handle_event(const NekoProto::IProto &event) -> ::ilias::Task<void> override;

    private:
        std::string_view _senderNode;
        std::vector<int> _subscribes;
    };
} // namespace mks::base