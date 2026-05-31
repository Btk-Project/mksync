#pragma once

#include "preinclude.hpp"
#include "core.hpp"
#include <ilias/task.hpp>
#include <ilias/net.hpp>
#include <map>

MKS_BEGIN

using ilias::IPEndpoint;
using ilias::TcpListener;
using ilias::TcpStream;

class Client {
public:
    Client(IPEndpoint endpoint);
    Client(const Client &) = delete;
    ~Client() = default;

    auto run() -> IoTask<void>;
private:
    auto handleWrite(void *platform, void *transport) -> IoTask<void>;
    auto handleRead(void *transport) -> IoTask<void>;

    IPEndpoint mEndpoint;
};

MKS_END