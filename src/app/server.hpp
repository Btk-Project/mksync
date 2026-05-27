#pragma once

#include "preinclude.hpp"
#include "core.hpp"
#include <ilias/task.hpp>
#include <ilias/net.hpp>

MKS_BEGIN

using ilias::IPEndpoint;
using ilias::TcpListener;

/**
 * @brief The server, collection event and dispatch to another client
 * 
 */
class Server {
public:
    explicit Server(IPEndpoint endpoint);
    Server(const Server &) = delete;
    ~Server();

    auto run() -> IoTask<void>;
private:
    // Background tasks
    auto acceptIncomingConnections(TcpListener listener) -> Task<void>;
    auto waitPlatformEvent(void *capture) -> Task<void>;

    IPEndpoint  mEndpoint;
    TcpListener mListener;    
};

MKS_END