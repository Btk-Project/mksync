#pragma once

#include "preinclude.hpp"
#include "config/app_config.hpp"
#include "core.hpp"
#include <ilias/task.hpp>
#include <ilias/net.hpp>
#include <map>
#include <optional>

MKS_BEGIN

using ilias::IPEndpoint;
using ilias::TcpListener;
using ilias::TcpStream;

class InputInjector;
class RpcTransport;
struct RpcMessage;

class Client {
public:
    Client(IPEndpoint endpoint);
    Client(IPEndpoint endpoint, AppConfig config);
    Client(const Client &) = delete;
    ~Client() = default;

    auto run() -> IoTask<void>;

#ifdef MKS_ENABLE_TEST_HOOKS
    auto handleMessageForTest(const RpcMessage &message, InputInjector &injector) -> IoTask<void>;
#endif
private:
    auto handleWrite(void *platform, void *transport) -> IoTask<void>;
    auto handleRead(void *transport, void *injector) -> IoTask<void>;
    auto handleMessage(const RpcMessage &message, InputInjector &injector) -> IoTask<void>;
    auto shutdownConnection(RpcTransport *transport, InputInjector *injector) -> Task<void>;

    IPEndpoint mEndpoint;
    AppConfig mConfig;
    std::optional<uint32_t> mLastInjectedMouseScreen;
};

MKS_END
