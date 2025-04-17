/**
 * @file rpc_proto.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-04-08
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once

#include <nekoproto/jsonrpc/jsonrpc.hpp>

#include "mksync/proto/proto_library.h"
#include "mksync/proto/config_proto.hpp"
#include "mksync/proto/command_proto.hpp"

MKS_BEGIN
using NekoProto::JsonRpcServer;
using NekoProto::RpcMethod;

struct BaseMethods {
    RpcMethod<void(std::string), "reloadConfigFile">      reloadConfigFile;
    RpcMethod<std::string(std::string), "executeCommand"> executeCommand;
    RpcMethod<VirtualScreenInfo(), "localScreenInfo">     localScreenInfo;

    // server
    RpcMethod<std::string(ServerControl::Command, std::string, uint16_t), "server"> server;
    RpcMethod<int(), "serverStatus">                                                serverStatus;

    // client
    RpcMethod<std::string(ClientControl::Command, std::string, uint16_t), "client"> client;
    RpcMethod<int(), "clientStatus">                                                clientStatus;

    // virtual screen config
    RpcMethod<void(VirtualScreenConfig), "setVirtualScreenConfig"> setVirtualScreenConfig;
    RpcMethod<void(std::vector<VirtualScreenConfig>), "setVirtualScreenConfigs">
                                                                    setVirtualScreenConfigs;
    RpcMethod<std::vector<VirtualScreenInfo>(), "getOnlineScreens"> getOnlineScreens;
    RpcMethod<void(std::string), "removeVirtualScreen"> removeVirtualScreen;
};
MKS_END