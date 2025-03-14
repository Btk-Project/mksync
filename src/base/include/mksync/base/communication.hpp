/**
 * @file communication.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-02-26
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <nekoproto/communication/communication_base.hpp>

#include "mksync/base/base_library.h"

namespace mks::base
{
    // 公共通信接口
    class MKS_BASE_API ICommunication {
    public:
        ICommunication()                                     = default;
        virtual ~ICommunication()                            = default;
        virtual auto declare_proto_to_send(int type) -> void = 0;
    };
    class MKS_BASE_API IServerCommunication : public ICommunication {
    public:
        IServerCommunication()          = default;
        virtual ~IServerCommunication() = default;
        virtual auto send(NekoProto::IProto &event, std::string_view peer)
            -> ilias::IoTask<void>                                                   = 0;
        virtual auto recv(std::string_view peer) -> ilias::IoTask<NekoProto::IProto> = 0;
        virtual auto peers() const -> std::vector<std::string>                       = 0;
    };
    class MKS_BASE_API IClientCommunication : public ICommunication {
    public:
        IClientCommunication()                                             = default;
        virtual ~IClientCommunication()                                    = default;
        virtual auto send(NekoProto::IProto &event) -> ilias::IoTask<void> = 0;
        virtual auto recv() -> ilias::IoTask<NekoProto::IProto>            = 0;
    };
} // namespace mks::base