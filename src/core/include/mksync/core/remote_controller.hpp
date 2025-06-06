/**
 * @file remote_controller.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-03-22
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <ilias/sync/scope.hpp>
#include <nekoproto/jsonrpc/jsonrpc.hpp>

#include "mksync/core/core_library.h"
#include "mksync/base/nodebase.hpp"
#include "mksync/proto/rpc_proto.hpp"

MKS_BEGIN
class IApp;

class MKS_CORE_API RemoteController : public NodeBase {
public:
    RemoteController(IApp *app);
    ~RemoteController();
    [[nodiscard("coroutine function")]]
    auto setup() -> ::ilias::Task<int> override;
    [[nodiscard("coroutine function")]]
    auto teardown() -> ::ilias::Task<int> override;
    auto name() -> const char * override;
    auto reconfigure([[maybe_unused]] Settings &settings) -> ::ilias::Task<void> override;

    auto rpc_server() -> NekoProto::JsonRpcServer<BaseMethods> &;

private:
    IApp                                 *_app;
    NekoProto::JsonRpcServer<BaseMethods> _rpcServer;
    ::ilias::TaskScope                    _taskScop;
};
MKS_END