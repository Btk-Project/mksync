#pragma once

#include "preinclude.hpp"
#include "config/app_config.hpp"
#include "core.hpp"
#include "platform/platform.hpp"
#include "rpc/message.hpp"
#include "server_input.hpp"
#include "server_screens.hpp"
#include "server_types.hpp"
#include <ilias/task.hpp>
#include <ilias/net.hpp>
#include <ilias/sync.hpp>
#include <map>

MKS_BEGIN

using ilias::IPEndpoint;
using ilias::TcpListener;
using ilias::TcpStream;

/**
 * @brief Host process role that accepts clients and captures local input.
 *
 * Collaboration (ownership stays on Server; peers borrow references):
 *
 * - @ref ServerScreenStore  — topology cells, VirtualScreen routes, config layout
 * - @ref ServerInputRouter  — active screen, edge switch, remote InputMessage queue
 * - @ref ServerSession      — one TCP peer: Hello, ScreensMessage, read/write loops
 *
 * @c run() starts accept + capture in parallel. Each accept spawns a
 * ServerSession task under a TaskScope so disconnects are structured.
 *
 * @invariant @c platform is non-null at construction
 */
class Server {
public:
    Server(Platform::Ptr platform, IPEndpoint endpoint);
    Server(Platform::Ptr platform, IPEndpoint endpoint, AppConfig config);
    Server(
        Platform::Ptr platform,
        IPEndpoint endpoint,
        AppConfig config,
        std::filesystem::path configPath
    );
    Server(const Server &) = delete;
    ~Server();

    /**
     * @brief Bind listener, init capture, register local screens, then serve.
     */
    auto run() -> IoTask<void>;

    /** @brief Snapshot of topology screens (for tests / diagnostics). */
    auto topologyScreens() const -> std::vector<TopologyScreen>;

    /** @brief Key of the screen that currently owns keyboard/mouse, if any. */
    auto activeScreenKey() const -> std::optional<ScreenKey>;

private:
    // MARK: Background tasks

    /** @brief Accept loop; each connection is a ServerSession under TaskScope. */
    auto acceptIncomingConnections(TcpListener listener) -> Task<void>;

    /** @brief Drain InputCapture and forward events to the input router. */
    auto waitPlatformEvent(InputCapture &capture) -> Task<void>;

    /**
     * @brief Build session context, run one ServerSession to completion.
     *
     * Spawned from the accept loop; errors are logged inside the session.
     */
    auto handleIncoming(TcpStream stream) -> IoTask<void>;

    // MARK: Screen registration (store + input coordination)

    /**
     * @brief Full screen replace for an endpoint, keeping active-screen safe.
     *
     * Order: remove old screens (clear active if needed) → register new set →
     * ensure a valid local active screen when @p local is true.
     */
    auto registerScreens(
        IPEndpoint endpoint,
        const std::vector<ScreenInfo> &screens,
        bool local
    ) -> void;
    auto registerScreens(
        IPEndpoint endpoint,
        std::string_view ownerId,
        const std::vector<ScreenInfo> &screens,
        bool local
    ) -> void;

    /**
     * @brief Drop topology routes for an endpoint after disconnect or re-report.
     */
    auto removeEndpointScreens(IPEndpoint endpoint) -> void;

    Platform::Ptr mPlatform;
    IPEndpoint mEndpoint;
    ServerScreenStore mScreens;
    // Endpoint → outbound RPC queue used by ServerInputRouter for remote peers.
    ServerInputRouter::ClientSenders mClientSenders;
    ServerInputRouter mInput;
};

MKS_END
