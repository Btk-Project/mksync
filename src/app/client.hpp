#pragma once

#include "preinclude.hpp"
#include "config/app_config.hpp"
#include "core.hpp"
#include "platform/platform.hpp"
#include <ilias/task.hpp>
#include <ilias/net.hpp>
#include <map>
#include <optional>

MKS_BEGIN

using ilias::IPEndpoint;
using ilias::TcpListener;
using ilias::TcpStream;

class RpcTransport;
struct RpcMessage;

class Client {
public:
    Client(Platform::Ptr platform, IPEndpoint endpoint);
    Client(Platform::Ptr platform, IPEndpoint endpoint, AppConfig config);
    Client(const Client &) = delete;
    ~Client() = default;

    auto run() -> IoTask<void>;

private:
    auto handleWrite(RpcTransport &transport) -> IoTask<void>;
    auto handleRead(RpcTransport &transport, InputInjector &injector) -> IoTask<void>;
    auto handleMessage(const RpcMessage &message, InputInjector &injector) -> IoTask<void>;
    auto shutdownConnection(RpcTransport &transport, InputInjector &injector) -> Task<void>;

    Platform::Ptr mPlatform;
    IPEndpoint mEndpoint;
    AppConfig mConfig;
    std::optional<uint32_t> mLastInjectedMouseScreen;
};

MKS_END
