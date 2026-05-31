#pragma once

#include "preinclude.hpp"
#include "refl/formatter.hpp"
#include "core.hpp"
#include <ilias/task.hpp>
#include <ilias/net.hpp>
#include <map>

MKS_BEGIN

using ilias::IPEndpoint;
using ilias::TcpListener;
using ilias::TcpStream;

/**
 * @brief The virtual screen
 * 
 */
struct VirtualScreen {
    // Owner
    IPEndpoint  endpoint;
    bool        local = false;

    // The info
    ScreenInfo  info;
};
FORMATTER(VirtualScreen);

/**
 * @brief The state of client, manage the outcoming queue
 * 
 */
struct ClientState;

/**
 * @brief The server, collection event and dispatch to another client
 * 
 */
class Server {
public:
    explicit Server(IPEndpoint endpoint);
    Server(const Server &) = delete;
    ~Server();

    /**
     * @brief Start the server
     * 
     * @return IoTask<void> 
     */
    auto run() -> IoTask<void>;
private:
    // Background tasks
    auto acceptIncomingConnections(TcpListener listener) -> Task<void>;
    auto waitPlatformEvent(void *_platform, void *capture) -> Task<void>;

    // Handle client
    auto handleIncoming(TcpStream stream) -> IoTask<void>;
    auto handleClientRead(ClientState *state) -> IoTask<void>;
    auto handleClientWrite(ClientState *state) -> IoTask<void>;

    // Screen Manage
    auto addScreen(IPEndpoint endpoint, ScreenInfo info) -> VirtualScreen *;
    auto removeScreen(IPEndpoint endpoint) -> void;

    IPEndpoint  mEndpoint;
    TcpListener mListener;

    // Client connections
    std::multimap<IPEndpoint, VirtualScreen> mScreens; // Mapping (owner) to some virtual screen
    std::map<IPEndpoint, ClientState *>      mClients; // Mapping ip to client state

    // Current active screen, which take the input
    VirtualScreen *mActiveScreen = nullptr;
};

MKS_END